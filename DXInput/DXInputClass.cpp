#include <Xtreme/XInput.h>
#include <Xtreme/Input/DXInput.h>

#include <Xtreme/Xtreme.h>

#include "DXInputClass.h"



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
      g_pInput = new CDXInput;
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



DXINPUT_API Xtreme::XInput* GetInterface()
{
  return g_pInput; 
}



DXINPUT_API Xtreme::ePluginType GetInterfaceType()
{
  return Xtreme::XPLUG_INPUT;
}


