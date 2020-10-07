#include <Xtreme/XInput.h>
#include <Xtreme/Input/RawInput.h>

#include <Xtreme/Xtreme.h>

#include "RawInputClass.h"



Xtreme::XInput*     g_pInput = NULL;


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
      g_pInput = new RawInput;
      break;
    case DLL_PROCESS_DETACH:  // The DLL is being unmapped from the
                              // process address space
      delete g_pInput;
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



RAWINPUT_API Xtreme::XInput* GetInterface()
{
  return g_pInput; 
}



RAWINPUT_API Xtreme::ePluginType GetInterfaceType()
{
  return Xtreme::XPLUG_INPUT;
}


