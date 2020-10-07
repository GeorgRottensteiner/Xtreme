#include "Xtreme.h"

#include "resource.h"



int WINAPI WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow )
{

  theApp.Create( WS_VISIBLE | WS_OVERLAPPED | WS_SYSMENU | WS_THICKFRAME, 640, 480, "Xtreme", 0, 0, IDR_MENU_MAIN );

  return theApp.Run();

}
