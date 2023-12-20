#include <windows.h>
#include <cstdint>
#include <Xinput.h>

#define local_persist static
#define global_variable static
#define internal static

typedef uint8_t uint8;
typedef uint32_t uint32;

global_variable bool W_STATE;
global_variable bool A_STATE;
global_variable bool S_STATE;
global_variable bool D_STATE;

struct win32_offscreen_buffer
{
    BITMAPINFO Info;
    void *Memory;
    int Width;
    int Height;
    int Pitch;
};

#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_GET_STATE(x_input_get_state);
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
    return 0;
}
X_INPUT_SET_STATE(XInputSetStateStub)
{
    return 0;
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub;
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputGetState XInputGetState_
#define XInputSetState XInputSetState_

internal void Win32LoadXInput(void)
{
    HMODULE XInputLibrary = LoadLibraryA("xinput_1_4.dll");
    if (XInputLibrary)
    {
        XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
        XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
    }
}

global_variable bool GlobalRunning;
global_variable win32_offscreen_buffer GlobalBackBuffer;

struct win32_window_dimension
{
    int Width;
    int Height;
};

internal win32_window_dimension Win32GetWindowDimension(HWND Window)
{
    win32_window_dimension Result;
    RECT ClientRect;
    GetClientRect(Window, &ClientRect);
    Result.Width = ClientRect.right - ClientRect.left;
    Result.Height = ClientRect.bottom - ClientRect.top;
    return Result;
}

internal void RenderWeirdGradient(win32_offscreen_buffer *Buffer, int XOffset, int YOffset)
{
    uint8 *Row = (uint8 *)Buffer->Memory;
    for (int Y = 0; Y < Buffer->Height; ++Y)
    {
        uint32 *Pixel = (uint32 *)Row;
        for (int X = 0; X < Buffer->Width; ++X)
        {
            uint8 Red = 0;
            uint8 Green = (uint8)(X + XOffset);
            uint8 Blue = (uint8)(Y + YOffset);
            *Pixel++ = ((Red << 16) | (Green << 8) | Blue);
        }
        Row += Buffer->Pitch;
    }
}

internal void Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height)
{
    if (Buffer->Memory)
    {
        VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
    }
    Buffer->Width = Width;
    Buffer->Height = Height;
    int BytesPerPixel = 4;
    Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
    Buffer->Info.bmiHeader.biWidth = Buffer->Width;
    Buffer->Info.bmiHeader.biHeight = -Buffer->Height;
    Buffer->Info.bmiHeader.biPlanes = 1;
    Buffer->Info.bmiHeader.biBitCount = 32;
    Buffer->Info.bmiHeader.biCompression = BI_RGB;

    int BitmapMemorySize = BytesPerPixel * Buffer->Width * Buffer->Height;
    Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_COMMIT, PAGE_READWRITE);
    Buffer->Pitch = Width * BytesPerPixel;
}

internal void Win32DisplayBufferInWindow(HDC DeviceContext, int WindowWidth, int WindowHeight,
                                         win32_offscreen_buffer *Buffer)
{
    StretchDIBits(DeviceContext, 0, 0, WindowWidth, WindowHeight, 0, 0, Buffer->Width, Buffer->Height, Buffer->Memory, &Buffer->Info, DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK Win32WindowProc(HWND Window, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CLOSE:
    {
        GlobalRunning = false;
        PostQuitMessage(0);
        return 0;
    }
    break;
    case WM_DESTROY:
    {
        GlobalRunning = false;
        PostQuitMessage(0);
        return 0;
    }
    break;
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYDOWN:
    case WM_KEYUP:
    {
        uint32 VKCode = wParam;
        bool IsDown = ((lParam & (1 << 30)) != 0);
        bool WasDown = ((lParam & (1 << 31)) == 0);
        if (IsDown != WasDown)
        {
            if (VKCode == 0x57) // W Key
            {
                W_STATE = true;
                S_STATE = false;
            }
            else if (VKCode == 0x53) // S Key
            {
                W_STATE = false;
                S_STATE = true;
            }
            else if (VKCode == 0x41) // A Key
            {
                A_STATE = true;
                D_STATE = false;
            }
            else if (VKCode == 0x44) // D Key
            {
                D_STATE = true;
                A_STATE = false;
            }
            else
            {
                W_STATE = false;
                A_STATE = false;
                D_STATE = false;
                S_STATE = false;
            }
        }
    }
    break;
    case WM_QUIT:
    {
        GlobalRunning = false;
        PostQuitMessage(0);
        return 0;
    }
    case WM_SIZE:
    {
    }
    break;
    case WM_PAINT:
    {
        PAINTSTRUCT Paint;
        // All painting occurs here, between BeginPaint and EndPaint.
        HDC DeviceContext = BeginPaint(Window, &Paint);
        win32_window_dimension Dimension = Win32GetWindowDimension(Window);
        Win32DisplayBufferInWindow(DeviceContext, Dimension.Width, Dimension.Height, &GlobalBackBuffer);

        EndPaint(Window, &Paint);
    }
    break;
    }
    return DefWindowProc(Window, uMsg, wParam, lParam);
}

int CALLBACK WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, PSTR CommandLine, int ShowCommand)
{
    // Register the window class.

    WNDCLASS wc = {}; // Zero out the memory.

    Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);

    wc.lpfnWndProc = Win32WindowProc;
    wc.hInstance = Instance;
    wc.lpszClassName = TEXT("Handmade");
    wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW; // Redraw on size, and own DC for window.

    RegisterClass(&wc);

    // Create the window.

    HWND hwnd = CreateWindowEx(
        0,                      // Optional window styles.
        wc.lpszClassName,       // Window class
        TEXT("Handemade Hero"), // Window text
        WS_OVERLAPPEDWINDOW,    // Window style

        // Size and position
        CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,

        NULL,     // Parent window
        NULL,     // Menu
        Instance, // Instance handle
        NULL      // Additional application data
    );

    if (hwnd == NULL)
    {
        return 0;
    }

    ShowWindow(hwnd, ShowCommand);

    GlobalRunning = true;
    int XOffset = 0;
    int YOffset = 0;
    while (GlobalRunning)
    {

        MSG msg = {};
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (msg.message == WM_QUIT)
            {
                GlobalRunning = false;
            }
            // Translate virtual-key messages into character messages.
            TranslateMessage(&msg);
            // Send message to WindowProc.
            DispatchMessage(&msg);
        }
        for (int i = 0; i < XUSER_MAX_COUNT; i++)
        {
            XINPUT_STATE ControllerState;
            if (XInputGetState(i, &ControllerState) == ERROR_SUCCESS)
            {
                // Is plugged in
                XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;
                bool Up = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
                bool Down = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
                bool Left = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
                bool Right = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
                bool Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
                bool Back = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
                bool LeftShoulder = (Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
                bool RightShoulder = (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
                bool XButton = (Pad->wButtons & XINPUT_GAMEPAD_X);
                bool YButton = (Pad->wButtons & XINPUT_GAMEPAD_Y);
                bool AButton = (Pad->wButtons & XINPUT_GAMEPAD_A);
                bool BButton = (Pad->wButtons & XINPUT_GAMEPAD_B);

                int16_t StickX = Pad->sThumbLX;
                int16_t StickY = Pad->sThumbLY;
            }
            else
            {
                // Controller not available
            }
        }
        RenderWeirdGradient(&GlobalBackBuffer, XOffset, YOffset);
        HDC DeviceContext = GetDC(hwnd);
        RECT ClientRect;
        win32_window_dimension Dimension = Win32GetWindowDimension(hwnd);
        Win32DisplayBufferInWindow(DeviceContext, Dimension.Width,
                                   Dimension.Height, &GlobalBackBuffer);
        ReleaseDC(hwnd, DeviceContext);

        if (W_STATE)
        {
            ++YOffset;
        }
        if (S_STATE)
        {
            --YOffset;
        }
        if (A_STATE)
        {
            ++XOffset;
        }
        if (D_STATE)
        {
            --XOffset;
        }
    }

    return 0;
}
