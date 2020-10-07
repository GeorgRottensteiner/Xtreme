#include <Xtreme/Xtreme.h>

#include <Xtreme/Audio/XAudio2Sound.h>

#include "XAudio2SoundDll.h"



#include <Windows.h>



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
      Xtreme::XAudio2Sound::Instance();
      break;
    case DLL_PROCESS_DETACH:  // The DLL is being unmapped from the
                              // process address space

      Xtreme::XAudio2Sound::Instance().Release();
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



XAUDIO2SOUND_API XSound* GetInterface()
{
  return &Xtreme::XAudio2Sound::Instance();
}


XAUDIO2SOUND_API Xtreme::ePluginType GetInterfaceType()
{
  return Xtreme::XPLUG_SOUND;
}


