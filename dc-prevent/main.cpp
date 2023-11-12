#define TEST_MODE

#include <windows.h>
#include <iostream>
#include <format>
#include "resource.h"

const std::wstring mutexName = L"DC_Instance";

static bool enableDcPrevent = true;
static HMENU hMenu;

enum ItemID : int 
{
  TOGGLE = 1,
  CONSOLE,
  EXIT,
};

LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
  switch (msg)
  {
  case WM_CREATE:
  {
    // Create the tray icon
    //
    NOTIFYICONDATA nid = { 0 };

    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_USER + 1;

    // Load the icon from resources
    //
    nid.hIcon = LoadIcon(GetModuleHandle(NULL), MAKEINTRESOURCE(IDI_ICON1));

    wcscpy_s(nid.szTip, sizeof(nid.szTip) / sizeof(wchar_t), L"DC Prevent Tray");

    Shell_NotifyIcon(NIM_ADD, &nid);

    // Create the context menu
    //
    hMenu = CreatePopupMenu();

    AppendMenu(hMenu, MF_CHECKED, 1, L"Prevent double clicks");
    AppendMenu(hMenu, MF_CHECKED, 2, L"Toggle debug console");
    AppendMenu(hMenu, MF_SEPARATOR, 0, NULL); // Add a separator
    AppendMenu(hMenu, MF_STRING, 3, L"Exit (build 1.0)");

    break;
  }
  case WM_USER + 1:
  {
    // Handle only the right mouse click on the tray icon
    //
    if (lParam != WM_RBUTTONUP)
      return DefWindowProcW(hwnd, msg, wParam, lParam);

    POINT pt{ 0 }; GetCursorPos(&pt); SetForegroundWindow(hwnd);

    int selected = TrackPopupMenu(hMenu, TPM_LEFTALIGN | TPM_RETURNCMD, pt.x, pt.y, 0, hwnd, NULL);

    if (selected == ItemID::TOGGLE)
    {
      // Update the check mark based on the state of the enableDcPrevent boolean
      //
      enableDcPrevent = !enableDcPrevent;

      CheckMenuItem(hMenu, 1, MF_BYCOMMAND | (enableDcPrevent ? MF_CHECKED : MF_UNCHECKED));

      std::cout << std::format("DC Prevent is now: {}", enableDcPrevent ? "enabled" : "disabled") << '\n';
    }

    if (selected == ItemID::EXIT)
    {
      // Terminate the program
      //
      PostQuitMessage(0);
    }

    break;
  }
  case WM_DESTROY:
  {
    // Remove the tray icon
    //
    NOTIFYICONDATA nid = { 0 };

    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = 1;
    Shell_NotifyIcon(NIM_DELETE, &nid);

    PostQuitMessage(0);
  }
  default:
    return DefWindowProcW(hwnd, msg, wParam, lParam);
  }

  return 0;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
#ifdef TEST_MODE
  FILE* p_file = nullptr;

  AllocConsole();

  freopen_s(&p_file, "CONOUT$", "w", stdout);
  freopen_s(&p_file, "CONIN$", "r", stdin);
#endif

  HANDLE mutex = CreateMutex(nullptr, TRUE, mutexName.c_str());

  if (GetLastError() == ERROR_ALREADY_EXISTS) 
  {
    MessageBox(nullptr, L"An instance of DC Prevent is already running", L"Error", MB_ICONEXCLAMATION);
    return -8;
  }

  const auto windowClassName = L"Duels DC Prevent";

  WNDCLASSEX wc = { 0 };

  // Set cbSize to the size of the WNDCLASSEX structure
  //
  wc.cbSize = sizeof(WNDCLASSEX);  
  wc.lpfnWndProc = WindowProcedure;
  wc.hInstance = hInstance;
  wc.lpszClassName = windowClassName;

  RegisterClassExW(&wc);

  // Create a message-only window
  //
  HWND hwndMessage = CreateWindowExW(0, windowClassName, NULL, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, hInstance, NULL);

  if (hwndMessage == NULL)
  {
    DWORD error = GetLastError();

    MessageBox(hwndMessage, 
      std::format(L"CreateWindowExW failed with error code {}", error).c_str(), 
      L"Info",
      MB_OK);
    
    return 0;
  }

  std::printf("Tray successfully created!\n\n");

  MSG msg = { 0 };

  while (GetMessage(&msg, NULL, 0, 0))
  {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
}
