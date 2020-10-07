#include <debug/debugclient.h>

#include <Xtreme/Xtreme.h>
#include <Xtreme/X2dRenderer.h>

#include <Xtreme/DDraw/DDrawRenderClass.h>



extern "C" __declspec( dllexport ) X2dRenderer* GetInterface();
extern "C" __declspec( dllexport ) Xtreme::ePluginType GetInterfaceType();



static CDDrawRenderClass* pDDrawRendererInstance = NULL;



BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved )
{

  switch ( ul_reason_for_call )
  {
    case DLL_PROCESS_ATTACH:  // The DLL is being mapped into the
                              // process's address space
      //DisableThreadLibraryCalls( (HMODULE)hModule );

      pDDrawRendererInstance = new CDDrawRenderClass( (HINSTANCE)hModule );
      // This tells the system we don't want
      // DLL_THREAD_ATTACH and DLL_THREAD_DETACH
      // modifications sent to the specified
      // DLL's DllMain function
      break;
    case DLL_PROCESS_DETACH:  // The DLL is being unmapped from the
                              // process address space

      delete pDDrawRendererInstance;
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

  return pDDrawRendererInstance; 

}


Xtreme::ePluginType GetInterfaceType()
{

  return Xtreme::XPLUG_2D_RENDERER;

}


