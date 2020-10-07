// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the DX8SOUND_EXPORTS
// symbol defined on the command line. this symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// DX8SOUND_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef DX8SOUND_EXPORTS
#define DX8SOUND_API __declspec(dllexport)
#else
#define DX8SOUND_API __declspec(dllimport)
#endif

#include <Xtreme/Xtreme.h>

extern "C" DX8SOUND_API XSound* GetInterface();

extern "C" DX8SOUND_API Xtreme::ePluginType GetInterfaceType();