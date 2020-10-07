#ifdef DX8MPLAYER_EXPORTS
#define DX8SOUND_API __declspec(dllexport)
#else
#define DX8SOUND_API __declspec(dllimport)
#endif



#include <Xtreme/Xtreme.h>

extern "C" DX8SOUND_API XMusic* GetInterface();

extern "C" DX8SOUND_API Xtreme::ePluginType GetInterfaceType();