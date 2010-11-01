//  ---------------------------------------------------------------------------
//
//  @file       TwDirect3D10.h
//  @brief      Direct3D10 graph functions
//  @author     Philippe Decaudin - http://www.antisphere.com
//  @license    This file is part of the AntTweakBar library.
//              For conditions of distribution and use, see License.txt
//
//  notes:      Private header
//              TAB=4
//
//  ---------------------------------------------------------------------------


#if !defined ANT_TW_DIRECT3D11_INCLUDED
#define ANT_TW_DIRECT3D11_INCLUDED

#include "TwGraph.h"
#include "effects11/inc/d3dx11effect.h"

//  ---------------------------------------------------------------------------

class CTwGraphDirect3D11 : public ITwGraph
{
public:
    virtual int                 Init();
    virtual int                 Shut();
    virtual void                BeginDraw(int _WndWidth, int _WndHeight);
    virtual void                EndDraw();
    virtual bool                IsDrawing();
    virtual void                Restore();
    virtual void                DrawLine(int _X0, int _Y0, int _X1, int _Y1, color32 _Color0, color32 _Color1, bool _AntiAliased=false);
    virtual void                DrawLine(int _X0, int _Y0, int _X1, int _Y1, color32 _Color, bool _AntiAliased=false) { DrawLine(_X0, _Y0, _X1, _Y1, _Color, _Color, _AntiAliased); }
    virtual void                DrawRect(int _X0, int _Y0, int _X1, int _Y1, color32 _Color00, color32 _Color10, color32 _Color01, color32 _Color11);
    virtual void                DrawRect(int _X0, int _Y0, int _X1, int _Y1, color32 _Color) { DrawRect(_X0, _Y0, _X1, _Y1, _Color, _Color, _Color, _Color); }
    virtual void                DrawTriangles(int _NumTriangles, int *_Vertices, color32 *_Colors, Cull _CullMode);

    virtual void *              NewTextObj();
    virtual void                DeleteTextObj(void *_TextObj);
    virtual void                BuildText(void *_TextObj, const std::string *_TextLines, color32 *_LineColors, color32 *_LineBgColors, int _NbLines, const CTexFont *_Font, int _Sep, int _BgWidth);
    virtual void                DrawText(void *_TextObj, int _X, int _Y, color32 _Color, color32 _BgColor);

    virtual void                ChangeViewport(int _X0, int _Y0, int _Width, int _Height, int _OffsetX, int _OffsetY);
    virtual void                RestoreViewport();

protected:
    struct ID3D11Device *       m_D3DDev;
    struct ID3D11DeviceContext *m_D3DContext;
    unsigned int                m_D3DDevInitialRefCount;
    bool                        m_Drawing;
    const CTexFont *            m_FontTex;
    struct ID3D11ShaderResourceView *m_FontD3DTexRV;
    int                         m_WndWidth;
    int                         m_WndHeight;
    int                         m_OffsetX;
    int                         m_OffsetY;
    void *                      m_ViewportInit;

    struct CLineRectVtx
    {
        float                   m_Pos[3];
        color32                 m_Color;
    };
    struct CTextVtx
    {
        float                   m_Pos[3];
        color32                 m_Color;
        float                   m_UV[2];
    };

    struct CTextObj
    {
        struct ID3D11Buffer *   m_TextVertexBuffer;
        struct ID3D11Buffer *   m_BgVertexBuffer;
        int                     m_NbTextVerts;
        int                     m_NbBgVerts;
        int                     m_TextVertexBufferSize;
        int                     m_BgVertexBufferSize;
        bool                    m_LineColors;
        bool                    m_LineBgColors;
    };

		D3DX11_STATE_BLOCK_MASK					m_StateBlockMask;
    struct StateBlock *               m_State;
    struct ID3D11DepthStencilState *m_DepthStencilState;
    struct ID3D11BlendState *       m_BlendState;
    struct ID3D11RasterizerState *  m_RasterState;
    struct ID3D11RasterizerState *  m_RasterStateAntialiased;
    struct ID3D11RasterizerState *  m_RasterStateCullCW;
    struct ID3D11RasterizerState *  m_RasterStateCullCCW;
    struct ID3DX11Effect *           m_Effect;
    struct ID3DX11EffectTechnique*   m_LineRectTech;
    struct ID3DX11EffectTechnique*   m_LineRectCstColorTech;
    struct ID3D11InputLayout *      m_LineRectVertexLayout;
    struct ID3D11Buffer *           m_LineVertexBuffer;
    struct ID3D11Buffer *           m_RectVertexBuffer;
    struct ID3D11Buffer *           m_TrianglesVertexBuffer;
    int                             m_TrianglesVertexBufferCount;
    struct ID3DX11EffectTechnique*   m_TextTech;
    struct ID3DX11EffectTechnique*   m_TextCstColorTech;
    struct ID3D11InputLayout *      m_TextVertexLayout;
    struct ID3DX11EffectShaderResourceVariable *m_FontD3DResVar;
    struct ID3DX11EffectVectorVariable *m_OffsetVar;
    struct ID3DX11EffectVectorVariable *m_CstColorVar;
};

//  ---------------------------------------------------------------------------


#endif // !defined ANT_TW_DIRECT3D10_INCLUDED
