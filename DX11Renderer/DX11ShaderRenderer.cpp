#include <debug/debugclient.h>

#include <Xtreme/Xtreme.h>
#include <Xtreme/XRenderer.h>

#include <Xtreme/DX11/DX11Renderer.h>



extern "C" __declspec( dllexport ) XRenderer* GetInterface();
extern "C" __declspec( dllexport ) Xtreme::ePluginType GetInterfaceType();



static DX11Renderer*      pDX11RendererInstance = NULL;



BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved )
{

  switch ( ul_reason_for_call )
  {
    case DLL_PROCESS_ATTACH:  // The DLL is being mapped into the
                              // process's address space
      //DisableThreadLibraryCalls( (HMODULE)hModule );

      pDX11RendererInstance = new DX11Renderer( (HINSTANCE)hModule );
      // This tells the system we don't want
      // DLL_THREAD_ATTACH and DLL_THREAD_DETACH
      // modifications sent to the specified
      // DLL's DllMain function
      break;
    case DLL_PROCESS_DETACH:  // The DLL is being unmapped from the
                              // process address space

      delete pDX11RendererInstance;
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
  return pDX11RendererInstance; 
}


Xtreme::ePluginType GetInterfaceType()
{
  return Xtreme::XPLUG_RENDERER;
}


