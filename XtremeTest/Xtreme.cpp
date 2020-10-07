#include ".\xtreme.h"

#include <Misc/Misc.h>

#include <Xtreme/XFont.h>

#include <Xtreme/MeshFormate/Tao.h>
#include <Xtreme/MeshFormate/MD3.h>
#include <Xtreme/MeshFormate/T3DLoader.h>

#include <WinSys/WinUtils.h>

#include <Controls/Xtreme/GUIComponentDisplayer.h>
#include <Controls/Xtreme/GUIButton.h>

#include "resource.h"

#include <Xtreme/X2dRenderer.h>


CXtreme     theApp;


CXtreme::CXtreme() :
  m_pVB( NULL ),
  m_pVBCloned( NULL ),
  m_pTexture( NULL ),
  m_pTexture2( NULL ),
  m_pTexture3( NULL ),
  m_fX( 0.0f ),
  m_pFont( NULL ),
  m_vectRotatedPos( 5.0f, 0.0f, 0.0f ),
  m_vectRotatedPosX( 5.0f, 0.0f, 0.0f ),
  m_vectRotatedPosY( 5.0f, 0.0f, 0.0f ),
  m_Angle( 0.0f ),
  m_DisplayMode( DM_FIRST_ENTRY ),
  m_3Released( false ),
  m_LReleased( false ),
  m_FReleased( false ),
  m_BReleased( false ),
  m_VReleased( false ),
  m_FogEnabled( false ),
  m_NumLightsActive( 0 ),
  m_NumBoundingBoxes( 1 ),
  m_NumParticles( 0 ),
  m_NumLines( 1 ),
  m_LightAngle( 0.0f ),
  m_TextureTransformFactor( 0.0f ),
  m_pRenderer2d( NULL )
{
  m_Frames = 0;
  m_fLastFPSStopTime = 0.0f;
  m_pVBTest = NULL;

  memset( &m_Light, 0, sizeof( m_Light ) );
  m_Light.m_Ambient = 0xff202020;
  m_Light.m_Type    = XLight::LT_DIRECTIONAL;
  m_Light.m_Diffuse = 0xff807060;
  m_Light.m_Direction.set( 0.0f, 0.0f, -1.0f );
}



CXtreme::~CXtreme()
{
}



void CXtreme::Configure( Xtreme::EnvironmentConfig& Config )
{
  Config.FixedSize = false;
  Config.KeepAspectRatio = false;
}



void CXtreme::DisplayFrame2d( X2dRenderer& Renderer )
{
  Renderer.RenderQuad( 5, 5, 20, 20, 0xffff00ff );
  Renderer.RenderLine( GR::tPoint( rand() % 640, rand() % 480 ), GR::tPoint( rand() % 640, rand() % 480 ), 0xffffffff );
}



void CXtreme::Display2LightScene( XRenderer& Renderer )
{
  XMaterial       material;

  ZeroMemory( &material, sizeof( XMaterial ) );
  material.Ambient = 0xff808080;
  material.Diffuse = 0xffffffff;
  Renderer.SetMaterial( material );

  XLight      Light;

  memset( &Light, 0, sizeof( XLight ) );

  Light.m_Type = XLight::LT_DIRECTIONAL;
  Light.m_Ambient = 0xff404040;
  Light.m_Diffuse = 0xff0000c0;

  Light.m_Direction.set( 0.2f, 0.8f, -0.3f );

  Renderer.SetLight( 0, Light );
  Renderer.SetState( XRenderer::RS_LIGHT, XRenderer::RSV_ENABLE );

  //Renderer.SetState( XRenderer::RS_LIGHT, XRenderer::RSV_DISABLE, 1 );
  memset( &Light, 0, sizeof( XLight ) );

  Light.m_Type = XLight::LT_DIRECTIONAL;
  Light.m_Ambient = 0xff808080;
  Light.m_Diffuse = 0xff400000;

  Light.m_Direction.set( -0.3f, 0.8f, 0.2f );

  Renderer.SetLight( 1, Light );
  Renderer.SetState( XRenderer::RS_LIGHT, XRenderer::RSV_ENABLE, 1 );
  m_pRenderClass->SetState( XRenderer::RS_LIGHTING, XRenderer::RSV_ENABLE );

  m_pRenderClass->Clear();

  math::matrix4     mat;

  mat.ProjectionPerspectiveFovLH( 90, 32.0f / 24.0f, 0.1f, 100.0f );

  m_pRenderClass->SetTransform( XRenderer::TT_PROJECTION, mat );

  XCamera cam;

  cam.SetValues( GR::tVector( 10, 0, 30 ),
    GR::tVector( 0, 0, 0 ), 
    GR::tVector( 0, -1, 0 ) );

  m_pRenderClass->SetTransform( XRenderer::TT_VIEW, cam.GetViewMatrix() );

  math::matrix4   matWorld;

  matWorld = math::matrix4().Scaling( 1 / 8.0f, 1 / 8.0f, 1 / 8.0f );
  matWorld *= ( math::matrix4().RotationY( m_Angle ) * math::matrix4().Translation( -10.0f, 0.0f, 0.0f ) );

  m_pRenderClass->SetTransform( XRenderer::TT_WORLD, matWorld );

  m_pRenderClass->SetTexture( 0, m_pTexture );
  m_pRenderClass->SetShader( XRenderer::ST_FLAT );

  m_pRenderClass->RenderVertexBuffer( m_pVBCloned );

  m_pRenderClass->SetState( XRenderer::RS_LIGHTING, XRenderer::RSV_DISABLE );
  Renderer.SetState( XRenderer::RS_LIGHT, XRenderer::RSV_DISABLE, 1 );

  m_pRenderClass->SetShader( XRenderer::ST_ALPHA_TEST );
  if ( m_pFont )
  {
    m_pRenderClass->RenderText2d( m_pFont, 10, 10, "2 lights scene", 0xffffffff );
  }
}



void CXtreme::DisplayCacheTest( XRenderer& Renderer )
{
  Renderer.Clear();

  math::matrix4     mat;

  mat.ProjectionPerspectiveFovLH( 90, 32.0f / 24.0f, 0.1f, 100.0f );

  Renderer.SetTransform( XRenderer::TT_PROJECTION, mat );

  XCamera cam;

  cam.SetValues( GR::tVector( 0, 0, 30 ),
                 GR::tVector( 0, 0, 0 ),
                 GR::tVector( 0, -1, 0 ) );

  Renderer.SetTransform( XRenderer::TT_VIEW, cam.GetViewMatrix() );

  Renderer.SetTransform( XRenderer::TT_WORLD, math::matrix4().RotationY( m_Angle ) );

  for ( int i = 0; i < m_NumParticles; ++i )
  {
    GR::tVector     pos;

    pos.x = ( GR::f32 )( rand() % 41 - 20.0f );
    pos.y = ( GR::f32 )( rand() % 41 - 20.0f );


    GR::tVector     objPos( pos );

    math::matrix4   matWorld;
    
    matWorld.Billboard( Renderer.Matrix( XRenderer::TT_VIEW ), pos );

    Renderer.SetTransform( XRenderer::TT_WORLD, matWorld );

    GR::f32 BillboardWidth = 2.0f;
    GR::f32 BillboardHeight = 1.5f;

    // billboard
    Renderer.RenderTextureSection( GR::tVector( -BillboardWidth * 0.5f, BillboardHeight, 0.0f ),
                                   GR::tVector( -BillboardWidth * 0.5f + BillboardWidth, BillboardHeight, 0.0f ),
                                   GR::tVector( -BillboardWidth * 0.5f, 0.0f, 0.0f ),
                                   GR::tVector( -BillboardWidth * 0.5f + BillboardWidth, 0.0f, 0.0f ),
                                   XTextureSection( m_pTexture3, 64, 128, 31, 31 ),
                                   0xffffffff );
  }
  m_pRenderClass->SetShader( XRenderer::ST_ALPHA_TEST );
  if ( m_pFont )
  {
    m_pRenderClass->RenderText2d( m_pFont, 10, 10, Misc::Format( "Quad Cache scene, %1% quads, press p/P, FPS %02::2%" ) << m_NumParticles << m_fFPS, 0xffffffff );
  }
}



void CXtreme::DisplayLineCacheTest( XRenderer& Renderer )
{
  Renderer.Clear();

  Renderer.SetShader( XRenderer::ST_FLAT_NO_TEXTURE );
  for ( int i = 0; i < m_NumLines; ++i )
  {
    GR::tFPoint      target( 200, 0 );
    target.RotateZ( i * 7.0f );
    Renderer.RenderLine2d( GR::tPoint( 100, 100 ),
                           GR::tPoint( (int)( 100 + target.x ), (int)( 100 + target.y ) ),
                           0xffffffff );
  }

  m_pRenderClass->SetShader( XRenderer::ST_ALPHA_TEST );
  if ( m_pFont )
  {
    m_pRenderClass->RenderText2d( m_pFont, 10, 10, Misc::Format( "Line Cache scene, %1% lines, press +/-, FPS %02::2%" ) << m_NumLines << m_fFPS, 0xffffffff );
  }
}



void CXtreme::DisplayCirclingLightScene( XRenderer& Renderer )
{
  XMaterial       material;

  ZeroMemory( &material, sizeof( XMaterial ) );
  //material.Ambient = 0xff808080;
  material.Ambient = 0xffffffff;
  material.Diffuse = 0xffffffff;
  Renderer.SetMaterial( material );

  XLight      Light;

  memset( &Light, 0, sizeof( XLight ) );

  Light.m_Type = XLight::LT_DIRECTIONAL;
  Light.m_Ambient = 0xff404040;
  Light.m_Diffuse = 0xffc0c0c0;

  Light.m_Direction.set( 1.0f, 0.0f, 0.0f );
  Light.m_Direction.RotateZ( m_LightAngle );

  Renderer.SetLight( 0, Light );
  Renderer.SetState( XRenderer::RS_LIGHT, m_NumLightsActive ? XRenderer::RSV_ENABLE : XRenderer::RSV_DISABLE );

  /*
  memset( &Light, 0, sizeof( XLight ) );

  Light.m_Type = XLight::LT_DIRECTIONAL;
  Light.m_Ambient = 0xff808080;
  Light.m_Diffuse = 0xff404040;

  Light.m_Direction.set( -0.3f, 0.8f, 0.2f );

  Renderer.SetLight( 1, Light );
  Renderer.SetState( XRenderer::RS_LIGHT, XRenderer::RSV_ENABLE, 1 );
  */
  Renderer.SetState( XRenderer::RS_LIGHT, XRenderer::RSV_DISABLE, 1 );
  Renderer.SetState( XRenderer::RS_LIGHTING, XRenderer::RSV_ENABLE );

  Renderer.Clear();

  math::matrix4     mat;

  mat.ProjectionPerspectiveFovLH( 90, 32.0f / 24.0f, 0.1f, 100.0f );

  Renderer.SetTransform( XRenderer::TT_PROJECTION, mat );

  XCamera cam;

  cam.SetValues( GR::tVector( 0, 0, 30 ),
    GR::tVector( 0, 0, 0 ),
    GR::tVector( 0, -1, 0 ) );

  m_pRenderClass->SetTransform( XRenderer::TT_VIEW, cam.GetViewMatrix() );

  m_pRenderClass->SetTransform( XRenderer::TT_WORLD, math::matrix4().RotationY( m_Angle ) );

  m_pRenderClass->SetTexture( 0, m_pTexture );
  m_pRenderClass->SetShader( XRenderer::ST_FLAT );

  m_pRenderClass->RenderVertexBuffer( m_pVBCloned );

  m_pRenderClass->SetState( XRenderer::RS_LIGHTING, XRenderer::RSV_DISABLE );
  Renderer.SetState( XRenderer::RS_LIGHT, XRenderer::RSV_DISABLE, 1 );

  Renderer.SetShader( XRenderer::ST_FLAT_NO_TEXTURE );

  for ( int i = 0; i < m_NumBoundingBoxes; ++i )
  {
    Renderer.RenderBoundingBox( GR::tVector( -1.0f + i * 3.0f, -1.0f, -1.0f ), GR::tVector( 2.0f + i * 3.0f, 2.0f, 2.0f ), 0xff00ff00 );
  }

  m_pRenderClass->SetShader( XRenderer::ST_ALPHA_TEST );
  if ( m_pFont )
  {
    //m_pRenderClass->RenderText2d( m_pFont, 10, 10, CMisc::printf( "Circling Light, %d lights active", m_NumLightsActive ), 0xffffffff );
    m_pRenderClass->RenderText2d( m_pFont, 10, 10, Misc::Format( "Circling Light, %1% lights active, %2% boxes" ) << m_NumLightsActive << m_NumBoundingBoxes, 0xffffffff );
  }
}



#pragma comment( lib, "opengl32.lib" )
#pragma comment( lib, "glu32.lib" )

#include <gl\gl.h>
#include <gl\glu.h>
#include <D:\SDK\OpenGL\gl\glext.h>

void CXtreme::DisplayOpenGLDirectly( XRenderer& Renderer )
{
  //Renderer.SetTexture( 0, m_pTextureColorKey );

  Renderer.Clear();

  // flat
  glDisable( GL_BLEND );
  glDisable( GL_ALPHA_TEST );
  glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
  glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE );

  /*
  Renderer.SetState( XRenderer::RS_ALPHATEST, XRenderer::RSV_DISABLE );
  Renderer.SetState( XRenderer::RS_ALPHABLENDING, XRenderer::RSV_DISABLE );

  Renderer.SetState( XRenderer::RS_COLOR_OP, XRenderer::RSV_MODULATE );
  Renderer.SetState( XRenderer::RS_COLOR_ARG_1, XRenderer::RSV_TEXTURE );
  Renderer.SetState( XRenderer::RS_COLOR_ARG_2, XRenderer::RSV_DIFFUSE );

  Renderer.SetState( XRenderer::RS_ALPHA_OP, XRenderer::RSV_SELECT_ARG_1 );
  Renderer.SetState( XRenderer::RS_ALPHA_ARG_1, XRenderer::RSV_TEXTURE );

  Renderer.SetState( XRenderer::RS_COLOR_OP, XRenderer::RSV_DISABLE, 1 );
  Renderer.SetState( XRenderer::RS_ALPHA_OP, XRenderer::RSV_DISABLE, 1 );
  */

  // plain
  Renderer.RenderTextureSection2d( 5, 5, XTextureSection( m_pTextureColorKey, 0, 0, 64, 64 ) );

  // modulated
  Renderer.RenderTextureSection2d( 85, 5, XTextureSection( m_pTextureColorKey, 0, 0, 64, 64 ), 0xffff00ff );

  
  // alpha test

  glBlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
  glEnable( GL_BLEND );
  glEnable( GL_TEXTURE_2D );

  
  //glAlphaFunc( GL_GEQUAL, 8.0f / 255.0f );
  //glEnable( GL_ALPHA_TEST );

  
  //glBlendFunc( GL_ONE, GL_ONE ); 
  //glBlendFunc( GL_DST_COLOR, GL_ONE );

  /*
  glTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_EXT );
  glTexEnvf( GL_TEXTURE_ENV, GL_COMBINE_ALPHA_EXT, GL_MODULATE );
  glTexEnvf( GL_TEXTURE_ENV, GL_SOURCE0_ALPHA_EXT, GL_TEXTURE );
  glTexEnvf( GL_TEXTURE_ENV, GL_SOURCE1_ALPHA_EXT, GL_PRIMARY_COLOR_EXT );
  // rgb always modulates
  glTexEnvf( GL_TEXTURE_ENV, GL_COMBINE_RGB_EXT, GL_MODULATE );
  glTexEnvf( GL_TEXTURE_ENV, GL_SOURCE0_RGB_EXT, GL_TEXTURE );
  glTexEnvf( GL_TEXTURE_ENV, GL_SOURCE1_RGB_EXT, GL_PRIMARY_COLOR_EXT );*/
  Renderer.RenderTextureSection2d( 165, 5, XTextureSection( m_pTextureColorKey, 0, 0, 64, 64 ) );
  //Renderer.RenderTextureSection2d( 165, 5, XTextureSection( m_pTexture2, 144, 0, 16, 16 ) );
}



void CXtreme::DisplayFrame( XRenderer& Renderer )
{
  //DisplayOpenGLDirectly( Renderer );
  //return;

  // since we're using GameStates we have to reset the Viewport to full screen!
  XViewport   vp = Renderer.Viewport();

  vp.X = 0;
  vp.Y = 0;
  vp.Width = Renderer.Width();
  vp.Height = Renderer.Height();
  Renderer.SetViewport( vp );
  ++m_Frames;

  if ( m_FogEnabled )
  {
    Renderer.SetState( XRenderer::RS_FOG_ENABLE, XRenderer::RSV_ENABLE );
    Renderer.SetState( XRenderer::RS_FOG_COLOR, 0xffff00ff );
    Renderer.SetState( XRenderer::RS_FOG_VERTEXMODE, XRenderer::RSV_FOG_LINEAR );

    GR::f32  fValue = ( GR::f32 )25.0f;
    Renderer.SetState( XRenderer::RS_FOG_START, *( GR::u32* )&fValue );
    fValue = 35.0f;
    Renderer.SetState( XRenderer::RS_FOG_END, *( GR::u32* )&fValue );
  }
  else
  {
    Renderer.SetState( XRenderer::RS_FOG_ENABLE, XRenderer::RSV_DISABLE );
  }
  /*
  m_pRenderClass->Clear();


  XTextureSection   tsTemp2( m_pTexture, 0, 0, 128, 128 );
  XTextureSection   tsTemp3( m_pTextureColorKey, 0, 0, 64, 64 );

  m_pRenderClass->SetShader( XRenderer::ST_FLAT_NO_TEXTURE );
  m_pRenderClass->RenderQuad2d( 192, 0, 64, 64, 0xffffffff );

  m_pRenderClass->SetShader( XRenderer::ST_FLAT );
  m_pRenderClass->RenderTextureSection2d( 0, 0, tsTemp2 );
  m_pRenderClass->RenderTextureSection2d( 128, 0, tsTemp3 );

  //m_pRenderClass->RenderTextureSection2d( 192, 0, tsTemp3 );
  //m_pRenderClass->RenderTextureSection2d( 256, 0, tsTemp3 );

  return;
  */

  if ( m_DisplayMode == DM_3D_2_LIGHTS )
  {
    m_pRenderClass->Clear();

    Display2LightScene( Renderer );
    return;
  }
  else if ( m_DisplayMode == DM_3D_1_LIGHT )
  {
    ++m_Frames;

    m_pRenderClass->Clear();

    math::matrix4     mat;

    mat.ProjectionPerspectiveFovLH( 90, 32.0f / 24.0f, 0.1f, 100.0f );

    m_pRenderClass->SetTransform( XRenderer::TT_PROJECTION, mat );

    XCamera cam;
    
    cam.SetValues( GR::tVector( 0, 0, 30 ),
      GR::tVector( 0, 0, 0 ),
      GR::tVector( 0, -1, 0 ) );

    m_pRenderClass->SetTransform( XRenderer::TT_VIEW, cam.GetViewMatrix() );

    m_pRenderClass->SetTransform( XRenderer::TT_WORLD, math::matrix4().RotationY( m_Angle ) * math::matrix4().Translation( -10.0f, 0.0f, 0.0f ) );

    m_pRenderClass->SetTexture( 0, m_pTexture );
    m_pRenderClass->SetShader( XRenderer::ST_FLAT );

    m_pRenderClass->SetState( XRenderer::RS_LIGHT, XRenderer::RSV_ENABLE, 0 );
    m_pRenderClass->SetState( XRenderer::RS_LIGHTING, XRenderer::RSV_ENABLE );

    m_pRenderClass->RenderVertexBuffer( m_pVBCloned );

    m_pRenderClass->SetState( XRenderer::RS_LIGHTING, XRenderer::RSV_DISABLE );
    return;
  }
  else if ( m_DisplayMode == DM_BOX_CIRCLING_LIGHT )
  {
    DisplayCirclingLightScene( Renderer );
    return;
  }
  else if ( m_DisplayMode == DM_QUAD_CACHE_TEST )
  {
    DisplayCacheTest( Renderer );
    return;
  }
  else if ( m_DisplayMode == DM_LINE_CACHE_TEST )
  {
    DisplayLineCacheTest( Renderer );
    return;
  }

  if ( m_pVBTest == NULL )
  {
    // simple, "pretransformed" buffer
    XMesh     mesh;

    mesh.AddVertex( 0.0f, 1.5f, 0.0f );
    mesh.AddVertex( 1.45f, -1.5f, 0.0f );
    mesh.AddVertex( -1.45f, -1.5f, 0.0f );

    Mesh::Face    face( 0, 0xffff0000, 1, 0xff00ff00, 2, 0xff0000ff );

    face.m_TextureX[0] = 0.5f;
    face.m_TextureY[0] = 0.0f;
    face.m_TextureX[1] = 0.0f;
    face.m_TextureY[1] = 1.0f;
    face.m_TextureX[2] = 1.0f;
    face.m_TextureY[2] = 1.0f;

    mesh.AddFace( face );

    /* 
    // simple quad
    XMesh       mesh;

    GR::f32 iX = -1.5f;
    GR::f32 iWidth = 3.0f;
    GR::f32 iY = 1.5f;
    GR::f32 iHeight = -3.0f;
    GR::f32 fZ = 0.0f;

    DWORD     dwColor1 = 0xffff0000;
    DWORD     dwColor2 = 0xff00ff00;
    DWORD     dwColor3 = 0xff0000ff;
    DWORD     dwColor4 = 0xffffffff;

    GR::f32   fTU1 = 0.0f;
    GR::f32   fTV1 = 0.0f;
    GR::f32   fTU2 = 1.0f;
    GR::f32   fTV2 = 0.0f;
    GR::f32   fTU3 = 0.0f;
    GR::f32   fTV3 = 1.0f;
    GR::f32   fTU4 = 1.0f;
    GR::f32   fTV4 = 1.0f;


    mesh.AddVertex( GR::tVector( iX + 0.5f, iY + 0.5f, fZ ) );
    mesh.AddVertex( GR::tVector( iX + iWidth + 0.5f, iY + 0.5f, fZ ) );
    mesh.AddVertex( GR::tVector( iX + 0.5f, iY + iHeight + 0.5f, fZ ) );
    mesh.AddVertex( GR::tVector( iX + iWidth + 0.5f, iY + iHeight + 0.5f, fZ ) );

    Mesh::Face    face1( 0, dwColor1, 1, dwColor2, 2, dwColor3 );
    face1.m_TextureX[0] = fTU1;
    face1.m_TextureY[0] = fTV1;
    face1.m_TextureX[1] = fTU2;
    face1.m_TextureY[1] = fTV2;
    face1.m_TextureX[2] = fTU3;
    face1.m_TextureY[2] = fTV3;

    Mesh::Face    face2( 2, dwColor3, 1, dwColor2, 3, dwColor4 );
    face2.m_TextureX[0] = fTU3;
    face2.m_TextureY[0] = fTV3;
    face2.m_TextureX[1] = fTU2;
    face2.m_TextureY[1] = fTV2;
    face2.m_TextureX[2] = fTU4;
    face2.m_TextureY[2] = fTV4;

    mesh.AddFace( face1 );
    mesh.AddFace( face2 );*/
    m_pVBTest = Renderer.CreateVertexBuffer( mesh, XVertexBuffer::VFF_XYZ | XVertexBuffer::VFF_DIFFUSE );
    //m_pVBTest = Renderer.CreateVertexBuffer( mesh, XVertexBuffer::VFF_XYZ | XVertexBuffer::VFF_DIFFUSE | XVertexBuffer::VFF_TEXTURECOORD );
  }

  if ( m_pVBTest == NULL )
  {
    int m_iDiagonal = rand() % 3;

    int m_iBlocks = 2 + rand() % 3;

    XMesh           objFloor;

    GR::tVector   vectOffset( 0, 0, 0 );

    // Deckel
    GR::u32    iV1 = (GR::u32)objFloor.AddVertex( vectOffset.x, vectOffset.y, 1.0f );
    GR::u32    iV2 = (GR::u32)objFloor.AddVertex( vectOffset.x + m_iBlocks, vectOffset.y + (int)m_iBlocks * m_iDiagonal * 0.2f, 1.0f );
    GR::u32    iV3 = (GR::u32)objFloor.AddVertex( vectOffset.x, vectOffset.y, 0.0f );
    GR::u32    iV4 = (GR::u32)objFloor.AddVertex( vectOffset.x + m_iBlocks, vectOffset.y + (int)m_iBlocks * m_iDiagonal * 0.2f, 0.0f );

    Mesh::Face    Face1( iV1, iV2, iV3 );
    Face1.m_TextureX[0] = 81.0f / 256.0f;
    Face1.m_TextureY[0] = 0.0f;
    Face1.m_TextureX[1] = 95.0f / 256.0f;
    Face1.m_TextureY[1] = 0.0f;
    Face1.m_TextureX[2] = 81.0f / 256.0f;
    Face1.m_TextureY[2] = 16.0f / 256.0f;
    Face1.m_DiffuseColor[0] = Face1.m_DiffuseColor[1] = Face1.m_DiffuseColor[2] = 0xffff00ff;
    objFloor.AddFace( Face1 );

    Mesh::Face    Face2( iV3, iV2, iV4 );
    Face2.m_TextureX[0] = 81.0f / 256.0f;
    Face2.m_TextureY[0] = 16.0f / 256.0f;
    Face2.m_TextureX[1] = 95.0f / 256.0f;
    Face2.m_TextureY[1] = 0.0f;
    Face2.m_TextureX[2] = 95.0f / 256.0f;
    Face2.m_TextureY[2] = 16.0f / 256.0f;
    Face2.m_DiffuseColor[0] = Face2.m_DiffuseColor[1] = Face2.m_DiffuseColor[2] = 0xffff00ff;
    objFloor.AddFace( Face2 );

    // Front
    GR::u32    iVertex[4];

    for ( GR::i32 i = 0; i < m_iBlocks; ++i )
    {
      iVertex[0] = (GR::u32)objFloor.AddVertex( i + vectOffset.x,         vectOffset.y + i * m_iDiagonal * 0.2f,        0.0f );
      iVertex[1] = (GR::u32)objFloor.AddVertex( i + vectOffset.x + 1.0f,  vectOffset.y + ( i + 1 ) * m_iDiagonal * 0.2f,        0.0f );
      iVertex[2] = (GR::u32)objFloor.AddVertex( i + vectOffset.x,         vectOffset.y + i * m_iDiagonal * 0.2f - 1.0f, 0.0f );
      iVertex[3] = (GR::u32)objFloor.AddVertex( i + vectOffset.x + 1.0f,  vectOffset.y + ( i + 1 ) * m_iDiagonal * 0.2f - 1.0f, 0.0f );

      float   fOtherBlock = ( 16.0f / 256.0f ) * ( i % 2 );

      Mesh::Face    Face1( iVertex[0], iVertex[1], iVertex[2] );
      /*
      if ( m_bCrumbleOnceLeft )
      {
        Face1.m_fTextureX[0] = 33.0f / 256.0f;
        Face1.m_fTextureY[0] = 18.0f / 256.0f;
        Face1.m_fTextureX[1] = 41.0f / 256.0f;
        Face1.m_fTextureY[1] = 18.0f / 256.0f;
        Face1.m_fTextureX[2] = 33.0f / 256.0f;
        Face1.m_fTextureY[2] = 26.0f / 256.0f;
      }
      else if ( m_bIronbar )
      {
        Face1.m_fTextureX[0] = 44.0f / 256.0f;
        Face1.m_fTextureY[0] = 18.0f / 256.0f;
        Face1.m_fTextureX[1] = 53.0f / 256.0f;
        Face1.m_fTextureY[1] = 18.0f / 256.0f;
        Face1.m_fTextureX[2] = 44.0f / 256.0f;
        Face1.m_fTextureY[2] = 26.0f / 256.0f;
      }
      else
      */
      {
        Face1.m_TextureX[0] = 1.0f / 256.0f + fOtherBlock;
        Face1.m_TextureY[0] = 0.0f;
        Face1.m_TextureX[1] = 15.0f / 256.0f + fOtherBlock;
        Face1.m_TextureY[1] = 0.0f;
        Face1.m_TextureX[2] = 1.0f / 256.0f + fOtherBlock;
        Face1.m_TextureY[2] = 16.0f / 256.0f;
      }
      objFloor.AddFace( Face1 );

      Mesh::Face    Face2( iVertex[2], iVertex[1], iVertex[3] );
      /*
      if ( m_bCrumbleOnceLeft )
      {
        Face2.m_fTextureX[0] = 33.0f / 256.0f;
        Face2.m_fTextureY[0] = 26.0f / 256.0f;
        Face2.m_fTextureX[1] = 41.0f / 256.0f;
        Face2.m_fTextureY[1] = 18.0f / 256.0f;
        Face2.m_fTextureX[2] = 41.0f / 256.0f;
        Face2.m_fTextureY[2] = 26.0f / 256.0f;
      }
      else if ( m_bIronbar )
      {
        Face2.m_fTextureX[0] = 44.0f / 256.0f;
        Face2.m_fTextureY[0] = 26.0f / 256.0f;
        Face2.m_fTextureX[1] = 53.0f / 256.0f;
        Face2.m_fTextureY[1] = 18.0f / 256.0f;
        Face2.m_fTextureX[2] = 53.0f / 256.0f;
        Face2.m_fTextureY[2] = 26.0f / 256.0f;
      }
      else
      */
      {
        Face2.m_TextureX[0] = 1.0f / 256.0f + fOtherBlock;
        Face2.m_TextureY[0] = 16.0f / 256.0f;
        Face2.m_TextureX[1] = 15.0f / 256.0f + fOtherBlock;
        Face2.m_TextureY[1] = 0.0f;
        Face2.m_TextureX[2] = 15.0f / 256.0f + fOtherBlock;
        Face2.m_TextureY[2] = 16.0f / 256.0f;
      }
      objFloor.AddFace( Face2 );
    }

    // rechte Kante
    Mesh::Face    Face3( iVertex[1], iV2, iVertex[3] );
    Face3.m_TextureX[0] = 1.0f / 256.0f;
    Face3.m_TextureY[0] = 0.0f / 256.0f;
    Face3.m_TextureX[1] = 15.0f / 256.0f;
    Face3.m_TextureY[1] = 0.0f;
    Face3.m_TextureX[2] = 1.0f / 256.0f;
    Face3.m_TextureY[2] = 16.0f / 256.0f;
    objFloor.AddFace( Face3 );

    iV4 = (GR::u32)objFloor.AddVertex( vectOffset.x + m_iBlocks, vectOffset.y + (int)m_iBlocks * m_iDiagonal * 0.2f - 1.0f, 1.0f );

    Mesh::Face    Face4( iVertex[3], iV2, iV4 );
    Face4.m_TextureX[0] = 1.0f / 256.0f;
    Face4.m_TextureY[0] = 16.0f / 256.0f;
    Face4.m_TextureX[1] = 15.0f / 256.0f;
    Face4.m_TextureY[1] = 0.0f;
    Face4.m_TextureX[2] = 15.0f / 256.0f;
    Face4.m_TextureY[2] = 16.0f / 256.0f;
    objFloor.AddFace( Face4 );

    m_pVBTest = Renderer.CreateVertexBuffer( objFloor );
  }

  GUIComponentDisplayer::Instance().m_pActualRenderer = m_pRenderClass;
  GUIComponentDisplayer::Instance().SetExtents( Renderer.Width(), Renderer.Height() );

  Renderer.SetState( XRenderer::RS_ALPHATEST, XRenderer::RSV_DISABLE );
  Renderer.SetState( XRenderer::RS_ALPHABLENDING, XRenderer::RSV_DISABLE );

  Renderer.SetShader( XRenderer::ST_FLAT );
  /*
  Renderer.SetState( XRenderer::RS_COLOR_OP, XRenderer::RSV_MODULATE );
  Renderer.SetState( XRenderer::RS_COLOR_ARG_1, XRenderer::RSV_TEXTURE );
  Renderer.SetState( XRenderer::RS_COLOR_ARG_2, XRenderer::RSV_DIFFUSE );

  Renderer.SetState( XRenderer::RS_ALPHA_OP, XRenderer::RSV_SELECT_ARG_1 );
  Renderer.SetState( XRenderer::RS_ALPHA_ARG_1, XRenderer::RSV_TEXTURE );

  Renderer.SetState( XRenderer::RS_COLOR_OP, XRenderer::RSV_DISABLE, 1 );
  Renderer.SetState( XRenderer::RS_ALPHA_OP, XRenderer::RSV_DISABLE, 1 );
  */

  m_pRenderClass->Clear();

  math::matrix4     mat;
  mat.LookAtLH( GR::tVector( 0, 0, -10 ), 
                      GR::tVector( 0, 0, 10 ),
                      GR::tVector( 0, 1, 0 ) );
  m_pRenderClass->SetTransform( XRenderer::TT_VIEW, mat );

  RECT    rc;

  GetClientRect( m_RenderFrame.m_hwndMain, &rc );

  mat.OrthoLH( (float)( rc.right - rc.left ), (float)( rc.bottom - rc.top ), 0.0f, 100.0f );

  m_pRenderClass->SetTransform( XRenderer::TT_PROJECTION, mat );

  m_pRenderClass->SetTexture( 0, m_pTexture );
  m_pRenderClass->SetShader( XRenderer::ST_FLAT );
  m_pRenderClass->SetState( XRenderer::RS_COLOR_OP,     XRenderer::RSV_SELECT_ARG_1 );
  m_pRenderClass->SetState( XRenderer::RS_COLOR_ARG_1, XRenderer::RSV_DIFFUSE );
  m_pRenderClass->RenderQuad2d( 20, 20, 40, 40,
                                0.0f, 0.0f, 1.0f, 0.0f,
                                0.0f, 1.0f, 1.0f, 1.0f,
                                0xffffffff,
                                0xff0000ff,
                                0xff00ff00,
                                0xffff0000, 0.001f );

  mat.OrthoLH( (float)( rc.right - rc.left ), (float)( rc.bottom - rc.top ), 0.0f, 110.0f );
  m_pRenderClass->SetTransform( XRenderer::TT_PROJECTION, mat );

  /*
  m_pRenderClass->SetState( XRenderer::RS_COLOR_OP,     XRenderer::RSV_MODULATE );
  m_pRenderClass->SetState( XRenderer::RS_COLOR_ARG_1, XRenderer::RSV_DIFFUSE );
  m_pRenderClass->SetState( XRenderer::RS_COLOR_ARG_2, XRenderer::RSV_TEXTURE );

  m_pRenderClass->SetState( XRenderer::RS_CULLMODE, XRenderer::RSV_CULL_NONE );

  m_pRenderClass->SetState( XRenderer::RS_LIGHTING, XRenderer::RSV_DISABLE );
  */

  m_pRenderClass->SetShader( XRenderer::ST_FLAT );
  m_pRenderClass->SetTexture( 0, m_pTexture );

  math::matrix4   matMovedCube;

  matMovedCube = math::matrix4().RotationZ( m_Angle ) * math::matrix4().Translation( m_fX, 0, 0 );
  m_pRenderClass->SetTransform( XRenderer::TT_WORLD, matMovedCube );
  m_pRenderClass->RenderVertexBuffer( m_pVBCloned );

  m_pRenderClass->SetShader( XRenderer::ST_FLAT_NO_TEXTURE );
  //m_pRenderClass->SetTexture( 0, m_pTexture );
  m_pRenderClass->SetState( XRenderer::RS_LIGHTING, XRenderer::RSV_DISABLE );
  //m_pRenderClass->SetShader( XRenderer::ST_FLAT );
  m_pRenderClass->SetTransform( XRenderer::TT_WORLD, math::matrix4().Translation( -m_fX, 0, 0 ) * math::matrix4().Scaling( 10.0f, 10.0f, 10.0f ) );
  m_pRenderClass->RenderVertexBuffer( m_pVBTest );

  m_pRenderClass->SetTransform( XRenderer::TT_WORLD, math::matrix4().Translation( -2 * m_fX, 0, 0 ) * math::matrix4().Scaling( 10.0f, 10.0f, 10.0f ) );
  m_pRenderClass->RenderVertexBuffer( m_pVBTest );

  //m_pRenderClass->SetShader( XRenderer::ST_ALPHA_TEST );
  m_pRenderClass->SetShader( XRenderer::ST_FLAT );
  m_pRenderClass->SetTexture( 0, m_pTexture );
  m_pRenderClass->SetTransform( XRenderer::TT_WORLD, math::matrix4().Translation( 0, 0, 50 ) );
  //m_pRenderClass->RenderBox( GR::tVector( 0, 0, 0 ), GR::tVector( 50, 50, 50 ), 0xffff00ff );
  //m_pRenderClass->SetTransform( XRenderer::TT_WORLD, math::matrix4().Identity() );
  m_pRenderClass->RenderBox( GR::tVector( 0, 0, 0 ), GR::tVector( 50, 50, 50 ), 0xffff80ff );

  /*
  m_pRenderClass->RenderTextureSection2d( 400, 200, XTextureSection( m_pTexture3,  96, 128, 31, 31 ) );
  m_pRenderClass->RenderTextureSection2d( 440, 200, XTextureSection( m_pTexture3,  96, 128, 31, 31, XTextureSection::TSF_H_MIRROR ) );
  m_pRenderClass->RenderTextureSection2d( 480, 200, XTextureSection( m_pTexture3,  96, 128, 31, 31, XTextureSection::TSF_V_MIRROR ) );
  m_pRenderClass->RenderTextureSection2d( 520, 200, XTextureSection( m_pTexture3,  96, 128, 31, 31, XTextureSection::TSF_H_MIRROR | XTextureSection::TSF_V_MIRROR ) );
    */                                                                              
  m_pRenderClass->RenderTextureSection2d( 400, 200, XTextureSection( m_pTexture3,  64, 128, 31, 31, XTextureSection::TSF_ROTATE_90 ) );
  m_pRenderClass->RenderTextureSection2d( 440, 200, XTextureSection( m_pTexture3,  64, 128, 31, 31, XTextureSection::TSF_ROTATE_90 | XTextureSection::TSF_H_MIRROR ) );
  m_pRenderClass->RenderTextureSection2d( 480, 200, XTextureSection( m_pTexture3,  64, 128, 31, 31, XTextureSection::TSF_ROTATE_90 | XTextureSection::TSF_V_MIRROR ) );
  m_pRenderClass->RenderTextureSection2d( 520, 200, XTextureSection( m_pTexture3,  64, 128, 31, 31, XTextureSection::TSF_ROTATE_90 | XTextureSection::TSF_H_MIRROR | XTextureSection::TSF_V_MIRROR ) );

  m_pRenderClass->RenderTextureSection2d( 400, 240, XTextureSection( m_pTexture3,  96, 128, 31, 31, XTextureSection::TSF_ROTATE_90 ) );
  m_pRenderClass->RenderTextureSection2d( 440, 240, XTextureSection( m_pTexture3,  96, 128, 31, 31, XTextureSection::TSF_ROTATE_90 | XTextureSection::TSF_H_MIRROR ) );
  m_pRenderClass->RenderTextureSection2d( 480, 240, XTextureSection( m_pTexture3,  96, 128, 31, 31, XTextureSection::TSF_ROTATE_90 | XTextureSection::TSF_V_MIRROR ) );
  m_pRenderClass->RenderTextureSection2d( 520, 240, XTextureSection( m_pTexture3,  96, 128, 31, 31, XTextureSection::TSF_ROTATE_90 | XTextureSection::TSF_H_MIRROR | XTextureSection::TSF_V_MIRROR ) );
                                                                                  
  m_pRenderClass->RenderTextureSection2d( 400, 280, XTextureSection( m_pTexture3, 128, 128, 31, 31, XTextureSection::TSF_ROTATE_90 ) );
  m_pRenderClass->RenderTextureSection2d( 440, 280, XTextureSection( m_pTexture3, 128, 128, 31, 31, XTextureSection::TSF_ROTATE_90 | XTextureSection::TSF_H_MIRROR ) );
  m_pRenderClass->RenderTextureSection2d( 480, 280, XTextureSection( m_pTexture3, 128, 128, 31, 31, XTextureSection::TSF_ROTATE_90 | XTextureSection::TSF_V_MIRROR ) );
  m_pRenderClass->RenderTextureSection2d( 520, 280, XTextureSection( m_pTexture3, 128, 128, 31, 31, XTextureSection::TSF_ROTATE_90 | XTextureSection::TSF_H_MIRROR | XTextureSection::TSF_V_MIRROR ) );
  /*
  m_pRenderClass->RenderTextureSection2d( 400, 280, XTextureSection( m_pTexture3,  96, 128, 31, 31, XTextureSection::TSF_ROTATE_180 ) );
  m_pRenderClass->RenderTextureSection2d( 440, 280, XTextureSection( m_pTexture3,  96, 128, 31, 31, XTextureSection::TSF_ROTATE_180 | XTextureSection::TSF_H_MIRROR ) );
  m_pRenderClass->RenderTextureSection2d( 480, 280, XTextureSection( m_pTexture3,  96, 128, 31, 31, XTextureSection::TSF_ROTATE_180 | XTextureSection::TSF_V_MIRROR ) );
  m_pRenderClass->RenderTextureSection2d( 520, 280, XTextureSection( m_pTexture3,  96, 128, 31, 31, XTextureSection::TSF_ROTATE_180 | XTextureSection::TSF_H_MIRROR | XTextureSection::TSF_V_MIRROR ) );
    */

  m_pRenderClass->RenderTextureSection2d( 400, 320, XTextureSection( m_pTexture3,  96, 128, 31, 31, XTextureSection::TSF_ROTATE_270 ) );
  m_pRenderClass->RenderTextureSection2d( 440, 320, XTextureSection( m_pTexture3,  96, 128, 31, 31, XTextureSection::TSF_ROTATE_270 | XTextureSection::TSF_H_MIRROR ) );
  m_pRenderClass->RenderTextureSection2d( 480, 320, XTextureSection( m_pTexture3,  96, 128, 31, 31, XTextureSection::TSF_ROTATE_270 | XTextureSection::TSF_V_MIRROR ) );
  m_pRenderClass->RenderTextureSection2d( 520, 320, XTextureSection( m_pTexture3,  96, 128, 31, 31, XTextureSection::TSF_ROTATE_270 | XTextureSection::TSF_H_MIRROR | XTextureSection::TSF_V_MIRROR ) );


  m_pRenderClass->RenderTextureSectionRotated2d( 320, 240,
                                                 XTextureSection( m_pTexture3, 96, 128, 31, 31, XTextureSection::TSF_ROTATE_270 | XTextureSection::TSF_H_MIRROR | XTextureSection::TSF_V_MIRROR ),
                                                 m_Angle );

  // texture transform
  math::matrix4     matTexTransform;
  math::matrix4     matProj;
  XCamera           cam;

  cam.SetValues( GR::tVector( 0.0f, (GR::f32)m_pRenderClass->Width(), 10.0f ),
                 GR::tVector( 0.0f, (GR::f32)m_pRenderClass->Height(), 0.0f ),
                 GR::tVector( 0.0f, -5.0f, 0.0f ) );

  matTexTransform.Identity();
  matProj.OrthoOffCenterLH( 0.0f, (GR::f32)m_pRenderClass->Width(), 0.0f, (GR::f32)m_pRenderClass->Height(), 0.001f, 1000.0f );

  m_pRenderClass->SetTransform( XRenderer::TT_VIEW, cam.GetViewMatrix() );
  m_pRenderClass->SetTransform( XRenderer::TT_PROJECTION, matProj );


  matTexTransform.ms._31 = m_TextureTransformFactor / 256.0f;
  matTexTransform.ms._32 = m_TextureTransformFactor / 256.0f;


  // wirkt sich auf x aus
  //matTexTransform.ms._14 = 0.5f + m_TextureTransformFactor / 256.0f;

  // wirkt sich auf y aus
  //matTexTransform.ms._24 = 0.5f + m_TextureTransformFactor / 256.0f;
  m_pRenderClass->SetState( XRenderer::RS_TEXTURE_TRANSFORM, XRenderer::RSV_TEXTURE_TRANSFORM_COUNT2 );
  m_pRenderClass->SetTransform( XRenderer::TT_TEXTURE_STAGE_0, matTexTransform );
  
  m_pRenderClass->RenderTextureSection( GR::tVector( 400.0f, 360.0f, 0.0f ),
                                        GR::tVector( 432.0f, 360.0f, 0.0f ),
                                        GR::tVector( 400.0f, 492.0f, 0.0f ),
                                        GR::tVector( 432.0f, 492.0f, 0.0f ),
                                        XTextureSection( m_pTexture3, 64, 128, 31, 31 ),
                                        0xffffffff );
  m_pRenderClass->SetTransform( XRenderer::TT_TEXTURE_STAGE_0, matTexTransform );
  m_pRenderClass->SetState( XRenderer::RS_TEXTURE_TRANSFORM, XRenderer::RSV_DISABLE );

  mat.LookAtLH( GR::tVector( 0, 0, -10 ),
                GR::tVector( 0, 0, 10 ),
                GR::tVector( 0, 1, 0 ) );
  m_pRenderClass->SetTransform( XRenderer::TT_VIEW, mat );
  mat.OrthoLH( (float)( rc.right - rc.left ), (float)( rc.bottom - rc.top ), 0.0f, 100.0f );
  m_pRenderClass->SetTransform( XRenderer::TT_PROJECTION, mat );


  m_pRenderClass->SetShader( XRenderer::ST_ALPHA_TEST );
  if ( m_pFont )
  {
    m_pRenderClass->SetTransform( XRenderer::TT_WORLD, math::matrix4().Identity() );

    m_pRenderClass->RenderText2d( m_pFont, 10, 10, CMisc::printf( "FPS: %.2f", m_fFPS ), 0xffffffff );
    if ( m_pInputClass )
    {
      m_pRenderClass->RenderText2d( m_pFont, 10, 330, CMisc::printf( "Mouse: %d,%d", m_pInputClass->MouseX(), m_pInputClass->MouseY() ), 0xffffffff );

      m_pRenderClass->RenderText2d( m_pFont, 10, 350, CMisc::printf( "Analog Controls: %d", m_pInputClass->GetAnalogControlCount() ), 0xffffffff );
    }
    m_pRenderClass->SetTransform( XRenderer::TT_WORLD, math::matrix4().Identity() );
    m_pRenderClass->RenderText( m_pFont,
                                GR::tVector( 3.0f, 0, 0 ),
                                "Hurz",
                                GR::tVector( 10, 10, 0 ),
                                0xffffffff );

    m_pRenderClass->RenderText2d( m_pFont, 50, 50, "Hurz", 0.5f, 0.5f, 0xffffffff );
    m_pRenderClass->RenderText( m_pFont, GR::tVector( 3.0f, 0.0f, 0.0f ), 
                                "Wullewatz", 
                                m_vectRotatedPos,
                                0xffff00ff );
    m_pRenderClass->RenderText( m_pFont, GR::tVector( 1.0f, 0.0f, 0.0f ), "Wullewatz", 
                                m_vectRotatedPosX );
    m_pRenderClass->RenderText( m_pFont, GR::tVector( 2.0f, 0.0f, 0.0f ), "Wullewatz", 
                                m_vectRotatedPosY );
  }

  Renderer.SetTexture( 0, NULL );
  Renderer.RenderRect2d( GR::tPoint( 120, 150 ),
                         GR::tPoint( 40, 40 ),
                         0xff00ffff );

  XTextureSection   tsTemp( m_pTexture, 0, 0, 128, 128 );

  m_pRenderClass->RenderTextureSection2d( 0, 0, tsTemp );

  tsTemp.m_Flags |= XTextureSection::TSF_H_MIRROR;
  m_pRenderClass->RenderTextureSection2d( 128, 0, tsTemp );

  tsTemp.m_Flags |= XTextureSection::TSF_H_MIRROR | XTextureSection::TSF_ROTATE_90;
  m_pRenderClass->RenderTextureSection2d( 256, 0, tsTemp );

  tsTemp.m_Flags |= XTextureSection::TSF_H_MIRROR | XTextureSection::TSF_ROTATE_180;
  m_pRenderClass->RenderTextureSection2d( 384, 0, tsTemp );

  tsTemp.m_Flags |= XTextureSection::TSF_H_MIRROR | XTextureSection::TSF_ROTATE_270;
  m_pRenderClass->RenderTextureSection2d( 512, 0, tsTemp );

  // normal color keyed
  m_pRenderClass->SetShader( XRenderer::ST_ALPHA_TEST );
  m_pRenderClass->RenderTextureSection2d( 20, 320, XTextureSection( m_pTextureColorKey, 0, 0, 64, 64 ) );
  // normal color keyed and colorized
  m_pRenderClass->RenderTextureSection2d( 100, 320, XTextureSection( m_pTextureColorKey, 0, 0, 64, 64 ), 0xffff8080 );

  // no texture (although texture is set)
  m_pRenderClass->SetShader( XRenderer::ST_FLAT_NO_TEXTURE );
  m_pRenderClass->RenderTextureSection2d( 180, 320, XTextureSection( m_pTextureColorKey, 0, 0, 64, 64 ), 0xff1140ee );

  // Quad with 4 colors interpolated
  m_pRenderClass->RenderTextureSection2d( 260, 320, XTextureSection( m_pTextureColorKey, 0, 0, 64, 64 ), 0xffff0000, 0xffff00ff, 0xff00ff00, 0xff0000ff );

  m_pRenderClass->SetShader( XRenderer::ST_ALPHA_TEST );
  // double size - should appear scaled
  m_pRenderClass->SetState( XRenderer::RS_MINFILTER, XRenderer::RSV_FILTER_LINEAR );
  m_pRenderClass->SetState( XRenderer::RS_MAGFILTER, XRenderer::RSV_FILTER_LINEAR );
  m_pRenderClass->SetState( XRenderer::RS_MIPFILTER, XRenderer::RSV_FILTER_LINEAR );
  m_pRenderClass->RenderTextureSection2d( 20, 400, XTextureSection( m_pTextureColorKey, 0, 0, 64, 64 ), 0xffff8080, 128, 128 );

  // double size - should appear crisp
  m_pRenderClass->SetState( XRenderer::RS_MINFILTER, XRenderer::RSV_FILTER_POINT );
  m_pRenderClass->SetState( XRenderer::RS_MAGFILTER, XRenderer::RSV_FILTER_POINT );
  m_pRenderClass->SetState( XRenderer::RS_MIPFILTER, XRenderer::RSV_FILTER_POINT );
  m_pRenderClass->RenderTextureSection2d( 160, 400, XTextureSection( m_pTextureColorKey, 0, 0, 64, 64 ), 0xffff8080, 128, 128 );

  m_pRenderClass->SetState( XRenderer::RS_MINFILTER, XRenderer::RSV_FILTER_POINT );
  m_pRenderClass->SetState( XRenderer::RS_MAGFILTER, XRenderer::RSV_FILTER_POINT );
  m_pRenderClass->SetState( XRenderer::RS_MIPFILTER, XRenderer::RSV_FILTER_POINT );
  m_pRenderClass->SetShader( XRenderer::ST_FLAT );
  m_pRenderClass->RenderTextureSection2d( 62, 222, XTextureSection( m_pTexture2, 144, 0, 16, 16 ), 0xffffffff, 40, 40, -1, 0.000000f );
  m_pRenderClass->RenderTextureSection2d( 112, 222, XTextureSection( m_pTexture2, 144, 0, 16, 16 ), 0xffffffff, 20, 20, -1, 0.000000f );
  m_pRenderClass->RenderTextureSection2d( 142, 222, XTextureSection( m_pTexture2, 144, 0, 16, 16 ), 0xffffffff, 80, 80, -1, 0.000000f );

  m_pRenderClass->SetShader( XRenderer::ST_ALPHA_TEST );
  m_pRenderClass->RenderTextureSection2d( 62, 22, XTextureSection( m_pTexture2, 144, 0, 16, 16 ), 0xffffffff, 40, 40, -1, 0.000000f );
  m_pRenderClass->RenderTextureSection2d( 112, 22, XTextureSection( m_pTexture2, 144, 0, 16, 16 ), 0xffffffff, 20, 20, -1, 0.000000f );
  m_pRenderClass->RenderTextureSection2d( 142, 22, XTextureSection( m_pTexture2, 144, 0, 16, 16 ), 0xffffffff, 80, 80, -1, 0.000000f );

  // use texture alpha to fill with diffuse color only
  m_pRenderClass->SetShader( XRenderer::ST_ALPHA_TEST_COLOR_FROM_DIFFUSE );
  m_pRenderClass->RenderTextureSection2d( 340, 320, XTextureSection( m_pTextureColorKey, 0, 0, 64, 64 ), 0xffffffff );


  GUIComponentDisplayer::Instance().DisplayAllControls();

  //m_pRenderClass->SetTexture( 0, m_pTextureColorKey );

  XViewport     vpOld = m_pRenderClass->Viewport();

  XViewport     vpNew = vpOld;

  vpNew.Height = 218;
  m_pRenderClass->SetViewport( vpNew );

  m_pRenderClass->SetTexture( 0, NULL );
  m_pRenderClass->SetShader( XRenderer::ST_ALPHA_TEST );
  m_pRenderClass->RenderQuad2d( 200, 200, 36, 36, 0xffff00ff );

  m_pRenderClass->SetViewport( vpOld );


  XTextureSection   tsLaser = XTextureSection( m_pTextureTGA, 1, 115, 43, 43 );

  Renderer.SetState( XRenderer::RS_LIGHTING, XRenderer::RSV_DISABLE );
  Renderer.SetState( XRenderer::RS_ZWRITE, XRenderer::RSV_DISABLE );
  Renderer.SetShader( XRenderer::ST_ADDITIVE );

  Renderer.RenderTextureSection2d( 0, 200, tsLaser, 0xffc500c5 );

  Renderer.SetShader( XRenderer::ST_FLAT_NO_TEXTURE );
  Renderer.RenderLine2d( GR::tPoint( 0, 0 ), GR::tPoint( 640, 480 ), 0xffffffff, 0xff00ff00 );

  /*
  if ( m_pFont )
  {
    m_pRenderClass->RenderText2d( m_pFont, 320, 10, CMisc::printf( "Console is %d", m_ConsoleVisible ) );
  }*/
  if ( ( m_pFont )
  &&   ( m_pSoundClass ) )
  {
    Renderer.SetShader( XRenderer::ST_ALPHA_BLEND_AND_TEST );
    m_pRenderClass->RenderText2d( m_pFont, 320, 310, CMisc::printf( "SFX is playing: %d", m_pSoundClass->IsPlaying( m_SoundClick ) ) );
  }
}



void CXtreme::UpdatePerDisplayFrame( const float fElapsedTime )
{
  m_fLastFPSStopTime += fElapsedTime;
  if ( m_fLastFPSStopTime >= 1.0f )
  {
    m_fFPS = (float)m_Frames / m_fLastFPSStopTime;
    m_fLastFPSStopTime -= 1.0f;
    m_Frames = 0;
  }


  if ( !m_NextRenderer.empty() )
  {
    SwitchRenderer();
    if ( m_NextRenderer == "null" )
    {
      SwitchRenderer();
    }
    else
    {
      SwitchRenderer2d();
      SwitchRenderer( m_NextRenderer.c_str() );
    }
    m_pVBTest = NULL;
    m_NextRenderer = "";
  }

  if ( !m_NextRenderer2d.empty() )
  {
    if ( m_NextRenderer2d == "null" )
    {
      SwitchRenderer2d();
    }
    else
    {
      SwitchRenderer();
      SwitchRenderer2d( m_NextRenderer2d.c_str() );
    }
    m_pVBTest = NULL;
    m_NextRenderer2d = "";
  }

  m_TextureTransformFactor += 32.0f * fElapsedTime;

  GUIComponentDisplayer::Instance().UpdateAllControls( fElapsedTime );

  if ( GetAsyncKeyState( VK_SPACE ) & 0x8000 )
  {
    InvalidateWindow();
  }

  m_LightAngle = fmodf( m_LightAngle + 45.0f * fElapsedTime, 360.0f );

  if ( GetAsyncKeyState( 'F' ) & 0x8000 )
  {
    if ( m_FReleased )
    {
      m_FReleased = false;
      m_FogEnabled = !m_FogEnabled;
    }
  }
  else
  {
    m_FReleased = true;
  }

  if ( GetAsyncKeyState( 'L' ) & 0x8000 )
  {
    if ( m_LReleased )
    {
      m_LReleased = false;

      m_NumLightsActive = 1 - m_NumLightsActive;
    }
  }
  else
  {
    m_LReleased = true;
  }

  if ( GetAsyncKeyState( 'B' ) & 0x8000 )
  {
    if ( m_BReleased )
    {
      m_BReleased = false;

      ++m_NumBoundingBoxes;
    }
  }
  else
  {
    m_BReleased = true;
  }
  if ( GetAsyncKeyState( 'V' ) & 0x8000 )
  {
    if ( m_VReleased )
    {
      m_VReleased = false;

      if ( m_NumBoundingBoxes > 0 )
      {
        --m_NumBoundingBoxes;
      }
    }
  }
  else
  {
    m_VReleased = true;
  }

  m_Angle += fElapsedTime * 45.0f;

  m_vectRotatedPos.RotateZ( fElapsedTime * 50.0f );
  m_vectRotatedPosX.RotateX( fElapsedTime * 50.0f );
  m_vectRotatedPosY.RotateY( fElapsedTime * 50.0f );

  if ( m_pRenderClass )
  {
    if ( GetAsyncKeyState( VK_LEFT ) & 0x8000 )
    {
      m_fX -= 50.0f * fElapsedTime;

      math::matrix4   matTranslation;

      matTranslation.Translation( m_fX, 0.0f, 50.0f );

      m_pRenderClass->SetTransform( XRenderer::TT_WORLD, matTranslation );
    }
    else if ( GetAsyncKeyState( VK_RIGHT ) & 0x8000 )
    {
      m_fX += 50.0f * fElapsedTime;

      math::matrix4   matTranslation;

      matTranslation.Translation( m_fX, 0.0f, 50.0f );

      m_pRenderClass->SetTransform( XRenderer::TT_WORLD, matTranslation );
    }
  }
  if ( !m_NewInput.empty() )
  {
    if ( m_NewInput == "none" )
    {
      SwitchInput();
    }
    else
    {
      SwitchInput( m_NewInput.c_str() );
    }
    m_NewInput.clear();
  }
}



void CXtreme::UpdateFixedLogic()
{
  if ( m_pRenderer2d )
  {
    InvalidateRect( m_RenderFrame.m_hwndMain, NULL, FALSE );
  }
}



void CXtreme::RestoreData()
{
  m_pFont               = NULL;
  m_pTexture            = NULL;
  m_pTexture2           = NULL;
  m_pTexture3 = NULL;
  m_pTextureColorKey    = NULL;
  m_pVB                 = NULL;

  if ( m_pRenderClass )
  {
    RECT    rc;
    GetClientRect( m_RenderFrame.m_hwndMain, &rc );

    if ( m_pRenderClass->Initialize( rc.right - rc.left, rc.bottom - rc.top, 16, false, GR::Service::Environment::Instance() ) )
    {
      m_pRenderClass->SetLight( 0, m_Light );
      /*
      CLoadMD3    md3Loader;

      XMesh*    pMeshSpikes = md3Loader.ImportMD3( CMisc::AppPath( "player.md3" ) );
        //CMeshObjectLoader::LoadDX8Object( CMisc::AppPath( "spikes.md3" ) );

      if ( pMeshSpikes )
      {
        m_pVB = m_pRenderClass->CreateVertexBuffer( *pMeshSpikes );
        delete pMeshSpikes;
      }*/

      /*
      m_pVBCloned = NULL;
      if ( m_pVB )
      {
        m_pVBCloned = m_pRenderClass->CreateVertexBuffer( m_pVB );
      }*/

      //m_pRenderClass->LoadTexture( "d:/projekte/HitBlockDeluxe3d2013/images/back.igf" );

      XMesh*       pMesh = NULL;

      pMesh = CT3DMeshLoader::Load( CMisc::AppPath( "cube.t3d" ).c_str() );
      pMesh->CalculateNormals();
      m_pVB = m_pRenderClass->CreateVertexBuffer( *pMesh, XVertexBuffer::VFF_XYZ | XVertexBuffer::VFF_DIFFUSE | XVertexBuffer::VFF_NORMAL | XVertexBuffer::VFF_TEXTURECOORD );
      delete pMesh;
      if ( m_pVB )
      {
        m_pVBCloned = m_pRenderClass->CreateVertexBuffer( m_pVB );
      }

      m_pTextureTGA = m_pRenderClass->LoadTexture( CMisc::AppPath( "deco.tga" ).c_str() );

      m_pTexture2 = m_pRenderClass->LoadTexture( CMisc::AppPath( "test2.igf" ).c_str(), GR::Graphic::IF_UNKNOWN, 0xffff00ff, 0, 0xff000000 );
      m_pTexture3 = m_pRenderClass->LoadTexture( CMisc::AppPath( "test3.igf" ).c_str() );

      m_pTexture = m_pRenderClass->LoadTexture( CMisc::AppPath( "test.igf" ).c_str() );
      m_pTextureColorKey = m_pRenderClass->LoadTexture( CMisc::AppPath( "colorkeytexture.igf" ).c_str(), GR::Graphic::IF_UNKNOWN, 0xffff00ff );
      m_pRenderClass->SetState( XRenderer::RS_CLEAR_COLOR, 0xff808080 );

      m_pFont = m_pRenderClass->LoadFontSquare( CMisc::AppPath( "font.tga" ).c_str(), XFont::FLF_SQUARED_ONE_FONT | XFont::FLF_ALPHA_BIT );


      /*
      m_pVB = m_pRenderClass->CreateVertexBuffer( IVertexBuffer::VFF_XYZ | IVertexBuffer::VFF_DIFFUSE | IVertexBuffer::VFF_TEXTURECOORD );
      m_pVB->AddVertex( GR::tVector( 0, 0, 0.5f ), 1.0f,
                        0xffff0000,
                        GR::tFPoint( 0.0f, 0.0f ) );
      m_pVB->AddVertex( GR::tVector( 10, 0, 0.5f ), 1.0f,
                        0xff00ff00,
                        GR::tFPoint( 1.0f, 0.0f ) );
      m_pVB->AddVertex( GR::tVector( 0, 10, 0.5f ), 1.0f,
                        0xff0000ff,
                        GR::tFPoint( 0.0f, 1.0f ) );
                        */
    }
    else
    {
      dh::Log( "Init of renderer failed" );
      SwitchRenderer();
    }
  }
  else if ( m_pRenderer2d )
  {
    RECT    rc;
    GetClientRect( m_RenderFrame.m_hwndMain, &rc );

    if ( m_pRenderer2d->Initialize( rc.right - rc.left, rc.bottom - rc.top, 16, false, GR::Service::Environment::Instance() ) )
    {

    }
    else
    {
      dh::Log( "m_pRenderer2d->Initialize failed" );
    }
  }

  if ( m_pSoundClass )
  {
    if ( m_pSoundClass->Initialize( GR::Service::Environment::Instance() ) )
    {
      ConsolePrint( "SoundClass Initialized" );
       
      m_SoundClick = m_pSoundClass->LoadWave( CMisc::AppPath( "click.wav" ).c_str() );
    }
  }

}



bool CXtreme::ProcessEvent( const GR::Gamebase::tXFrameEvent& Event )
{
  switch ( Event.m_Type )
  {
    case GR::Gamebase::tXFrameEvent::ET_RENDERER_SWITCHED:
      if ( ( m_pRenderClass )
      ||   ( m_pRenderer2d ) )
      {
        RestoreData();
      }
      break;
    case GR::Gamebase::tXFrameEvent::ET_SOUND_SWITCHED:
      if ( m_pSoundClass )
      {
        RestoreData();
      }
      break;
  }

  return XFrameApp::ProcessEvent( Event );
}


#undef DIRECT3D_VERSION
#include <d3d9.h>


#pragma comment( lib, "d3d9.lib" )

#include <Xtreme/DX9/DX9RenderClass.h>


HBITMAP ScreenGrab()
{
  dh::Log( "a %x", GetLastError() );
   //RenderTargetSurface.
   IDirect3DSurface9* pRenderTargetSurface = NULL;

   //DestinationTargetSurface
   IDirect3DSurface9* pDestinationTargetSurface = NULL;

   //DisplayMode
   D3DDISPLAYMODE d3dDipMode;

   IDirect3DDevice9* m_pd3dDevice = ( (CDX9RenderClass*)theApp.Renderer() )->Device();
   if (m_pd3dDevice == NULL)
      return NULL;

   dh::Log( "b %x", GetLastError() );
   //Get the client rectangle
   RECT rc;
   GetClientRect( theApp.GetSafeHwnd(), &rc);
   ClientToScreen(theApp.GetSafeHwnd(), LPPOINT(&rc.left));
   ClientToScreen(theApp.GetSafeHwnd(), LPPOINT(&rc.right));

   dh::Log( "c %x", GetLastError() );

   //GetRenderTargetSurface.
   if(FAILED(m_pd3dDevice->GetRenderTarget(0, &pRenderTargetSurface)))
	   return NULL;

   IDirect3D9*    m_pD3D;

   dh::Log( "d %x", GetLastError() );
   
   m_pd3dDevice->GetDirect3D( &m_pD3D );

   //Display Mode (d3dDipMode)
   if(FAILED(m_pD3D->GetAdapterDisplayMode(D3DADAPTER_DEFAULT,&d3dDipMode)))
       return NULL;
   
   dh::Log( "e %x", GetLastError() );

   //GetDestinationTargetSurface
   if(FAILED(m_pd3dDevice->CreateOffscreenPlainSurface((rc.right - rc.left),
	                                               (rc.bottom - rc.top),
                                                   d3dDipMode.Format,
												   D3DPOOL_SYSTEMMEM,
                                                   &pDestinationTargetSurface,
												   NULL)))
												   return NULL;

   dh::Log( "f %x", GetLastError() );

   //copy RenderTargetSurface -> DestTarget
   if(FAILED(m_pd3dDevice->GetRenderTargetData(pRenderTargetSurface, pDestinationTargetSurface)))
	   return NULL;

   dh::Log( "g %x", GetLastError() );

   //Create a lock on the DestinationTargetSurface
   D3DLOCKED_RECT lockedRC;
   if(FAILED(pDestinationTargetSurface->LockRect(&lockedRC,
	                                   NULL,
									   D3DLOCK_NO_DIRTY_UPDATE|D3DLOCK_NOSYSLOCK|D3DLOCK_READONLY)))
   return NULL;

   dh::Log( "h %x", GetLastError() );
   int windowSize = (rc.bottom - rc.top) * (rc.right - rc.left) * 4;

   LPBYTE lpSource = reinterpret_cast<LPBYTE>(lockedRC.pBits);

   //Create a handle to a bitmap (return from CreateDIBitmap())
   HBITMAP hbm;

   //Create a Device Context (parameter 1 for CreateDIBitmap())
   HDC    hdcDesktop = GetDC( GetDesktopWindow() );
   HDC    hCaptureDC = CreateCompatibleDC( hdcDesktop );
   //HDC hCaptureDC = CreateCompatibleDC(NULL);

   //Create a BITMAPINFO/BITMAPINFOHEADER structure and fill it(parameter 2 & 5 for CreateDIBitmap())

   BITMAPINFO*    pbmpInfo = (BITMAPINFO*)new GR::u8[sizeof( BITMAPINFOHEADER ) + sizeof( GR::u16 ) * 256];

   //BITMAPINFO	bmpInfo;

   BITMAPINFO& bmpInfo = (*pbmpInfo );
   ZeroMemory(&bmpInfo,sizeof(BITMAPINFO));
   bmpInfo.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
   bmpInfo.bmiHeader.biBitCount=16;
   bmpInfo.bmiHeader.biCompression = BI_RGB;
   bmpInfo.bmiHeader.biWidth= rc.right - rc.left;
   bmpInfo.bmiHeader.biHeight= -( rc.bottom - rc.top );;
   bmpInfo.bmiHeader.biSizeImage = 0;
   bmpInfo.bmiHeader.biPlanes = 1;
   bmpInfo.bmiHeader.biClrUsed = 0;
   bmpInfo.bmiHeader.biClrImportant = 0;

   dh::Log( "i %x", GetLastError() );
   void* pData = NULL;

   //Create DIB Section -> this returns a valid pointer to use in pData
   hbm = CreateDIBSection(hCaptureDC,
	                    pbmpInfo,//&bmpInfo,
			            DIB_PAL_COLORS,
		                (void**)&pData,
			            NULL,
			            NULL);

   delete[] pbmpInfo;

   
   dh::Log( "j %x (%x)", GetLastError(), hbm );

   LPBYTE lpDestination = (LPBYTE)pData;
   LPBYTE lpDestinationTemp = lpDestination;

    //memcpy data across
    int termination = rc.bottom - rc.top;

    for ( int iY = 0; iY < termination; ++iY )
    {
      memcpy( lpDestinationTemp, lpSource, 2 * ( rc.right - rc.left ) );
      lpSource += lockedRC.Pitch;
      lpDestinationTemp += 2 * ( rc.right - rc.left );
    }

    dh::Log( "k %x", GetLastError() );

   //Unlock the rectangle
   if(FAILED(pDestinationTargetSurface->UnlockRect()))
		return NULL;
 

   dh::Log( "l %x", GetLastError() );

   //Release Resources
   pRenderTargetSurface->Release();
   pDestinationTargetSurface->Release();

   DeleteDC( hCaptureDC );
   ReleaseDC( GetDesktopWindow(), hdcDesktop );

   dh::Log( "m %x", GetLastError() );
   
   return hbm;
}



#include <d3dx9.h>



LRESULT CXtreme::WindowProc( UINT uMsg, WPARAM wParam, LPARAM lParam )
{
  switch ( uMsg )
  {
    case WM_CREATE:
      {
        GUIButton*   pButton = new GUIButton( 600, 440, 40, 40, "OK" );
        GUIComponentDisplayer::Instance().Add( pButton );

        if ( m_pInputClass )
        {
          m_pInputClass->AddListener( &GUIComponentDisplayer::Instance() );
        }
      }
      break;
    case WM_PAINT:
      if ( m_pRenderer2d )
      {
        if ( m_pRenderer2d->BeginScene() )
        {
          DisplayFrame2d( *m_pRenderer2d );

          if ( m_pInputClass )
          {
            RenderCustomCursor( m_pInputClass->MousePos() );
          }

          if ( m_ConsoleVisible )
          {
            DisplayConsole();
          }

          m_pRenderer2d->EndScene();
          m_pRenderer2d->PresentScene();
        }
        ValidateRect( m_RenderFrame.m_hwndMain, NULL );
        return 0;
      }
    case WM_CHAR:
      switch ( (char)wParam )
      {
        case 'a':
          if ( m_pRenderClass != NULL )
          {
            m_pRenderClass->Offset( GR::tPoint( -20, 0 ) );
          }
          break;
        case 'd':
          if ( m_pRenderClass != NULL )
          {
            m_pRenderClass->Offset( GR::tPoint( 20, 0 ) );
          }
          break;
        case 'p':
          ++m_NumParticles;
          break;
        case 'P':
          if ( m_NumParticles > 0 )
          {
            --m_NumParticles;
          }
          break;
        case '3':
          m_DisplayMode = (DisplayMode)( m_DisplayMode + 1 );
          if ( m_DisplayMode == DM_LAST_ENTRY )
          {
            m_DisplayMode = DM_FIRST_ENTRY;
          }
          break;
        case '+':
          switch ( m_DisplayMode )
          {
            case DM_LINE_CACHE_TEST:
              ++m_NumLines;
              break;
          }
          break;
        case '-':
          switch ( m_DisplayMode )
          {
            case DM_LINE_CACHE_TEST:
              if ( m_NumLines > 0 )
              {
                --m_NumLines;
              }
              break;
          }
          break;
        case 'v':
          if ( m_pRenderClass != NULL )
          {
            m_pRenderClass->EnableVSync( !m_pRenderClass->VSyncEnabled() );
          }
          break;
      }
      break;
    case WM_COMMAND:
      if ( HIWORD( wParam ) == 0 )
      {
        switch ( LOWORD( wParam ) )
        {
          case ID_TEST_SOUND:
            if ( m_pSoundClass )
            {
              m_pSoundClass->Play( m_SoundClick );
            }

            /*
            {


              D3DVERTEXELEMENT9     array[65];

              memset( &array, 0, sizeof( array ) );

              D3DXDeclaratorFromFVF( D3DFVF_XYZRHW, &array[0] );

              dh::Log( "lsmf" );
            }

            {
              HBITMAP hbm = ScreenGrab();

              HWND    hwndTemp = CreateWindow( "STATIC", "Hurz", WS_VISIBLE | SS_BITMAP, 0, 0, 400, 400, NULL, 0, GetModuleHandle( 0 ), 0 );
              SendMessage( hwndTemp, STM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hbm );
            }*/
            break;
          case ID_XTREME_EXIT:
            ShutDown();
            break;
          case ID_MUSIC_DX8OGGFROMFILE:
            SwitchMusic( CMisc::AppPath( "dx8oggplayer.dll" ).c_str() );
            if ( m_pMusicPlayer->LoadMusic( CMisc::AppPath( "ingame.ogg" ).c_str() ) )
            {
              m_pMusicPlayer->Play();
            }
            break;
          case ID_MUSIC_DX8OGGFROMRESOURCE:
            SwitchMusic( CMisc::AppPath( "dx8oggplayer.dll" ).c_str() );
            if ( m_pMusicPlayer->LoadMusic( Win::Util::MemoryStreamFromResource( GetModuleHandle( NULL ), MAKEINTRESOURCE( IDR_OGG ), "BINARY" ) ) )
            {
              m_pMusicPlayer->Play();
            }
            break;
          case ID_MUSIC_NONE:
            SwitchMusic();
            break;
          case ID_2DRENDERER_DDRAW:
            m_NextRenderer2d = "ddrawrenderer.dll";
            break;
          case ID_2DRENDERER_DIRECTX82D:
            m_NextRenderer2d = "DX82dRenderer.dll";
            break;
          case ID_2DRENDERER_SDL:
            m_NextRenderer2d = "SDLRenderer.dll";
            break;
          case ID_2DRENDERER_NONE:
            m_NextRenderer2d = "null";
            break;
          case ID_RENDERER_TOGGLEFULLSCREEN:
            if ( m_pRenderClass != NULL )
            {
              m_pRenderClass->ToggleFullscreen();
            }
            break;
          case ID_RENDERER_SET800X600:
            if ( m_pRenderClass != NULL )
            {
              XRendererDisplayMode    mode;

              mode.Width = 800;
              mode.Height = 600;
              mode.ImageFormat = GR::Graphic::IF_A8R8G8B8;

              m_pRenderClass->SetMode( mode );
            }
            break;
          case ID_RENDERER_SETNEXTMODE:
            if ( m_pRenderClass != NULL )
            {
              XRendererDisplayMode      modeList[256];

              m_pRenderClass->ListDisplayModes( &modeList[0], sizeof( modeList ) );

              for ( int i = 0; i < 256; ++i )
              {
                auto& mode( modeList[i] );

                if ( ( mode.Width == m_pRenderClass->Width() )
                &&   ( mode.Height == m_pRenderClass->Height() ) )
                {
                  // find next mode
                  for ( int j = i; j < 256; ++j )
                  {
                    auto& mode2( modeList[j] );

                    if ( ( mode2.Width != mode.Width )
                    ||   ( mode2.Height != mode.Height ) )
                    {
                      theApp.ChangeWindowSize( mode2.Width, mode2.Height, 32 );// mode2.ImageFormat
                      //m_pRenderClass->SetMode( mode2 );
                      break;
                    }
                  }
                  break;
                }
              }

              /*
              XRendererDisplayMode    mode;

              mode.Width = 800;
              mode.Height = 600;
              mode.ImageFormat = GR::Graphic::IF_A8R8G8B8;

              m_pRenderClass->SetMode( mode );*/
            }
            break;
          case ID_RENDERER_DIRECTX8DIREKT:
            /*
            SwitchRenderer();
            m_pRenderClass = new CDX8RenderClass();
            RestoreData();
            */
            break;
          case ID_RENDERER_DIRECTX8:
            //m_DebugService.LogEnable( "Renderer.Full" );
            m_NextRenderer = "dx8renderer.dll";
            m_pVBTest = NULL;
            break;
          case ID_RENDERER_DIRECTX9:
            m_NextRenderer = "dx9renderer.dll";
            m_pVBTest = NULL;
            break;
          case ID_RENDERER_DIRECTX9SHADER:
            m_NextRenderer = "dx9shaderrenderer.dll";
            m_pVBTest = NULL;
            break;
          case ID_RENDERER_DIRECTX11:
            m_NextRenderer = "dx11renderer.dll";
            m_pVBTest = NULL;
            break;
          case ID_RENDERER_OPENGL:
            m_NextRenderer = "openglrenderer.dll";
            m_pVBTest = NULL;
            break;
          case ID_RENDERER_VULKAN:
            m_NextRenderer = "VulkanRenderer.dll";
            m_pVBTest = NULL;
            break;
          case ID_RENDERER_NONE:
            m_NextRenderer = "null";
            m_pVBTest = NULL;
            InvalidateWindow();
            break;
          case ID_SOUND_DIRECTX8:
            SwitchSound( CMisc::AppPath( "dx8sound.dll" ).c_str() );
            break;
          case ID_SOUND_XAUDIO2:
            SwitchSound( CMisc::AppPath( "xaudio2sound.dll" ).c_str() );
            break;
          case ID_SOUND_NONE:
            SwitchSound();
            break;
          case ID_INPUT_WININPUT:
            m_NewInput = CMisc::AppPath( "wininput.dll" );
            break;
          case ID_INPUT_DXINPUT:
            m_NewInput = CMisc::AppPath( "dxinput.dll" );
            break;
          case ID_INPUT_RAWINPUT:
            m_NewInput = CMisc::AppPath( "rawinput.dll" );
            break;
          case ID_INPUT_NONE:
            m_NewInput = "none";
            break;
        }
      }
      break;
  }
  return XFrameApp::WindowProc( uMsg, wParam, lParam );
}



bool CXtreme::SwitchRenderer2d( const char* szFileName )
{
  if ( m_pRenderer2d )
  {
    m_pRenderer2d->Release();
    GR::Service::Environment::Instance().RemoveService( "Renderer" );
  }
  m_pRenderer2d = NULL;
#if OPERATING_SUB_SYSTEM == OS_SUB_DESKTOP
  if ( m_hinstCurrentRenderer )
  {
    FreeLibrary( m_hinstCurrentRenderer );
    m_hinstCurrentRenderer = NULL;
  }
#endif

  if ( szFileName == NULL )
  {
    EventProducer<GR::Gamebase::tXFrameEvent>::SendEvent( GR::Gamebase::tXFrameEvent( GR::Gamebase::tXFrameEvent::ET_RENDERER_SWITCHED ) );
    return false;
  }

#if OPERATING_SUB_SYSTEM == OS_SUB_DESKTOP
  m_hinstCurrentRenderer = LoadLibraryA( szFileName );
  if ( m_hinstCurrentRenderer == NULL )
  {
    m_hinstCurrentRenderer = LoadLibraryA( AppPath( szFileName ).c_str() );
  }
  if ( m_hinstCurrentRenderer )
  {
    typedef X2dRenderer* ( *tIFunction )( void );
    tIFunction    fGetInterface = (tIFunction)GetProcAddress( m_hinstCurrentRenderer, "GetInterface" );

    if ( fGetInterface )
    {
      m_pRenderer2d = fGetInterface();

      GR::Service::Environment::Instance().SetService( "Renderer", m_pRenderer2d );
    }
  }
#endif
  EventProducer<GR::Gamebase::tXFrameEvent>::SendEvent( GR::Gamebase::tXFrameEvent( GR::Gamebase::tXFrameEvent::ET_RENDERER_SWITCHED ) );
  if ( m_pRenderer2d )
  {
    m_RenderFrame.SendEvent( GR::tRenderFrameEvent( GR::tRenderFrameEvent::RFE_WINDOW_SIZE_CHANGED, 0, GR::tPoint( m_pRenderer2d->Width(), m_pRenderer2d->Height() ) ) );
  }
  return true;
}