// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the DX8SOUND_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// DX8SOUND_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef RAWINPUT_EXPORTS
#define RAWINPUT_API __declspec(dllexport)
#else
#define RAWINPUT_API __declspec(dllimport)
#endif

#include <Xtreme/Xtreme.h>

extern "C" RAWINPUT_API Xtreme::XInput* GetInterface();

extern "C" RAWINPUT_API Xtreme::ePluginType GetInterfaceType();