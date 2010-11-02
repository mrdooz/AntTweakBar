//  ---------------------------------------------------------------------------
//
//  @file       TwDirect3D10.cpp
//  @author     Philippe Decaudin - http://www.antisphere.com
//  @license    This file is part of the AntTweakBar library.
//              For conditions of distribution and use, see License.txt
//
//  note:       TAB=4
//
//  ---------------------------------------------------------------------------


#include "TwPrecomp.h"
#include <d3d11.h>
#include <d3dx11.h>
#include <tchar.h>
#include <atlbase.h>
#include "Effects11/Effect.h"

#include "TwDirect3D11.h"
#include "TwMgr.h"
#include "TwColors.h"

//#include "d3d10vs2003.h" // Workaround to include D3D11.h with VS2003

#pragma comment (lib, "d3dcompiler.lib")

using namespace std;

static const char *g_ErrCantLoadD3D11   = "Cannot load Direct3D10 library dynamically";
static const char *g_ErrCompileFX       = "Direct3D10 effect compilation failed";
static const char *g_ErrCreateFX        = "Direct3D10 effect creation failed";
static const char *g_ErrTechNotFound    = "Cannot find Direct3D10 technique effect";
static const char *g_ErrCreateLayout    = "Direct3D10 vertex layout creation failed";
static const char *g_ErrCreateBuffer    = "Direct3D10 vertex buffer creation failed";

//  ---------------------------------------------------------------------------

// Dynamically loaded D3D11 functions (to avoid static linkage with d3d10.lib)
HMODULE g_D3D11Module = NULL;

typedef HRESULT (WINAPI *D3DX11CompileFromMemoryProc)(LPCSTR pSrcData, SIZE_T SrcDataLen, LPCSTR pFileName, const D3D10_SHADER_MACRO *pDefines, LPD3D10INCLUDE pInclude, LPCSTR pFunctionName, LPCSTR pProfile, UINT Flags1, UINT Flags2, ID3DX11ThreadPump *pPump, ID3D10Blob **ppShader, ID3D10Blob **ppErrorMsgs, HRESULT *pHResult);
typedef HRESULT (WINAPI *D3D11CreateEffectFromMemoryProc)(void *pData, SIZE_T DataLength, UINT FXFlags, ID3D11Device *pDevice, ID3D10EffectPool *pEffectPool, ID3DX11Effect **ppEffect);
typedef HRESULT (WINAPI *D3D11StateBlockMaskEnableAllProc)(D3D10_STATE_BLOCK_MASK *pMask);
typedef HRESULT (WINAPI *D3D11CreateStateBlockProc)(ID3D11Device *pDevice, D3D10_STATE_BLOCK_MASK *pStateBlockMask, ID3D10StateBlock **ppStateBlock);
D3DX11CompileFromMemoryProc _D3DX11CompileFromMemoryProc = NULL;
//D3D11StateBlockMaskEnableAllProc _D3D11StateBlockMaskEnableAll = NULL;
//D3D11CreateStateBlockProc _D3D11CreateStateBlock = NULL;

void state_block_enable_all(D3DX11_STATE_BLOCK_MASK *mask)
{
	memset(mask, 1, sizeof(D3DX11_STATE_BLOCK_MASK));
}

static int LoadDirect3D10()
{
    if( g_D3D11Module!=NULL )
        return 1; // Direct3D10 library already loaded

    g_D3D11Module = LoadLibrary("D3DX11_42.DLL");
    if( g_D3D11Module )
    {
        int res = 1;

        _D3DX11CompileFromMemoryProc = reinterpret_cast<D3DX11CompileFromMemoryProc>(GetProcAddress(g_D3D11Module, "D3DX11CompileFromMemory"));
        if( _D3DX11CompileFromMemoryProc==NULL )
            res = 0;
/*
        _D3D11CreateEffectFromMemory = reinterpret_cast<D3D11CreateEffectFromMemoryProc>(GetProcAddress(g_D3D11Module, "D3D11CreateEffectFromMemory"));
        if( _D3D11CreateEffectFromMemory==NULL )
            res = 0;

        _D3D11StateBlockMaskEnableAll = reinterpret_cast<D3D11StateBlockMaskEnableAllProc>(GetProcAddress(g_D3D11Module, "D3D11StateBlockMaskEnableAll"));
        if( _D3D11StateBlockMaskEnableAll==NULL )
            res = 0;
        _D3D11CreateStateBlock = reinterpret_cast<D3D11CreateStateBlockProc>(GetProcAddress(g_D3D11Module, "D3D11CreateStateBlock"));
        if( _D3D11CreateStateBlock==NULL )
            res = 0;
*/
        return res;
    }
    else
        return 0;   // cannot load DLL
}

static int UnloadDirect3D10()
{
    _D3DX11CompileFromMemoryProc = NULL;
    //_D3D11CreateEffectFromMemory =  NULL;
    //_D3D11StateBlockMaskEnableAll = NULL;
    //_D3D11CreateStateBlock = NULL;

    if( g_D3D11Module==NULL )
        return 1; // Direct3D10 library not loaded

    if( FreeLibrary(g_D3D11Module) )
    {
        g_D3D11Module = NULL;
        return 1;
    }
    else
        return 0; // cannot unload d3d10.dll
}

//  ---------------------------------------------------------------------------

static ID3D11ShaderResourceView *BindFont(ID3D11Device *_Dev, ID3DX11EffectShaderResourceVariable *_ResVar, const CTexFont *_Font)
{
    assert(_Font!=NULL);
    assert(_ResVar!=NULL);

    int w = _Font->m_TexWidth;
    int h = _Font->m_TexHeight;
    color32 *font32 = new color32[w*h];
    color32 *p = font32;
    for( int i=0; i<w*h; ++i, ++p )
        *p = 0x00ffffff | (((color32)(_Font->m_TexBytes[i]))<<24);

    D3D11_TEXTURE2D_DESC desc;
    desc.Width = w;
    desc.Height = h;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Usage = D3D11_USAGE_IMMUTABLE;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    desc.CPUAccessFlags = 0;
    desc.MiscFlags = 0;
    D3D11_SUBRESOURCE_DATA data;
    data.pSysMem = font32;
    data.SysMemPitch = w*sizeof(color32);
    data.SysMemSlicePitch = 0;
    ID3D11Texture2D *tex = NULL;
    ID3D11ShaderResourceView *texRV = NULL;

    if( SUCCEEDED(_Dev->CreateTexture2D(&desc, &data, &tex)) )
    {
        if( SUCCEEDED(_Dev->CreateShaderResourceView(tex, NULL, &texRV)) )
            if( _ResVar )
                _ResVar->SetResource(texRV);
        tex->Release();
        tex = NULL;
    }
    
    delete[] font32;
    return texRV;
}

//  ---------------------------------------------------------------------------

static void UnbindFont(ID3D11Device *_Dev, ID3DX11EffectShaderResourceVariable *_ResVar, ID3D11ShaderResourceView *_TexRV)
{
    (void)_Dev;

    if( _ResVar )
        _ResVar->SetResource(NULL);

    if( _TexRV )
    {
        ULONG rc = _TexRV->Release();
        assert( rc==0 ); (void)rc;
    }
}

//  ---------------------------------------------------------------------------

#ifndef _SET_BIT
#define _SET_BIT(bytes, x) (bytes[(x)/8] |= (1 << ((x) % 8)))
#endif
#define _GET_BIT(bytes, x) (bytes[(x)/8] & ~(1 << ((x) % 8)))

template<class T, int N>
struct CComPtrArray
{
	CComPtrArray()
	{
		for (int i = 0; i < N; ++i)
			_arr[i] = NULL;
	}

	~CComPtrArray()
	{
    release();
	}

  void release()
  {
    for (int i = 0; i < N; ++i)
      if (_arr[i]) {
        _arr[i]->Release();
        _arr[i] = NULL;
      }
  }

	T* _arr[N];
};

struct StateBlock
{
	StateBlock(const D3DX11_STATE_BLOCK_MASK& mask, ID3D11DeviceContext *context, ID3D11Device *device)
    : _mask(mask)
    , _context(context) 
		, _device(device)
		, _feature_level(device->GetFeatureLevel())
    , _vertex_shader(mask, 
				&ID3D11DeviceContext::VSGetSamplers, &ID3D11DeviceContext::VSGetShaderResources, &ID3D11DeviceContext::VSGetConstantBuffers, &ID3D11DeviceContext::VSGetShader, 
				&ID3D11DeviceContext::VSSetSamplers, &ID3D11DeviceContext::VSSetShaderResources, &ID3D11DeviceContext::VSSetConstantBuffers, &ID3D11DeviceContext::VSSetShader, 
				mask.VSSamplers, mask.VSShaderResources, mask.VSConstantBuffers, mask.VSInterfaces)
    , _hull_shader(mask, 
				&ID3D11DeviceContext::HSGetSamplers, &ID3D11DeviceContext::HSGetShaderResources, &ID3D11DeviceContext::HSGetConstantBuffers, &ID3D11DeviceContext::HSGetShader, 
				&ID3D11DeviceContext::HSSetSamplers, &ID3D11DeviceContext::HSSetShaderResources, &ID3D11DeviceContext::HSSetConstantBuffers, &ID3D11DeviceContext::HSSetShader, 
				mask.HSSamplers, mask.HSShaderResources, mask.HSConstantBuffers, mask.HSInterfaces)
    , _domain_shader(mask, 
				&ID3D11DeviceContext::DSGetSamplers, &ID3D11DeviceContext::DSGetShaderResources, &ID3D11DeviceContext::DSGetConstantBuffers, &ID3D11DeviceContext::DSGetShader, 
				&ID3D11DeviceContext::DSSetSamplers, &ID3D11DeviceContext::DSSetShaderResources, &ID3D11DeviceContext::DSSetConstantBuffers, &ID3D11DeviceContext::DSSetShader, 
				mask.DSSamplers, mask.DSShaderResources, mask.DSConstantBuffers, mask.DSInterfaces)
    , _geometry_shader(mask, 
				&ID3D11DeviceContext::GSGetSamplers, &ID3D11DeviceContext::GSGetShaderResources, &ID3D11DeviceContext::GSGetConstantBuffers, &ID3D11DeviceContext::GSGetShader, 
				&ID3D11DeviceContext::GSSetSamplers, &ID3D11DeviceContext::GSSetShaderResources, &ID3D11DeviceContext::GSSetConstantBuffers, &ID3D11DeviceContext::GSSetShader, 
				mask.GSSamplers, mask.GSShaderResources, mask.GSConstantBuffers, mask.GSInterfaces)
    , _pixel_shader(mask, 
				&ID3D11DeviceContext::PSGetSamplers, &ID3D11DeviceContext::PSGetShaderResources, &ID3D11DeviceContext::PSGetConstantBuffers, &ID3D11DeviceContext::PSGetShader, 
				&ID3D11DeviceContext::PSSetSamplers, &ID3D11DeviceContext::PSSetShaderResources, &ID3D11DeviceContext::PSSetConstantBuffers, &ID3D11DeviceContext::PSSetShader, 
				mask.PSSamplers, mask.PSShaderResources, mask.PSConstantBuffers, mask.PSInterfaces)
    , _compute_shader(mask, 
				&ID3D11DeviceContext::CSGetSamplers, &ID3D11DeviceContext::CSGetShaderResources, &ID3D11DeviceContext::CSGetConstantBuffers, &ID3D11DeviceContext::CSGetShader, 
				&ID3D11DeviceContext::CSSetSamplers, &ID3D11DeviceContext::CSSetShaderResources, &ID3D11DeviceContext::CSSetConstantBuffers, &ID3D11DeviceContext::CSSetShader, 
				mask.CSSamplers, mask.CSShaderResources, mask.CSConstantBuffers, mask.CSInterfaces)
  {
  }

  // Capture the current states according to the mask
  void capture();

  // Apply the captured state
  void apply();

  // Release all the captured resources
  void release();

  // If I didn't need to be able to debug this, then this bad boy would be a macro :)
  template<class T>
  class ShaderStates
  {
  public:
    typedef void (__stdcall ID3D11DeviceContext::*GetSamplers)(UINT, UINT, ID3D11SamplerState**);
    typedef void (__stdcall ID3D11DeviceContext::*GetShaderResources)(UINT, UINT, ID3D11ShaderResourceView**);
    typedef void (__stdcall ID3D11DeviceContext::*GetConstantBuffers)(UINT, UINT, ID3D11Buffer**);
    typedef void (__stdcall ID3D11DeviceContext::*GetShader)(T **, ID3D11ClassInstance **, UINT *);

		typedef void (__stdcall ID3D11DeviceContext::*SetSamplers)(UINT, UINT, ID3D11SamplerState *const *);
		typedef void (__stdcall ID3D11DeviceContext::*SetShaderResources)(UINT, UINT, ID3D11ShaderResourceView *const *);
		typedef void (__stdcall ID3D11DeviceContext::*SetConstantBuffers)(UINT, UINT, ID3D11Buffer *const *);
		typedef void (__stdcall ID3D11DeviceContext::*SetShader)(T*, ID3D11ClassInstance *const *, UINT);
		
    ShaderStates(const D3DX11_STATE_BLOCK_MASK &mask, 
			GetSamplers fn_get_samplers, GetShaderResources fn_get_shader_resources, GetConstantBuffers fn_get_constant_buffers, GetShader fn_get_shader,
			SetSamplers fn_set_samplers, SetShaderResources fn_set_shader_resources, SetConstantBuffers fn_set_constant_buffers, SetShader fn_set_shader,
      const BYTE *flag_samplers, const BYTE *flag_shader_resources, const BYTE *flag_constant_buffers, const BYTE *flag_interfaces)
      : _mask(mask)
			, _fn_get_samplers(fn_get_samplers), _fn_get_shader_resources(fn_get_shader_resources), _fn_get_constant_buffers(fn_get_constant_buffers), _fn_get_shader(fn_get_shader)
			, _fn_set_samplers(fn_set_samplers), _fn_set_shader_resources(fn_set_shader_resources), _fn_set_constant_buffers(fn_set_constant_buffers), _fn_set_shader(fn_set_shader)
      , _flag_samplers(flag_samplers), _flag_shader_resources(flag_shader_resources), _flag_constant_buffers(flag_constant_buffers), _flag_interfaces(flag_interfaces)
    {
    }

    void save(ID3D11DeviceContext *context)
    {
      for (int i = 0; i < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT; ++i)
        if (_GET_BIT(_flag_samplers, i)) (context->*_fn_get_samplers)(i, 1, &_samplers[i]);

      for (int i = 0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; ++i)
        if (_GET_BIT(_flag_shader_resources, i)) (context->*_fn_get_shader_resources)(i, 1, &_shader_resources[i]);

      for (int i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; ++i)
        if (_GET_BIT(_flag_constant_buffers, i)) (context->*_fn_get_constant_buffers)(i, 1, &_shader_constant_buffers[i]);

      (context->*_fn_get_shader)(&_shader.p, NULL, 0);
    }

    void apply(ID3D11DeviceContext *context)
    {
			for (int i = 0; i < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT; ++i)
				if (_GET_BIT(_flag_samplers, i)) (context->*_fn_set_samplers)(i, 1, &_samplers[i]);

			for (int i = 0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; ++i)
				if (_GET_BIT(_flag_shader_resources, i)) (context->*_fn_set_shader_resources)(i, 1, &_shader_resources[i]);

			for (int i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; ++i)
				if (_GET_BIT(_flag_constant_buffers, i)) (context->*_fn_set_constant_buffers)(i, 1, &_shader_constant_buffers[i]);

			(context->*_fn_set_shader)(_shader.p, NULL, 0);
    }

    void release()
    {
			for (int i = 0; i < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT; ++i)
				_samplers[i] = NULL;

			for (int i = 0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; ++i) 
				_shader_resources[i] = NULL; 

			for (int i = 0; i < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT; ++i) 
				_shader_constant_buffers[i] = NULL; 

			_shader = NULL;
    }

  private:
    const D3DX11_STATE_BLOCK_MASK& _mask;

    GetSamplers _fn_get_samplers;
		SetSamplers _fn_set_samplers;
    GetShaderResources _fn_get_shader_resources;
		SetShaderResources _fn_set_shader_resources;
		GetConstantBuffers _fn_get_constant_buffers;
		SetConstantBuffers _fn_set_constant_buffers;
		GetShader _fn_get_shader;
		SetShader _fn_set_shader;

    const BYTE *_flag_samplers;
    const BYTE *_flag_shader_resources;
    const BYTE *_flag_constant_buffers;
    const BYTE *_flag_interfaces;

    CComPtr<ID3D11SamplerState> _samplers[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];
    CComPtr<ID3D11ShaderResourceView> _shader_resources[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
    CComPtr<ID3D11Buffer> _shader_constant_buffers[D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT];
    CComPtr<T> _shader; 
  };

  ShaderStates<ID3D11VertexShader> _vertex_shader;
  ShaderStates<ID3D11HullShader> _hull_shader;
  ShaderStates<ID3D11DomainShader> _domain_shader;
  ShaderStates<ID3D11GeometryShader> _geometry_shader;
  ShaderStates<ID3D11PixelShader> _pixel_shader;
  ShaderStates<ID3D11ComputeShader> _compute_shader;


  CComPtr<ID3D11Buffer> _vertex_buffers[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	UINT _vertex_buffer_strides[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
	UINT _vertex_buffer_offsets[D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
  CComPtr<ID3D11Buffer> _index_buffer;
	DXGI_FORMAT _index_buffer_format;
	UINT _index_buffer_offset;
  CComPtr<ID3D11InputLayout> _input_layout;
  D3D11_PRIMITIVE_TOPOLOGY _topology;

	CComPtrArray<ID3D11RenderTargetView, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT> _render_targets;
	CComPtr<ID3D11DepthStencilView> _depth_stencil;

	CComPtr<ID3D11DepthStencilState> _depth_stencil_state;
  UINT _depth_stencil_ref;

  CComPtr<ID3D11BlendState> _blend_state;
	FLOAT	_blend_factor[4];
	UINT _sample_mask;

  D3D11_VIEWPORT _viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
	UINT _viewport_count;
  D3D11_RECT _scissor_rects[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
	UINT _scissor_rects_count;
  CComPtr<ID3D11RasterizerState> _rasterizer_state;

  CComPtrArray<ID3D11Buffer, D3D11_SO_BUFFER_SLOT_COUNT> _so_buffers;

  CComPtr<ID3D11Predicate> _predicate;
  BOOL _predicate_value;

	D3DX11_STATE_BLOCK_MASK _mask;
	D3D_FEATURE_LEVEL _feature_level;
	ID3D11DeviceContext *_context;
	ID3D11Device *_device;
};

void StateBlock::capture()
{
	if (_mask.VS) _vertex_shader.save(_context);
	if (_mask.HS) _hull_shader.save(_context);
	if (_mask.DS) _domain_shader.save(_context);
	if (_mask.GS) _geometry_shader.save(_context);
	if (_mask.PS) _pixel_shader.save(_context);
	if (_mask.CS) _compute_shader.save(_context);

	const int slots = _feature_level <= D3D_FEATURE_LEVEL_10_0 ? 16 : D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT;
	for (int i = 0; i < slots; ++i)
		if (_GET_BIT(_mask.IAVertexBuffers, i)) _context->IAGetVertexBuffers(i, 1, &_vertex_buffers[i], &_vertex_buffer_strides[i], &_vertex_buffer_offsets[i]);
	if (_mask.IAIndexBuffer) _context->IAGetIndexBuffer(&_index_buffer.p, &_index_buffer_format, &_index_buffer_offset);
	if (_mask.IAInputLayout) _context->IAGetInputLayout(&_input_layout.p);
	if (_mask.IAPrimitiveTopology) _context->IAGetPrimitiveTopology(&_topology);

	if (_mask.OMRenderTargets) _context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, _render_targets._arr, &_depth_stencil.p);
	if (_mask.OMDepthStencilState) _context->OMGetDepthStencilState(&_depth_stencil_state.p, &_depth_stencil_ref);
	if (_mask.OMBlendState) _context->OMGetBlendState(&_blend_state.p, _blend_factor, &_sample_mask);

	_viewport_count = _mask.RSViewports ? D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE : 0;
	if (_mask.RSViewports) _context->RSGetViewports(&_viewport_count, _viewports);
	_scissor_rects_count = _mask.RSScissorRects ? D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE : 0;
	if (_mask.RSScissorRects) _context->RSGetScissorRects(&_scissor_rects_count, _scissor_rects);
	if (_mask.RSRasterizerState) _context->RSGetState(&_rasterizer_state.p);
	if (_mask.SOBuffers) _context->SOGetTargets(D3D11_SO_BUFFER_SLOT_COUNT, _so_buffers._arr);
	if (_mask.Predication) _context->GetPredication(&_predicate.p, &_predicate_value);
}

void StateBlock::apply()
{
	if (_mask.VS) _vertex_shader.apply(_context);
	if (_mask.HS) _hull_shader.apply(_context);
	if (_mask.DS) _domain_shader.apply(_context);
	if (_mask.GS) _geometry_shader.apply(_context);
	if (_mask.PS) _pixel_shader.apply(_context);
	if (_mask.CS) _compute_shader.apply(_context);

	const int slots = _feature_level <= D3D_FEATURE_LEVEL_10_0 ? 16 : D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT;
	for (int i = 0; i < slots; ++i)
		if (_GET_BIT(_mask.IAVertexBuffers, i)) _context->IASetVertexBuffers(i, 1, &_vertex_buffers[i], &_vertex_buffer_strides[i], &_vertex_buffer_offsets[i]);
	if (_mask.IAIndexBuffer) _context->IASetIndexBuffer(_index_buffer.p, _index_buffer_format, _index_buffer_offset);
	if (_mask.IAInputLayout) _context->IASetInputLayout(_input_layout.p);
	if (_mask.IAPrimitiveTopology) _context->IASetPrimitiveTopology(_topology);

	if (_mask.OMRenderTargets) _context->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, _render_targets._arr, _depth_stencil.p);
	if (_mask.OMDepthStencilState) _context->OMSetDepthStencilState(_depth_stencil_state.p, _depth_stencil_ref);
	if (_mask.OMBlendState) _context->OMSetBlendState(_blend_state.p, _blend_factor, _sample_mask);

	_viewport_count = _mask.RSViewports ? D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE : 0;
	if (_mask.RSViewports) _context->RSSetViewports(_viewport_count, _viewports);
	_scissor_rects_count = _mask.RSScissorRects ? D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE : 0;
	if (_mask.RSScissorRects) _context->RSSetScissorRects(_scissor_rects_count, _scissor_rects);
	if (_mask.RSRasterizerState) _context->RSSetState(_rasterizer_state.p);
	if (_mask.SOBuffers) _context->SOSetTargets(D3D11_SO_BUFFER_SLOT_COUNT, _so_buffers._arr, NULL);
	if (_mask.Predication) _context->SetPredication(_predicate.p, _predicate_value);

	if (_mask.OMBlendState) _context->OMSetBlendState(_blend_state ? _blend_state : NULL, _blend_factor, _sample_mask);
}

void StateBlock::release()
{
  if (_mask.VS) _vertex_shader.release();
  if (_mask.HS) _hull_shader.release();
  if (_mask.DS) _domain_shader.release();
  if (_mask.GS) _geometry_shader.release();
  if (_mask.PS) _pixel_shader.release();
  if (_mask.CS) _compute_shader.release();

  for (int i = 0; i < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT; ++i)
    _vertex_buffers[i] = NULL;
  _index_buffer = NULL;
  _input_layout = NULL;

  _render_targets.release();
  _depth_stencil = NULL;

  _depth_stencil_state = NULL;
  _rasterizer_state = NULL;
  _so_buffers.release();
  _predicate = NULL;

  _blend_state.Release();
}

//  ---------------------------------------------------------------------------

static char g_ShaderFX[] = "// AntTweakBar shaders and techniques \n"
    " float4 g_Offset = 0; float4 g_CstColor = 1; \n"
    " struct LineRectPSInput { float4 Pos : SV_POSITION; float4 Color : COLOR0; }; \n"
    " LineRectPSInput LineRectVS(float4 pos : POSITION, float4 color : COLOR, uniform bool useCstColor) { \n"
    "   LineRectPSInput ps; ps.Pos = pos + g_Offset; \n"
    "   ps.Color = useCstColor ? g_CstColor : color; return ps; } \n"
    " float4 LineRectPS(LineRectPSInput input) : SV_Target { return input.Color; } \n"
    " technique10 LineRect { pass P0 { \n"
    "   SetVertexShader( CompileShader( vs_4_0, LineRectVS(false) ) ); \n"
    "   SetGeometryShader( NULL ); \n"
    "   SetPixelShader( CompileShader( ps_4_0, LineRectPS() ) ); \n"
    " } }\n"
    " technique10 LineRectCstColor { pass P0 { \n"
    "   SetVertexShader( CompileShader( vs_4_0, LineRectVS(true) ) ); \n"
    "   SetGeometryShader( NULL ); \n"
    "   SetPixelShader( CompileShader( ps_4_0, LineRectPS() ) ); \n"
    " } }\n"
    " Texture2D Font; \n"
    " SamplerState FontSampler { Filter = MIN_MAG_MIP_POINT; AddressU = BORDER; AddressV = BORDER; BorderColor=float4(0, 0, 0, 0); }; \n"
    " struct TextPSInput { float4 Pos : SV_POSITION; float4 Color : COLOR0; float2 Tex : TEXCOORD0; }; \n"
    " TextPSInput TextVS(float4 pos : POSITION, float4 color : COLOR, float2 tex : TEXCOORD0, uniform bool useCstColor) { \n"
    "   TextPSInput ps; ps.Pos = pos + g_Offset; \n"
    "   ps.Color = useCstColor ? g_CstColor : color; ps.Tex = tex; return ps; } \n"
    " float4 TextPS(TextPSInput input) : SV_Target { return Font.Sample(FontSampler, input.Tex)*input.Color; } \n"
    " technique10 Text { pass P0 { \n"
    "   SetVertexShader( CompileShader( vs_4_0, TextVS(false) ) ); \n"
    "   SetGeometryShader( NULL ); \n"
    "   SetPixelShader( CompileShader( ps_4_0, TextPS() ) ); \n"
    " } }\n"
    " technique10 TextCstColor { pass P0 { \n"
    "   SetVertexShader( CompileShader( vs_4_0, TextVS(true) ) ); \n"
    "   SetGeometryShader( NULL ); \n"
    "   SetPixelShader( CompileShader( ps_4_0, TextPS() ) ); \n"
    " } }\n"
    " // End of AntTweakBar shaders and techniques \n";

//  ---------------------------------------------------------------------------

int CTwGraphDirect3D11::Init()
{
    assert(g_TwMgr!=NULL);
    assert(g_TwMgr->m_Device!=NULL);

    m_D3DDev = static_cast<ID3D11Device *>(g_TwMgr->m_Device);
    m_D3DContext = static_cast<ID3D11DeviceContext *>(g_TwMgr->m_Context);
		m_FeatureLevel = m_D3DDev->GetFeatureLevel();
    m_D3DDevInitialRefCount = m_D3DDev->AddRef() - 1;

    m_Drawing = false;
    m_OffsetX = m_OffsetY = 0;
    m_ViewportInit = new D3D11_VIEWPORT;
    m_FontTex = NULL;
    m_FontD3DTexRV = NULL;
    m_WndWidth = 0;
    m_WndHeight = 0;
    m_State = NULL;
    m_DepthStencilState = NULL;
    m_BlendState = NULL;
    m_RasterState = NULL;
    m_RasterStateAntialiased = NULL;
    m_RasterStateCullCW = NULL;
    m_RasterStateCullCCW = NULL;
    m_Effect = NULL;
    m_LineRectTech = NULL;
    m_LineRectCstColorTech = NULL;
    m_LineRectVertexLayout = NULL;
    m_LineVertexBuffer = NULL;
    m_RectVertexBuffer = NULL;
    m_TrianglesVertexBuffer = NULL;
    m_TrianglesVertexBufferCount = 0;
    m_TextTech = NULL;
    m_TextCstColorTech = NULL;
    m_TextVertexLayout = NULL;
    m_FontD3DResVar = NULL;
    m_OffsetVar = NULL;
    m_CstColorVar = NULL;

    // Load some D3D11 functions
    if( !LoadDirect3D10() )
    {
        g_TwMgr->SetLastError(g_ErrCantLoadD3D11);
        Shut();
        return 0;
    }

    // Allocate state object
		state_block_enable_all(&m_StateBlockMask);
    m_State = new StateBlock(m_StateBlockMask, m_D3DContext, m_D3DDev);

    // Compile shaders
    // TODO(dooz)
    DWORD shaderFlags = 0; //D3D11_SHADER_ENABLE_STRICTNESS;
    #if defined( DEBUG ) || defined( _DEBUG )
        shaderFlags |= D3D10_SHADER_DEBUG;
    #endif
    ID3D10Blob *compiledFX = NULL;
    ID3D10Blob *errors = NULL;


    // TODO(dooz) can we get away with a lower shader level?
    HRESULT hr = _D3DX11CompileFromMemoryProc(g_ShaderFX, strlen(g_ShaderFX), "AntTweakBarFX", NULL, NULL, NULL, "fx_5_0", shaderFlags, 0, NULL, &compiledFX, &errors, NULL);
    if( FAILED(hr) )
    {
        const size_t ERR_MSG_MAX_LEN = 4096;
        static char s_ErrorMsg[ERR_MSG_MAX_LEN]; // must be static to be sent to SetLastError
        strncpy(s_ErrorMsg, g_ErrCompileFX, ERR_MSG_MAX_LEN-1);
        size_t errOffset = strlen(s_ErrorMsg);
        size_t errLen = 0;
        if( errors!=NULL )
        {
            s_ErrorMsg[errOffset++] = ':';
            s_ErrorMsg[errOffset++] = '\n';
            errLen = min(errors->GetBufferSize(), ERR_MSG_MAX_LEN-errOffset-2);
            strncpy(s_ErrorMsg+errOffset, static_cast<char *>(errors->GetBufferPointer()), errLen);
            errors->Release();
            errors = NULL;
        }
        s_ErrorMsg[errOffset+errLen] = '\0';
        g_TwMgr->SetLastError(s_ErrorMsg);
        Shut();
        return 0;
    }

    hr = D3DX11CreateEffectFromMemory(compiledFX->GetBufferPointer(), compiledFX->GetBufferSize(), 0, m_D3DDev, &m_Effect);
    compiledFX->Release();
    if( FAILED(hr) )
    {
        g_TwMgr->SetLastError(g_ErrCreateFX);
        Shut();
        return 0;
    }

    // Obtain the techniques
    m_LineRectTech = m_Effect->GetTechniqueByName("LineRect");
    m_LineRectCstColorTech = m_Effect->GetTechniqueByName("LineRectCstColor");
    m_TextTech = m_Effect->GetTechniqueByName("Text");
    m_TextCstColorTech = m_Effect->GetTechniqueByName("TextCstColor");
    if( m_LineRectTech==NULL || m_TextTech==NULL || m_LineRectCstColorTech==NULL || m_TextCstColorTech==NULL )
    {
        g_TwMgr->SetLastError(g_ErrTechNotFound);
        Shut();
        return 0;
    }
 
    // Create input layout for lines & rect
    D3D11_INPUT_ELEMENT_DESC lineRectLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },  
        { "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(CLineRectVtx, m_Color), D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    D3DX11_PASS_DESC passDesc;
    hr = m_LineRectTech->GetPassByIndex(0)->GetDesc(&passDesc);
    if( SUCCEEDED(hr) )
        hr = m_D3DDev->CreateInputLayout(lineRectLayout, sizeof(lineRectLayout)/sizeof(lineRectLayout[0]), passDesc.pIAInputSignature, passDesc.IAInputSignatureSize, &m_LineRectVertexLayout);
    if( FAILED(hr) )
    {
        g_TwMgr->SetLastError(g_ErrCreateLayout);
        Shut();
        return 0;
    }

    // Create line vertex buffer
    D3D11_BUFFER_DESC bd;
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.ByteWidth = 2 * sizeof(CLineRectVtx);
    bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    bd.MiscFlags = 0;
    hr = m_D3DDev->CreateBuffer(&bd, NULL, &m_LineVertexBuffer);
    if( FAILED(hr) )
    {
        g_TwMgr->SetLastError(g_ErrCreateBuffer);
        Shut();
        return hr;
    }

    // Create rect vertex buffer
    bd.ByteWidth = 4 * sizeof(CLineRectVtx);
    hr = m_D3DDev->CreateBuffer(&bd, NULL, &m_RectVertexBuffer);
    if( FAILED(hr) )
    {
        g_TwMgr->SetLastError(g_ErrCreateBuffer);
        Shut();
        return hr;
    }

    // Create input layout for text
    D3D11_INPUT_ELEMENT_DESC textLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },  
        { "COLOR", 0, DXGI_FORMAT_R8G8B8A8_UNORM, 0, offsetof(CTextVtx, m_Color), D3D11_INPUT_PER_VERTEX_DATA, 0 }, 
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(CTextVtx, m_UV), D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    hr = m_TextTech->GetPassByIndex(0)->GetDesc(&passDesc);
    if( SUCCEEDED(hr) )
        hr = m_D3DDev->CreateInputLayout(textLayout, sizeof(textLayout)/sizeof(textLayout[0]), passDesc.pIAInputSignature, passDesc.IAInputSignatureSize, &m_TextVertexLayout);
    if( FAILED(hr) )
    {
        g_TwMgr->SetLastError(g_ErrCreateLayout);
        Shut();
        return 0;
    }

    // Create depth stencil state object
    D3D11_DEPTH_STENCILOP_DESC od;
    od.StencilFunc = D3D11_COMPARISON_ALWAYS;
    od.StencilFailOp = D3D11_STENCIL_OP_KEEP;
    od.StencilPassOp = D3D11_STENCIL_OP_KEEP;
    od.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
    D3D11_DEPTH_STENCIL_DESC dsd;
    dsd.DepthEnable = FALSE;
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    dsd.DepthFunc = D3D11_COMPARISON_ALWAYS;
    dsd.StencilEnable = FALSE;
    dsd.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
    dsd.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
    dsd.FrontFace = od;
    dsd.BackFace = od;
    m_D3DDev->CreateDepthStencilState(&dsd, &m_DepthStencilState);

    // Create blend state object
    D3D11_BLEND_DESC bsd;
    bsd.AlphaToCoverageEnable = FALSE;
    for(int i=0; i<8; ++i)
    {
        bsd.RenderTarget[i].BlendEnable = TRUE;
        bsd.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        bsd.RenderTarget[i].SrcBlend = D3D11_BLEND_SRC_ALPHA;
        bsd.RenderTarget[i].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        bsd.RenderTarget[i].BlendOp =  D3D11_BLEND_OP_ADD;
        bsd.RenderTarget[i].SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA;
        bsd.RenderTarget[i].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        bsd.RenderTarget[i].BlendOpAlpha = D3D11_BLEND_OP_ADD;

    }
    m_D3DDev->CreateBlendState(&bsd, &m_BlendState);

    // Create rasterizer state object
    D3D11_RASTERIZER_DESC rd;
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    rd.FrontCounterClockwise = true;
    rd.DepthBias = false;
    rd.DepthBiasClamp = 0;
    rd.SlopeScaledDepthBias = 0;
    rd.DepthClipEnable = false;
    rd.ScissorEnable = true;
    rd.MultisampleEnable = false;
    rd.AntialiasedLineEnable = false;
    m_D3DDev->CreateRasterizerState(&rd, &m_RasterState);

    rd.AntialiasedLineEnable = true;
    m_D3DDev->CreateRasterizerState(&rd, &m_RasterStateAntialiased);
    rd.AntialiasedLineEnable = false;

    rd.CullMode = D3D11_CULL_BACK;
    m_D3DDev->CreateRasterizerState(&rd, &m_RasterStateCullCW);

    rd.CullMode = D3D11_CULL_FRONT;
    m_D3DDev->CreateRasterizerState(&rd, &m_RasterStateCullCCW);

    D3D11_RECT rect = {0, 0, 16000, 16000};
    m_D3DContext->RSSetScissorRects(1, &rect);    
    
    // Get effect globals
    if( m_Effect->GetVariableByName("Font") )
        m_FontD3DResVar = m_Effect->GetVariableByName("Font")->AsShaderResource();
    assert( m_FontD3DResVar!=NULL );
    if( m_Effect->GetVariableByName("g_Offset") )
        m_OffsetVar = m_Effect->GetVariableByName("g_Offset")->AsVector();
    assert( m_OffsetVar!=NULL );
    if( m_Effect->GetVariableByName("g_CstColor") )
        m_CstColorVar = m_Effect->GetVariableByName("g_CstColor")->AsVector();
    assert( m_CstColorVar!=NULL );

    return 1;
}

//  ---------------------------------------------------------------------------

int CTwGraphDirect3D11::Shut()
{
    assert(m_Drawing==false);

    UnbindFont(m_D3DDev, m_FontD3DResVar, m_FontD3DTexRV);
    m_FontD3DTexRV = NULL;
    if( m_State )
    {
        m_State->release();
        delete m_State;
        m_State = NULL;
    }
    if( m_ViewportInit )
    {
        delete m_ViewportInit;
        m_ViewportInit = NULL;
    }

    if( m_DepthStencilState )
    {
        ULONG rc = m_DepthStencilState->Release();
        //assert( rc==0 ); // no assert: the client can use a similar (then shared) state
        (void)rc;
        m_DepthStencilState = NULL;
    }
    if( m_BlendState )
    {
        ULONG rc = m_BlendState->Release();
        //assert( rc==0 ); // no assert: the client can use a similar (then shared) state
        (void)rc;
        m_BlendState = NULL;
    }
    if( m_RasterState )
    {
        ULONG rc = m_RasterState->Release();
        //assert( rc==0 ); // no assert: the client can use a similar (then shared) state
        (void)rc;
        m_RasterState = NULL;
    }
    if( m_RasterStateAntialiased )
    {
        ULONG rc = m_RasterStateAntialiased->Release();
        //assert( rc==0 ); // no assert: the client can use a similar (then shared) state
        (void)rc;
        m_RasterStateAntialiased = NULL;
    }
    if( m_RasterStateCullCW )
    {
        ULONG rc = m_RasterStateCullCW->Release();
        //assert( rc==0 ); // no assert: the client can use a similar (then shared) state
        (void)rc;
        m_RasterStateCullCW = NULL;
    }
    if( m_RasterStateCullCCW )
    {
        ULONG rc = m_RasterStateCullCCW->Release();
        //assert( rc==0 ); // no assert: the client can use a similar (then shared) state
        (void)rc;
        m_RasterStateCullCCW = NULL;
    }

    m_FontD3DResVar = NULL;
    m_OffsetVar = NULL;
    m_CstColorVar = NULL;

    if( m_LineVertexBuffer )
    {
        ULONG rc = m_LineVertexBuffer->Release();
        assert( rc==0 ); (void)rc;
        m_LineVertexBuffer = NULL;
    }
    if( m_RectVertexBuffer )
    {
        ULONG rc = m_RectVertexBuffer->Release();
        assert( rc==0 ); (void)rc;
        m_RectVertexBuffer = NULL;
    }
    if( m_TrianglesVertexBuffer )
    {
        ULONG rc = m_TrianglesVertexBuffer->Release();
        assert( rc==0 ); (void)rc;
        m_TrianglesVertexBuffer = NULL;
        m_TrianglesVertexBufferCount = 0;
    }
    if( m_LineRectVertexLayout ) 
    {
        ULONG rc = m_LineRectVertexLayout->Release();
        assert( rc==0 ); (void)rc;
        m_LineRectVertexLayout = NULL;
    }
    if( m_TextVertexLayout ) 
    {
        ULONG rc = m_TextVertexLayout->Release();
        assert( rc==0 ); (void)rc;
        m_TextVertexLayout = NULL;
    }
    if( m_Effect )
    {
        ULONG rc = m_Effect->Release();
        assert( rc==0 ); (void)rc;
        m_Effect = NULL;
    }

    if( m_D3DDev )
    {
        //unsigned int rc = m_D3DDev->Release();
        //assert( m_D3DDevInitialRefCount==rc ); (void)rc;
        m_D3DDev->Release();
        m_D3DDev = NULL;
    }

    // Unload D3D11
    UnloadDirect3D10(); // this is not a problem if it cannot be unloaded

    return 1;
}

//  ---------------------------------------------------------------------------

void CTwGraphDirect3D11::BeginDraw(int _WndWidth, int _WndHeight)
{
    assert(m_Drawing==false && _WndWidth>0 && _WndHeight>0);
    m_Drawing = true;

    m_WndWidth  = _WndWidth;
    m_WndHeight = _WndHeight;
    m_OffsetX = m_OffsetY = 0;

    // save context
    m_State->capture();

    // Setup the viewport
    D3D11_VIEWPORT vp;
    vp.Width = (FLOAT)_WndWidth;
    vp.Height = (FLOAT)_WndHeight;
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    m_D3DContext->RSSetViewports(1, &vp);
    *static_cast<D3D11_VIEWPORT *>(m_ViewportInit) = vp;

    m_D3DContext->RSSetState(m_RasterState);

    m_D3DContext->OMSetDepthStencilState(m_DepthStencilState, 0);
    float blendFactors[4] = { 1, 1, 1, 1 };
    m_D3DContext->OMSetBlendState(m_BlendState, blendFactors, 0xffffffff);
}

//  ---------------------------------------------------------------------------

void CTwGraphDirect3D11::EndDraw()
{
    m_D3DContext->RSSetState(NULL);
    m_D3DContext->OMSetDepthStencilState(NULL, 0);
    m_D3DContext->OMSetBlendState(NULL, NULL, 0xffffffff);

    assert(m_Drawing==true);
    m_Drawing = false;

    // restore context
    m_State->apply();
    // apply doesn't release any resources, so we must do that explicitly
    m_State->release();
}

//  ---------------------------------------------------------------------------

bool CTwGraphDirect3D11::IsDrawing()
{
    return m_Drawing;
}

//  ---------------------------------------------------------------------------

void CTwGraphDirect3D11::Restore()
{
  if (m_State)
    m_State->release();
/*
    if( m_State )
    {
        if( m_State->m_StateBlock )
        {
            UINT rc = m_State->m_StateBlock->Release();
            assert( rc==0 ); (void)rc;
            m_State->m_StateBlock = NULL;
        }
    }
*/
    UnbindFont(m_D3DDev, m_FontD3DResVar, m_FontD3DTexRV);
    m_FontD3DTexRV = NULL;
    
    m_FontTex = NULL;
}


//  ---------------------------------------------------------------------------

static inline float ToNormScreenX(int x, int wndWidth)
{
    return 2.0f*((float)x-0.5f)/wndWidth - 1.0f;
}

static inline float ToNormScreenY(int y, int wndHeight)
{
    return 1.0f - 2.0f*((float)y-0.5f)/wndHeight;
}

static inline color32 ToR8G8B8A8(color32 col)
{
    return (col & 0xff00ff00) | ((col>>16) & 0xff) | ((col<<16) & 0xff0000);
}

//  ---------------------------------------------------------------------------

void CTwGraphDirect3D11::DrawLine(int _X0, int _Y0, int _X1, int _Y1, color32 _Color0, color32 _Color1, bool _AntiAliased)
{
    assert(m_Drawing==true);

    float x0 = ToNormScreenX(_X0 + m_OffsetX, m_WndWidth);
    float y0 = ToNormScreenY(_Y0 + m_OffsetY, m_WndHeight);
    float x1 = ToNormScreenX(_X1 + m_OffsetX, m_WndWidth);
    float y1 = ToNormScreenY(_Y1 + m_OffsetY, m_WndHeight);
 
    D3D11_MAPPED_SUBRESOURCE r;
    HRESULT hr = m_D3DContext->Map(m_LineVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &r);
    if( SUCCEEDED(hr) )
    {
        CLineRectVtx *vertices = (CLineRectVtx *)r.pData;
        // Fill vertex buffer
        vertices[0].m_Pos[0] = x0;
        vertices[0].m_Pos[1] = y0;
        vertices[0].m_Pos[2] = 0;
        vertices[0].m_Color = ToR8G8B8A8(_Color0);
        vertices[1].m_Pos[0] = x1;
        vertices[1].m_Pos[1] = y1;
        vertices[1].m_Pos[2] = 0;
        vertices[1].m_Color = ToR8G8B8A8(_Color1);

        m_D3DContext->Unmap(m_LineVertexBuffer, 0);

        if( _AntiAliased )
            m_D3DContext->RSSetState(m_RasterStateAntialiased);

        // Reset shader globals
        float offsetVec[4] = { 0, 0, 0, 0 };
        if( m_OffsetVar )
            m_OffsetVar->SetFloatVector(offsetVec);
        float colorVec[4] = { 1, 1, 1, 1 };
        if( m_CstColorVar )
            m_CstColorVar->SetFloatVector(colorVec);

        // Set the input layout
        m_D3DContext->IASetInputLayout(m_LineRectVertexLayout);

        // Set vertex buffer
        UINT stride = sizeof(CLineRectVtx);
        UINT offset = 0;
        m_D3DContext->IASetVertexBuffers(0, 1, &m_LineVertexBuffer, &stride, &offset);

        // Set primitive topology
        m_D3DContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

        // Render the line
        D3DX11_TECHNIQUE_DESC techDesc;
        m_LineRectTech->GetDesc(&techDesc);
        for(UINT p=0; p<techDesc.Passes; ++p)
        {
            m_LineRectTech->GetPassByIndex(p)->Apply(0, m_D3DContext);
            m_D3DContext->Draw(2, 0);
        }

        if( _AntiAliased )
            m_D3DContext->RSSetState(m_RasterState); // restore default raster state
    }
}

//  ---------------------------------------------------------------------------

void CTwGraphDirect3D11::DrawRect(int _X0, int _Y0, int _X1, int _Y1, color32 _Color00, color32 _Color10, color32 _Color01, color32 _Color11)
{
    assert(m_Drawing==true);

    // border adjustment
    if(_X0<_X1)
        ++_X1;
    else if(_X0>_X1)
        ++_X0;
    if(_Y0<_Y1)
        ++_Y1;
    else if(_Y0>_Y1)
        ++_Y0;

    float x0 = ToNormScreenX(_X0 + m_OffsetX, m_WndWidth);
    float y0 = ToNormScreenY(_Y0 + m_OffsetY, m_WndHeight);
    float x1 = ToNormScreenX(_X1 + m_OffsetX, m_WndWidth);
    float y1 = ToNormScreenY(_Y1 + m_OffsetY, m_WndHeight);
 
    D3D11_MAPPED_SUBRESOURCE r;
    HRESULT hr = m_D3DContext->Map(m_RectVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &r);
    if( SUCCEEDED(hr) )
    {
        CLineRectVtx *vertices = (CLineRectVtx *)r.pData;

        // Fill vertex buffer
        vertices[0].m_Pos[0] = x0;
        vertices[0].m_Pos[1] = y0;
        vertices[0].m_Pos[2] = 0;
        vertices[0].m_Color = ToR8G8B8A8(_Color00);
        vertices[1].m_Pos[0] = x1;
        vertices[1].m_Pos[1] = y0;
        vertices[1].m_Pos[2] = 0;
        vertices[1].m_Color = ToR8G8B8A8(_Color10);
        vertices[2].m_Pos[0] = x0;
        vertices[2].m_Pos[1] = y1;
        vertices[2].m_Pos[2] = 0;
        vertices[2].m_Color = ToR8G8B8A8(_Color01);
        vertices[3].m_Pos[0] = x1;
        vertices[3].m_Pos[1] = y1;
        vertices[3].m_Pos[2] = 0;
        vertices[3].m_Color = ToR8G8B8A8(_Color11);

        m_D3DContext->Unmap(m_RectVertexBuffer, 0);

        // Reset shader globals
        float offsetVec[4] = { 0, 0, 0, 0 };
        if( m_OffsetVar )
            m_OffsetVar->SetFloatVector(offsetVec);
        float colorVec[4] = { 1, 1, 1, 1 };
        if( m_CstColorVar )
            m_CstColorVar->SetFloatVector(colorVec);

        // Set the input layout
        m_D3DContext->IASetInputLayout(m_LineRectVertexLayout);

        // Set vertex buffer
        UINT stride = sizeof(CLineRectVtx);
        UINT offset = 0;
        m_D3DContext->IASetVertexBuffers(0, 1, &m_RectVertexBuffer, &stride, &offset);

        // Set primitive topology
        m_D3DContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

        // Render the rect
        D3DX11_TECHNIQUE_DESC techDesc;
        m_LineRectTech->GetDesc(&techDesc);
        for(UINT p=0; p<techDesc.Passes; ++p)
        {
            m_LineRectTech->GetPassByIndex(p)->Apply(0, m_D3DContext);
            m_D3DContext->Draw(4, 0);
        }
    }
}

//  ---------------------------------------------------------------------------

void *CTwGraphDirect3D11::NewTextObj()
{
    CTextObj *textObj = new CTextObj;
    memset(textObj, 0, sizeof(CTextObj));
    return textObj;
}

//  ---------------------------------------------------------------------------

void CTwGraphDirect3D11::DeleteTextObj(void *_TextObj)
{
    assert(_TextObj!=NULL);
    CTextObj *textObj = static_cast<CTextObj *>(_TextObj);
    if( textObj->m_TextVertexBuffer )
        textObj->m_TextVertexBuffer->Release();
    if( textObj->m_BgVertexBuffer )
        textObj->m_BgVertexBuffer->Release();
    memset(textObj, 0, sizeof(CTextObj));
    delete textObj;
}

//  ---------------------------------------------------------------------------

void CTwGraphDirect3D11::BuildText(void *_TextObj, const std::string *_TextLines, color32 *_LineColors, color32 *_LineBgColors, int _NbLines, const CTexFont *_Font, int _Sep, int _BgWidth)
{
    assert(m_Drawing==true);
    assert(_TextObj!=NULL);
    assert(_Font!=NULL);

    if( _Font != m_FontTex )
    {
        UnbindFont(m_D3DDev, m_FontD3DResVar, m_FontD3DTexRV);
        m_FontD3DTexRV = BindFont(m_D3DDev, m_FontD3DResVar, _Font);
        m_FontTex = _Font;
    }

    int nbTextVerts = 0;
    int line;
    for( line=0; line<_NbLines; ++line )
        nbTextVerts += 6 * (int)_TextLines[line].length();
    int nbBgVerts = 0;
    if( _BgWidth>0 )
        nbBgVerts = _NbLines*6;

    CTextObj *textObj = static_cast<CTextObj *>(_TextObj);
    textObj->m_LineColors = (_LineColors!=NULL);
    textObj->m_LineBgColors = (_LineBgColors!=NULL);

    // (re)create text vertex buffer if needed, and map it
    CTextVtx *textVerts = NULL;
    if( nbTextVerts>0 )
    {
        if( textObj->m_TextVertexBuffer==NULL || textObj->m_TextVertexBufferSize<nbTextVerts )
        {
            if( textObj->m_TextVertexBuffer!=NULL )
            {
                ULONG rc = textObj->m_TextVertexBuffer->Release();
                assert( rc==0 ); (void)rc;
                textObj->m_TextVertexBuffer = NULL;
            }
            textObj->m_TextVertexBufferSize = nbTextVerts + 6*256; // add a reserve of 256 characters
            D3D11_BUFFER_DESC bd;
            bd.Usage = D3D11_USAGE_DYNAMIC;
            bd.ByteWidth = textObj->m_TextVertexBufferSize * sizeof(CTextVtx);
            bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            bd.MiscFlags = 0;
            m_D3DDev->CreateBuffer(&bd, NULL, &textObj->m_TextVertexBuffer);
        }

        if( textObj->m_TextVertexBuffer!=NULL ) {
          D3D11_MAPPED_SUBRESOURCE r;
          m_D3DContext->Map(textObj->m_TextVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &r);
          textVerts = (CTextVtx *)r.pData;
        }
    }

    // (re)create bg vertex buffer if needed, and map it
    CLineRectVtx *bgVerts = NULL;
    if( nbBgVerts>0 )
    {
        if( textObj->m_BgVertexBuffer==NULL || textObj->m_BgVertexBufferSize<nbBgVerts )
        {
            if( textObj->m_BgVertexBuffer!=NULL )
            {
                ULONG rc = textObj->m_BgVertexBuffer->Release();
                assert( rc==0 ); (void)rc;
                textObj->m_BgVertexBuffer = NULL;
            }
            textObj->m_BgVertexBufferSize = nbBgVerts + 6*32; // add a reserve of 32 rects
            D3D11_BUFFER_DESC bd;
            bd.Usage = D3D11_USAGE_DYNAMIC;
            bd.ByteWidth = textObj->m_BgVertexBufferSize * sizeof(CLineRectVtx);
            bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
            bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
            bd.MiscFlags = 0;
            m_D3DDev->CreateBuffer(&bd, NULL, &textObj->m_BgVertexBuffer);
        }

        if( textObj->m_BgVertexBuffer!=NULL ) {
          D3D11_MAPPED_SUBRESOURCE r;
          m_D3DContext->Map(textObj->m_BgVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &r);
          bgVerts = (CLineRectVtx *)r.pData;
        }
    }

    int x, x1, y, y1, i, len;
    float px, px1, py, py1;
    unsigned char ch;
    const unsigned char *text;
    color32 lineColor = COLOR32_RED;
    CTextVtx vtx;
    vtx.m_Pos[2] = 0;
    CLineRectVtx bgVtx;
    bgVtx.m_Pos[2] = 0;
    int textVtxIndex = 0;
    int bgVtxIndex = 0;
    for( line=0; line<_NbLines; ++line )
    {
        x = 0;
        y = line * (_Font->m_CharHeight+_Sep);
        y1 = y+_Font->m_CharHeight;
        len = (int)_TextLines[line].length();
        text = (const unsigned char *)(_TextLines[line].c_str());
        if( _LineColors!=NULL )
            lineColor = ToR8G8B8A8(_LineColors[line]);

        if( textVerts!=NULL )
            for( i=0; i<len; ++i )
            {
                ch = text[i];
                x1 = x + _Font->m_CharWidth[ch];

                px  = ToNormScreenX(x,  m_WndWidth);
                py  = ToNormScreenY(y,  m_WndHeight);
                px1 = ToNormScreenX(x1, m_WndWidth);
                py1 = ToNormScreenY(y1, m_WndHeight);

                vtx.m_Color  = lineColor;

                vtx.m_Pos[0] = px;
                vtx.m_Pos[1] = py;
                vtx.m_UV [0] = _Font->m_CharU0[ch];
                vtx.m_UV [1] = _Font->m_CharV0[ch];
                textVerts[textVtxIndex++] = vtx;

                vtx.m_Pos[0] = px1;
                vtx.m_Pos[1] = py;
                vtx.m_UV [0] = _Font->m_CharU1[ch];
                vtx.m_UV [1] = _Font->m_CharV0[ch];
                textVerts[textVtxIndex++] = vtx;

                vtx.m_Pos[0] = px;
                vtx.m_Pos[1] = py1;
                vtx.m_UV [0] = _Font->m_CharU0[ch];
                vtx.m_UV [1] = _Font->m_CharV1[ch];
                textVerts[textVtxIndex++] = vtx;

                vtx.m_Pos[0] = px1;
                vtx.m_Pos[1] = py;
                vtx.m_UV [0] = _Font->m_CharU1[ch];
                vtx.m_UV [1] = _Font->m_CharV0[ch];
                textVerts[textVtxIndex++] = vtx;

                vtx.m_Pos[0] = px1;
                vtx.m_Pos[1] = py1;
                vtx.m_UV [0] = _Font->m_CharU1[ch];
                vtx.m_UV [1] = _Font->m_CharV1[ch];
                textVerts[textVtxIndex++] = vtx;

                vtx.m_Pos[0] = px;
                vtx.m_Pos[1] = py1;
                vtx.m_UV [0] = _Font->m_CharU0[ch];
                vtx.m_UV [1] = _Font->m_CharV1[ch];
                textVerts[textVtxIndex++] = vtx;

                x = x1;
            }

        if( _BgWidth>0 && bgVerts!=NULL )
        {
            if( _LineBgColors!=NULL )
                bgVtx.m_Color = ToR8G8B8A8(_LineBgColors[line]);
            else
                bgVtx.m_Color = ToR8G8B8A8(COLOR32_BLACK);

            px  = ToNormScreenX(-1, m_WndWidth);
            py  = ToNormScreenY(y,  m_WndHeight);
            px1 = ToNormScreenX(_BgWidth+1, m_WndWidth);
            py1 = ToNormScreenY(y1, m_WndHeight);

            bgVtx.m_Pos[0] = px;
            bgVtx.m_Pos[1] = py;
            bgVerts[bgVtxIndex++] = bgVtx;

            bgVtx.m_Pos[0] = px1;
            bgVtx.m_Pos[1] = py;
            bgVerts[bgVtxIndex++] = bgVtx;

            bgVtx.m_Pos[0] = px;
            bgVtx.m_Pos[1] = py1;
            bgVerts[bgVtxIndex++] = bgVtx;

            bgVtx.m_Pos[0] = px1;
            bgVtx.m_Pos[1] = py;
            bgVerts[bgVtxIndex++] = bgVtx;

            bgVtx.m_Pos[0] = px1;
            bgVtx.m_Pos[1] = py1;
            bgVerts[bgVtxIndex++] = bgVtx;

            bgVtx.m_Pos[0] = px;
            bgVtx.m_Pos[1] = py1;
            bgVerts[bgVtxIndex++] = bgVtx;
        }
    }
    assert( textVtxIndex==nbTextVerts );
    assert( bgVtxIndex==nbBgVerts );
    textObj->m_NbTextVerts = nbTextVerts;
    textObj->m_NbBgVerts = nbBgVerts;

    if( textVerts!=NULL )
        m_D3DContext->Unmap(textObj->m_TextVertexBuffer, 0);
    if( bgVerts!=NULL )
        m_D3DContext->Unmap(textObj->m_BgVertexBuffer, 0);
}

//  ---------------------------------------------------------------------------

void CTwGraphDirect3D11::DrawText(void *_TextObj, int _X, int _Y, color32 _Color, color32 _BgColor)
{
    assert(m_Drawing==true);
    assert(_TextObj!=NULL);
    CTextObj *textObj = static_cast<CTextObj *>(_TextObj);
    float dx = 2.0f*(float)(_X + m_OffsetX)/m_WndWidth;
    float dy = -2.0f*(float)(_Y + m_OffsetY)/m_WndHeight;
 
    float offsetVec[4] = { 0, 0, 0, 0 };
    offsetVec[0] = dx;
    offsetVec[1] = dy;
    if( m_OffsetVar )
        m_OffsetVar->SetFloatVector(offsetVec);

    // Draw background
    if( textObj->m_NbBgVerts>=4 && textObj->m_BgVertexBuffer!=NULL )
    {
        float color[4];
        Color32ToARGBf(_BgColor, color+3, color+0, color+1, color+2);
        if( m_CstColorVar )
            m_CstColorVar->SetFloatVector(color);

        // Set the input layout
        m_D3DContext->IASetInputLayout(m_LineRectVertexLayout);

        // Set vertex buffer
        UINT stride = sizeof(CLineRectVtx);
        UINT offset = 0;
        m_D3DContext->IASetVertexBuffers(0, 1, &textObj->m_BgVertexBuffer, &stride, &offset);

        // Set primitive topology
        m_D3DContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // Render the bg rectangles
        ID3DX11EffectTechnique *tech;
        if( _BgColor!=0 || !textObj->m_LineBgColors ) // use a constant bg color
            tech = m_LineRectCstColorTech;
        else // use vertex buffer colors
            tech = m_LineRectTech;
        D3DX11_TECHNIQUE_DESC techDesc;
        tech->GetDesc(&techDesc);
        for( UINT p=0; p<techDesc.Passes; ++p )
        {
            tech->GetPassByIndex(p)->Apply(0, m_D3DContext);
            m_D3DContext->Draw(textObj->m_NbBgVerts, 0);
        }
    }

    // Draw text
    if( textObj->m_NbTextVerts>=4 && textObj->m_TextVertexBuffer!=NULL )
    {
        float color[4];
        Color32ToARGBf(_Color, color+3, color+0, color+1, color+2);
        if( m_CstColorVar )
            m_CstColorVar->SetFloatVector(color);

        // Set the input layout
        m_D3DContext->IASetInputLayout(m_TextVertexLayout);

        // Set vertex buffer
        UINT stride = sizeof(CTextVtx);
        UINT offset = 0;
        m_D3DContext->IASetVertexBuffers(0, 1, &textObj->m_TextVertexBuffer, &stride, &offset);

        // Set primitive topology
        m_D3DContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // Render the bg rectangles
        ID3DX11EffectTechnique *tech;
        if( _Color!=0 || !textObj->m_LineColors ) // use a constant color
            tech = m_TextCstColorTech;
        else // use vertex buffer colors
            tech = m_TextTech;
        D3DX11_TECHNIQUE_DESC techDesc;
        tech->GetDesc(&techDesc);
        for( UINT p=0; p<techDesc.Passes; ++p )
        {
            tech->GetPassByIndex(p)->Apply(0, m_D3DContext);
            m_D3DContext->Draw(textObj->m_NbTextVerts, 0);
        }
    }
}

//  ---------------------------------------------------------------------------

void CTwGraphDirect3D11::ChangeViewport(int _X0, int _Y0, int _Width, int _Height, int _OffsetX, int _OffsetY)
{
    if( _Width>0 && _Height>0 )
    {
	    /* viewport changes screen coordinates, use scissor instead
        D3D11_VIEWPORT vp;
        vp.TopLeftX = _X0;
        vp.TopLeftY = _Y0;
        vp.Width = _Width;
        vp.Height = _Height;
        vp.MinDepth = 0;
        vp.MaxDepth = 1;
        m_D3DDev->RSSetViewports(1, &vp);
        */
        
        D3D11_RECT rect;
        rect.left = _X0;
        rect.right = _X0 + _Width - 1;
        rect.top = _Y0;
        rect.bottom = _Y0 + _Height - 1;
        m_D3DContext->RSSetScissorRects(1, &rect);

        m_OffsetX = _X0 + _OffsetX;
        m_OffsetY = _Y0 + _OffsetY;
    }
}

//  ---------------------------------------------------------------------------

void CTwGraphDirect3D11::RestoreViewport()
{
    //m_D3DDev->RSSetViewports(1, static_cast<D3D11_VIEWPORT *>(m_ViewportInit));
    D3D11_RECT rect = {0, 0, 16000, 16000};
    m_D3DContext->RSSetScissorRects(1, &rect);
        
    m_OffsetX = m_OffsetY = 0;
}

//  ---------------------------------------------------------------------------

void CTwGraphDirect3D11::DrawTriangles(int _NumTriangles, int *_Vertices, color32 *_Colors, Cull _CullMode)
{
    assert(m_Drawing==true);

    if( _NumTriangles<=0 )
        return;

    if( m_TrianglesVertexBufferCount<3*_NumTriangles ) // force re-creation
    {
	    if( m_TrianglesVertexBuffer!=NULL )
	        m_TrianglesVertexBuffer->Release();
        m_TrianglesVertexBuffer = NULL;
        m_TrianglesVertexBufferCount = 0;
    }

    // DrawTriangles uses LineRect layout and technique

    if( m_TrianglesVertexBuffer==NULL )
    {
        // Create triangles vertex buffer
        D3D11_BUFFER_DESC bd;
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        bd.MiscFlags = 0;
        bd.ByteWidth = 3*_NumTriangles * sizeof(CLineRectVtx);
        HRESULT hr = m_D3DDev->CreateBuffer(&bd, NULL, &m_TrianglesVertexBuffer);
        if( SUCCEEDED(hr) )
            m_TrianglesVertexBufferCount = 3*_NumTriangles;
        else
        {
            m_TrianglesVertexBuffer = NULL;
            m_TrianglesVertexBufferCount = 0;
            return; // Problem: cannot create triangles VB
        }
    }
    assert( m_TrianglesVertexBufferCount>=3*_NumTriangles );
    assert( m_TrianglesVertexBuffer!=NULL );

    CLineRectVtx *vertices = NULL;
    D3D11_MAPPED_SUBRESOURCE r;
    HRESULT hr = m_D3DContext->Map(m_TrianglesVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &r);
    if( SUCCEEDED(hr) )
    {
        vertices = (CLineRectVtx *)r.pData;
        // Fill vertex buffer
        for( int i=0; i<3*_NumTriangles; ++ i )
        {
            vertices[i].m_Pos[0] = ToNormScreenX(_Vertices[2*i+0] + m_OffsetX, m_WndWidth);
            vertices[i].m_Pos[1] = ToNormScreenY(_Vertices[2*i+1] + m_OffsetY, m_WndHeight);
            vertices[i].m_Pos[2] = 0;
            vertices[i].m_Color = ToR8G8B8A8(_Colors[i]);
        }
        m_D3DContext->Unmap(m_TrianglesVertexBuffer, 0);

        // Reset shader globals
        float offsetVec[4] = { 0, 0, 0, 0 };
        if( m_OffsetVar )
            m_OffsetVar->SetFloatVector(offsetVec);
        float colorVec[4] = { 1, 1, 1, 1 };
        if( m_CstColorVar )
            m_CstColorVar->SetFloatVector(colorVec);

        // Set the input layout
        m_D3DContext->IASetInputLayout(m_LineRectVertexLayout);

        // Set vertex buffer
        UINT stride = sizeof(CLineRectVtx);
        UINT offset = 0;
        m_D3DContext->IASetVertexBuffers(0, 1, &m_TrianglesVertexBuffer, &stride, &offset);

        // Set primitive topology
        m_D3DContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        if( _CullMode==CULL_CW )
            m_D3DContext->RSSetState(m_RasterStateCullCW);
        else if( _CullMode==CULL_CCW )
            m_D3DContext->RSSetState(m_RasterStateCullCCW);

        // Render the rect
        D3DX11_TECHNIQUE_DESC techDesc;
        m_LineRectTech->GetDesc(&techDesc);
        for(UINT p=0; p<techDesc.Passes; ++p)
        {
            m_LineRectTech->GetPassByIndex(p)->Apply(0, m_D3DContext);
            m_D3DContext->Draw(3*_NumTriangles, 0);
        }

        if( _CullMode==CULL_CW || _CullMode==CULL_CCW )
            m_D3DContext->RSSetState(m_RasterState); // restore default raster state
    }
}

//  ---------------------------------------------------------------------------
