#pragma once

#include <MasterFrame/XFrameApp.h>

#include <Xtreme/XRenderer.h>



class XFont;
class X2dRenderer;

class CXtreme : public XFrameApp
{

  protected:

    enum DisplayMode
    {
      DM_2D,
      DM_FIRST_ENTRY = DM_2D,
      DM_3D_1_LIGHT,
      DM_3D_2_LIGHTS,
      DM_BOX_CIRCLING_LIGHT,
      DM_QUAD_CACHE_TEST,
      DM_LINE_CACHE_TEST,

      DM_LAST_ENTRY
    };

    XTexture*           m_pTexture;
    XTexture*           m_pTexture2;
    XTexture*           m_pTexture3;
    XTexture*           m_pTextureColorKey;
    XTexture*           m_pTextureTGA;

    XVertexBuffer*      m_pVB;

    XVertexBuffer*      m_pVBCloned;

    XVertexBuffer*      m_pVBTest;

    XFont*              m_pFont;
    XFont*              m_pFontBig;

    float               m_fX;

    float               m_fFPS;

    float               m_fLastFPSStopTime;

    GR::f32             m_Angle;

    GR::u32             m_Frames;

    GR::u32             m_SoundClick;

    GR::tVector         m_vectRotatedPos;
    GR::tVector         m_vectRotatedPosX;
    GR::tVector         m_vectRotatedPosY;

    GR::String          m_NewInput;

    XLight              m_Light;

    DisplayMode         m_DisplayMode;
    bool                m_3Released;
    bool                m_LReleased;
    bool                m_FReleased;
    bool                m_BReleased;
    bool                m_VReleased;
    bool                m_FogEnabled;

    int                 m_NumLightsActive;
    GR::f32             m_LightAngle;

    GR::f32             m_TextureTransformFactor;

    GR::String          m_NextRenderer;
    GR::String          m_NextRenderer2d;

    int                 m_NumBoundingBoxes;
    int                 m_NumParticles;
    int                 m_NumLines;

    X2dRenderer*        m_pRenderer2d;

    int                 m_Screen;



  public:

    CXtreme();
    virtual ~CXtreme();

    virtual void        DisplayFrame( XRenderer& Renderer );
    virtual void        UpdatePerDisplayFrame( const float fElapsedTime );

    virtual bool        ProcessEvent( const GR::Gamebase::tXFrameEvent& Event );

    void                RestoreData();

    virtual LRESULT     WindowProc( UINT uMsg, WPARAM wParam, LPARAM lParam );


    void Display2LightScene( XRenderer& Renderer );
    void DisplayCirclingLightScene( XRenderer& Renderer );
    void DisplayCacheTest( XRenderer& Renderer );
    void DisplayLineCacheTest( XRenderer& Renderer );

    virtual void Configure( Xtreme::EnvironmentConfig& Config );

    void                DisplayFrame2d( X2dRenderer& Renderer );
    bool                SwitchRenderer2d( const char* szFileName = NULL );
    virtual void        UpdateFixedLogic();

    void DisplayOpenGLDirectly( XRenderer& Renderer );



    void                DisplayScreenFlat( XRenderer& Renderer );
    void                DisplayScreenFlatTextured( XRenderer& Renderer );
    void                DisplayScreenFlatTexturedAlphaTest( XRenderer& Renderer );
    void                DisplayScreenAlphaBlended( XRenderer& Renderer );
    void                DisplayScreenAlphaBlendedTextured( XRenderer& Renderer );
    void                DisplayScreenAdditive( XRenderer& Renderer );
    void                DisplayScreenTextureAlphaOnly( XRenderer& Renderer );

    void                DisplayScreen3dFlatLightsDisabled( XRenderer& Renderer );
    void                DisplayScreen3dFlatTexturedLightsDisabled( XRenderer& Renderer );

    void                DisplayScreen3dFlatNoLight( XRenderer& Renderer );
    void                DisplayScreen3dFlatTexturedNoLight( XRenderer& Renderer );

    void                DisplayScreen3dFlatOneLight( XRenderer& Renderer );
    void                DisplayScreen3dFlatTexturedOneLight( XRenderer& Renderer );

    void                DisplayScreen3dFlatTwoLights( XRenderer& Renderer );
    void                DisplayScreen3dFlatTexturedTwoLights( XRenderer& Renderer );

    void                DisplayScreen3dQuadOneLight( XRenderer& Renderer );
};


extern CXtreme    theApp;