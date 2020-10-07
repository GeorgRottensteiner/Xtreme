#include <Xtreme/Xtreme.h>
#include <Xtreme/XMusic.h>
 
#include "DX8OggPlayer.h"
#include "OggDSound.h"



#pragma comment( lib, "dxguid.lib" )


CDirectSoundOgg*     g_pPlayer = NULL;



BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved )
{

  switch ( ul_reason_for_call )
  {
    case DLL_PROCESS_ATTACH:  // The DLL is being mapped into the
                              // process's address space
      // This tells the system we don't want
      // DLL_THREAD_ATTACH and DLL_THREAD_DETACH
      // modifications sent to the specified
      // DLL's DllMain function
      g_pPlayer = new CDirectSoundOgg();
      break;
    case DLL_PROCESS_DETACH:  // The DLL is being unmapped from the
                              // process address space
      delete g_pPlayer;
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



XMusic* GetInterface()
{

  return g_pPlayer; 

}


Xtreme::ePluginType GetInterfaceType()
{

  return Xtreme::XPLUG_MUSIC;

}


