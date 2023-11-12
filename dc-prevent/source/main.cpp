#define TEST_MODE

#include <windows.h>
#include <iostream>
#include <format>

#include "../resource/resource.h"

const std::wstring mutexName = L"DC_Instance";

static bool enableDcPrevent = true;
static bool showDebugConsole = true;
static HMENU hMenu;

enum ItemID : int 
{
  TOGGLE = 1,
  CONSOLE,
  EXIT,
};

// Global variable to hold the last click time
//
static DWORD lastClickTime = 0;
static std::atomic<bool> shouldBlockClick{};
static HHOOK hMouseHook{};

LRESULT CALLBACK MouseHookCallback(int nCode, WPARAM wParam, LPARAM lParam)
{
  if (nCode == HC_ACTION)
  {
    if (shouldBlockClick)
    {
      shouldBlockClick = false;
      return -1;
    }
  }

  return CallNextHookEx(hMouseHook, nCode, wParam, lParam);
}

LRESULT CALLBACK RawInputCallback(LPARAM lParam)
{
  if (!enableDcPrevent)
    return 0;

  RAWINPUT rawInput[sizeof(RAWINPUT)]{};
  UINT size = sizeof(RAWINPUT);

  GetRawInputData(
    (HRAWINPUT)lParam, 
    RID_INPUT, 
    rawInput,
    &size, 
    sizeof(RAWINPUTHEADER)
  );

  if (rawInput->header.dwType != RIM_TYPEMOUSE)
    return 0;

  RAWMOUSE* rawMouse = &rawInput->data.mouse;

  if (rawMouse->usButtonFlags & RI_MOUSE_LEFT_BUTTON_DOWN)
  {
    DWORD64 currentTime = GetTickCount64();

    if (currentTime - lastClickTime < 50)
    {
      // Disregard the click if it was within 50ms of the last click
      //
      std::cout << "Double click detected, disregarding\n";
      shouldBlockClick = true;

      return -1;
    }

    lastClickTime = currentTime;
  }
}

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
  case WM_INPUT:
  {
    return RawInputCallback(lParam);
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

      CheckMenuItem(
        hMenu, 
        ItemID::TOGGLE, 
        MF_BYCOMMAND | (enableDcPrevent ? MF_CHECKED : MF_UNCHECKED)
      );

      std::cout << std::format("DC Prevent is now: {}\n",
        enableDcPrevent ? "enabled" : "disabled");
    }

    if (selected == ItemID::CONSOLE)
    {
      showDebugConsole = !showDebugConsole;

      CheckMenuItem(
        hMenu, 
        ItemID::CONSOLE, 
        MF_BYCOMMAND | (showDebugConsole ? MF_CHECKED : MF_UNCHECKED)
      );

      ShowWindow(GetConsoleWindow(), showDebugConsole ? SW_SHOW : SW_HIDE);
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

#define HID_USAGE_PAGE_GENERIC  0x1
#define HID_USAGE_GENERIC_MOUSE 0x2

bool RegisterRawInput(HWND hwnd)
{
  // Initialize our mouse hook
  //
  hMouseHook = SetWindowsHookEx(
    WH_MOUSE_LL, 
    MouseHookCallback, 
    GetModuleHandleA(0), 
    NULL
  );

  RAWINPUTDEVICE rid[1];

  // Register the mouse device
  //
  rid[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
  rid[0].usUsage = HID_USAGE_GENERIC_MOUSE;
  rid[0].dwFlags = RIDEV_INPUTSINK;
  rid[0].hwndTarget = hwnd;

  return RegisterRawInputDevices(rid, 1, sizeof(rid[0]));
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
  HANDLE mutex = CreateMutex(nullptr, TRUE, mutexName.c_str());

  if (GetLastError() == ERROR_ALREADY_EXISTS) 
  {
    MessageBox(nullptr, L"An instance of DC Prevent is already running", L"Error", MB_ICONEXCLAMATION);
    return -8;
  }

  // Allocate our console
  //
  static FILE* file{};

  AllocConsole();

  freopen_s(&file, "CONOUT$", "w", stdout);
  freopen_s(&file, "CONIN$", "r", stdin);

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

    MessageBox(nullptr,
      std::format(L"CreateWindowExW failed with error code {}", error).c_str(), 
      L"Info",
      MB_OK);
    
    return -1;
  }

  std::printf("Tray successfully created!\n");

  if (!RegisterRawInput(hwndMessage))
  {
    MessageBox(nullptr,
      std::format(L"Failed to register Raw Input\n").c_str(),
      L"Info",
      MB_OK);

    return -1;
  }

  std::printf("Raw input registered!\n\n");

  MSG msg = { 0 };

  while (GetMessage(&msg, NULL, 0, 0))
  {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
}
