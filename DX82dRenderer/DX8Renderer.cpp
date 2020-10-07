#include <debug/debugclient.h>

#include <Xtreme/Xtreme.h>
#include <Xtreme/X2dRenderer.h>

#include <Xtreme/DX82d/DX82dRenderClass.h>



extern "C" __declspec( dllexport ) X2dRenderer* GetInterface();
extern "C" __declspec( dllexport ) Xtreme::ePluginType GetInterfaceType();


static DX82dRenderer* pDX82dRendererInstance = NULL;


BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved )
{
  switch ( ul_reason_for_call )
  {
    case DLL_PROCESS_ATTACH:  // The DLL is being mapped into the
                              // process's address space
      pDX82dRendererInstance = new DX82dRenderer( (HINSTANCE)hModule );
      // This tells the system we don't want
      // DLL_THREAD_ATTACH and DLL_THREAD_DETACH
      // modifications sent to the specified
      // DLL's DllMain function
      break;
    case DLL_PROCESS_DETACH:  // The DLL is being unmapped from the
                              // process address space
      delete pDX82dRendererInstance;
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



X2dRenderer* GetInterface()
{
  return pDX82dRendererInstance; 
}



Xtreme::ePluginType GetInterfaceType()
{
  return Xtreme::XPLUG_2D_RENDERER;
}


