#include <windows.h>
#include <cstdint>

#define local_persist static
#define global_variable static
#define internal static

typedef uint8_t uint8;
typedef uint32_t uint32;

struct win32_offscreen_buffer
{
    BITMAPINFO Info;
    void *Memory;
    int Width;
    int Height;
    int Pitch;
};

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

internal void RenderWeirdGradient(win32_offscreen_buffer Buffer, int XOffset, int YOffset)
{
    uint8 *Row = (uint8 *)Buffer.Memory;
    for (int Y = 0; Y < Buffer.Height; ++Y)
    {
        uint32 *Pixel = (uint32 *)Row;
        for (int X = 0; X < Buffer.Width; ++X)
        {
            uint8 Red = 0;
            uint8 Green = (uint8)(X + XOffset);
            uint8 Blue = (uint8)(Y + YOffset);
            *Pixel++ = ((Red << 16) | (Green << 8) | Blue);
        }
        Row += Buffer.Pitch;
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
                                         win32_offscreen_buffer Buffer)
{
    StretchDIBits(DeviceContext, 0, 0, WindowWidth, WindowHeight, 0, 0, Buffer.Width, Buffer.Height, Buffer.Memory, &Buffer.Info, DIB_RGB_COLORS, SRCCOPY);
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
        Win32DisplayBufferInWindow(DeviceContext, Dimension.Width, Dimension.Height, GlobalBackBuffer);

        EndPaint(Window, &Paint);
    }
    break;
    }
    return DefWindowProc(Window, uMsg, wParam, lParam);
}

int CALLBACK WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, PSTR CommandLine, int ShowCommand)
{
    // Register the window class.

    WNDCLASS wc = {};

    Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);

    wc.lpfnWndProc = Win32WindowProc;
    wc.hInstance = Instance;
    wc.lpszClassName = TEXT("Handmade");
    wc.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;

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
        RenderWeirdGradient(GlobalBackBuffer, XOffset, YOffset);
        HDC DeviceContext = GetDC(hwnd);
        RECT ClientRect;
        win32_window_dimension Dimension = Win32GetWindowDimension(hwnd);
        Win32DisplayBufferInWindow(DeviceContext, Dimension.Width,
                                   Dimension.Height, GlobalBackBuffer);
        ReleaseDC(hwnd, DeviceContext);

        ++XOffset;
        YOffset += 2;
    }

    return 0;
}