#include <debug/debugclient.h>

#include <Xtreme/Xtreme.h>
#include <Xtreme/XRenderer.h>

#include <Xtreme/OpenGLShader/OpenGLShaderRenderClass.h>



extern "C" __declspec( dllexport ) XRenderer* GetInterface();
extern "C" __declspec( dllexport ) Xtreme::ePluginType GetInterfaceType();



static OpenGLShaderRenderClass* pOpenGLRendererInstance = NULL;



BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved )
{

  switch ( ul_reason_for_call )
  {
    case DLL_PROCESS_ATTACH:  // The DLL is being mapped into the
                              // process's address space
      //DisableThreadLibraryCalls( (HMODULE)hModule );

      pOpenGLRendererInstance = new OpenGLShaderRenderClass();
      // This tells the system we don't want
      // DLL_THREAD_ATTACH and DLL_THREAD_DETACH
      // modifications sent to the specified
      // DLL's DllMain function
      break;
    case DLL_PROCESS_DETACH:  // The DLL is being unmapped from the
                              // process address space

      delete pOpenGLRendererInstance;
      break;
    case DLL_THREAD_ATTACH:  // A thread is being created
      break;
    case DLL_THREAD_DETACH:  // A thread is exiting cleanly
      break;
    default:
      break;
  }
  return TRUE;
}



XRenderer* GetInterface()
{
  return pOpenGLRendererInstance; 
}


Xtreme::ePluginType GetInterfaceType()
{
  return Xtreme::XPLUG_RENDERER;
}


