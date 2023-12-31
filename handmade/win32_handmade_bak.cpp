#include <cassert>
#include <dsound.h>
#include <fstream>
#include <math.h>
#include <objbase.h>
#include <stdint.h>
#include <string>
#include <thread>
#include <vector>
#include <windows.h>
#include <xaudio2.h>

#include "handmade.cpp"
#include <Xinput.h>

#define local_persist static
#define global_variable static
#define internal static

#define Pi32 3.14159265359f

typedef uint8_t uint8;
typedef uint32_t uint32;

global_variable bool W_STATE;
global_variable bool A_STATE;
global_variable bool S_STATE;
global_variable bool D_STATE;
global_variable bool MINUS_STATE;
global_variable bool PLUS_STATE;

extern IXAudio2SourceVoice *GlobalSourceVoice;

LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;

struct win32_offscreen_buffer
{
    BITMAPINFO Info;
    void *Memory;
    int Width;
    int Height;
    int Pitch;
};

#define X_INPUT_GET_STATE(name)                                                \
    DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
#define X_INPUT_SET_STATE(name)                                                \
    DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_GET_STATE(x_input_get_state);
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
    return ERROR_DEVICE_NOT_CONNECTED;
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
        XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary,
                                                             "XInputGetState");
        XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary,
                                                             "XInputSetState");
    }
}

#define DIRECT_SOUND_CREATE(name)                                              \
    HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS,             \
                        LPUNKNOWN pUnkOuter)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

internal void Win32InitDSound(HWND Window, int32_t SamplesPerSecond,
                              int32_t BufferSize)
{
    HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");
    if (DSoundLibrary)
    {
        direct_sound_create *DirectSoundCreate =
            (direct_sound_create *)GetProcAddress(DSoundLibrary,
                                                  "DirectSoundCreate");
        if (DirectSoundCreate)
        {
            LPDIRECTSOUND DirectSound;
            if (DirectSoundCreate(0, &DirectSound, 0) == DS_OK)
            {
                WAVEFORMATEX WaveFormat = {};
                WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
                WaveFormat.nChannels = 2;
                WaveFormat.nSamplesPerSec = SamplesPerSecond;
                WaveFormat.wBitsPerSample = 16;
                WaveFormat.nBlockAlign =
                    (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
                WaveFormat.nAvgBytesPerSec =
                    WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;
                WaveFormat.cbSize = 0;
                if (SUCCEEDED(DirectSound->SetCooperativeLevel(Window,
                                                               DSSCL_PRIORITY)))
                {
                    DSBUFFERDESC BufferDescription = {};
                    BufferDescription.dwSize = sizeof(BufferDescription);
                    BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;
                    BufferDescription.dwBufferBytes = 0;
                    LPDIRECTSOUNDBUFFER PrimaryBuffer;
                    if (DirectSound->CreateSoundBuffer(
                            &BufferDescription, &PrimaryBuffer, 0) == DS_OK)
                    {
                        PrimaryBuffer->SetFormat(&WaveFormat);
                    }
                }
                DSBUFFERDESC BufferDescription = {};
                BufferDescription.dwSize = sizeof(BufferDescription);
                BufferDescription.dwFlags = 0;
                BufferDescription.dwBufferBytes = BufferSize;
                BufferDescription.lpwfxFormat = &WaveFormat;
                if (DirectSound->CreateSoundBuffer(
                        &BufferDescription, &GlobalSecondaryBuffer, 0) == DS_OK)
                {
                    // Start it playing
                }
            }
        }
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

internal void Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width,
                                    int Height)
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
    Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE | MEM_COMMIT,
                                  PAGE_READWRITE);
    Buffer->Pitch = Width * BytesPerPixel;
}

internal void Win32DisplayBufferInWindow(HDC DeviceContext, int WindowWidth,
                                         int WindowHeight,
                                         win32_offscreen_buffer *Buffer)
{
    StretchDIBits(DeviceContext, 0, 0, WindowWidth, WindowHeight, 0, 0,
                  Buffer->Width, Buffer->Height, Buffer->Memory, &Buffer->Info,
                  DIB_RGB_COLORS, SRCCOPY);
}

LRESULT CALLBACK Win32WindowProc(HWND Window, UINT uMsg, WPARAM wParam,
                                 LPARAM lParam)
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
        {
            uint32 VKCode = wParam;
            bool IsDown = ((lParam & (1 << 30)) != 0);
            bool WasDown = ((lParam & (1 << 31)) == 0);
            if (IsDown != WasDown)
            {
                switch (VKCode)
                {
                    case 'W':
                    {
                        W_STATE = true;
                        S_STATE = false;
                    }
                    break;
                    case 'S':
                    {
                        W_STATE = false;
                        S_STATE = true;
                    }
                    break;
                    case 'A':
                    {
                        A_STATE = true;
                        D_STATE = false;
                    }
                    break;
                    case 'D':
                    {
                        D_STATE = true;
                        A_STATE = false;
                    }
                    break;
                    default:
                    {
                        OutputDebugStringA("VKCode: ");
                        OutputDebugStringA(std::to_string(VKCode).c_str());
                        W_STATE = false;
                        A_STATE = false;
                        D_STATE = false;
                        S_STATE = false;
                    }
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
            Win32DisplayBufferInWindow(DeviceContext, Dimension.Width,
                                       Dimension.Height, &GlobalBackBuffer);

            EndPaint(Window, &Paint);
        }
        break;
    }
    return DefWindowProc(Window, uMsg, wParam, lParam);
}

struct win32_sound_output
{
    int SamplesPerSecond;
    int ToneHz;
    int16_t ToneVolume;
    uint32_t RunningSampleIndex;
    int WavePeriod;
    int BytesPerSample;
    int SecondaryBufferSize;
    float tSine;
    int LatencySampleCount;
};

static void Win32ClearSoundBuffer(win32_sound_output *Output)
{
    VOID *Region1;
    DWORD Region1Size;
    VOID *Region2;
    DWORD Region2Size;
    if (SUCCEEDED(GlobalSecondaryBuffer->Lock(0, Output->SecondaryBufferSize,
                                              &Region1, &Region1Size, &Region2,
                                              &Region2Size, 0)))
    {
        uint8_t *DestSample = (uint8_t *)Region1;
        for (DWORD ByteIndex = 0; ByteIndex < Region1Size; ++ByteIndex)
        {
            *DestSample++ = 0;
        }
        DestSample = (uint8_t *)Region2;
        for (DWORD ByteIndex = 0; ByteIndex < Region2Size; ++ByteIndex)
        {
            *DestSample++ = 0;
        }
        GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2,
                                      Region2Size);
    }
}

internal void Win32FillSoundBuffer(win32_sound_output &Output, DWORD ByteToLock,
                                   DWORD BytesToWrite,
                                   game_sound_output_buffer *Source)
{
    VOID *Region1;
    DWORD Region1Size;
    VOID *Region2;
    DWORD Region2Size;
    if (SUCCEEDED(GlobalSecondaryBuffer->Lock(ByteToLock, BytesToWrite,
                                              &Region1, &Region1Size, &Region2,
                                              &Region2Size, 0)))
    {

        DWORD Region1SampleCount = Region1Size / Output.BytesPerSample;
        int16_t *DestSample = (int16_t *)Region1;
        int16_t *SourceSample = Source->Samples;
        for (DWORD SampleIndex = 0; SampleIndex < Region1SampleCount;
             ++SampleIndex)
        {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;
            ++Output.RunningSampleIndex;
        }
        DWORD Region2SampleCount = Region2Size / Output.BytesPerSample;
        DestSample = (int16_t *)Region2;
        for (DWORD SampleIndex = 0; SampleIndex < Region2SampleCount;
             ++SampleIndex)
        {
            *DestSample++ = *SourceSample++;
            *DestSample++ = *SourceSample++;
            ++Output.RunningSampleIndex;
        }
        GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2,
                                      Region2Size);
    }
}

int CALLBACK WinMain(HINSTANCE Instance, HINSTANCE PrevInstance,
                     PSTR CommandLine, int ShowCommand)
{
    // Register the window class.
    AllocConsole();
    freopen_s((FILE **)stdout, "CONOUT$", "w", stdout);
    WNDCLASS wc = {}; // Zero out the memory.

    Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);

    wc.lpfnWndProc = Win32WindowProc;
    wc.hInstance = Instance;
    wc.lpszClassName = TEXT("Handmade");
    wc.style = CS_OWNDC | CS_HREDRAW |
               CS_VREDRAW; // Redraw on size, and own DC for window.

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(0,                // Optional window styles.
                               wc.lpszClassName, // Window class
                               TEXT("Handemade Hero"), // Window text
                               WS_OVERLAPPEDWINDOW,    // Window style

                               // Size and position
                               CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                               CW_USEDEFAULT,

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
    int XOffset = 0;
    int YOffset = 0;

    GlobalRunning = true;

    win32_sound_output SoundOutput{};
    SoundOutput.SamplesPerSecond = 48000;
    SoundOutput.ToneHz = 256;
    SoundOutput.ToneVolume = 3000;
    SoundOutput.RunningSampleIndex = 0;
    SoundOutput.WavePeriod = SoundOutput.SamplesPerSecond / SoundOutput.ToneHz;
    SoundOutput.BytesPerSample = sizeof(int16_t) * 2;
    SoundOutput.SecondaryBufferSize =
        SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;
    SoundOutput.LatencySampleCount = SoundOutput.SamplesPerSecond / 15;

    Win32InitDSound(hwnd, SoundOutput.SamplesPerSecond,
                    SoundOutput.SecondaryBufferSize);
    Win32FillSoundBuffer(SoundOutput, 0, SoundOutput.SecondaryBufferSize);
    GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);
    LARGE_INTEGER Start;
    LARGE_INTEGER End;
    LARGE_INTEGER PerfCountFrequency;
    QueryPerformanceFrequency(&PerfCountFrequency);
    uint64_t LastCycleCount = __rdtsc();
    while (GlobalRunning)
    {
        QueryPerformanceCounter(&Start);

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
                bool LeftShoulder =
                    (Pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER);
                bool RightShoulder =
                    (Pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER);
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

        /*_________________
        | SOUND            |
        |__________________|*/
        game_sound_output_buffer SoundBuffer = {};
        int16_t Samples[48000 / 30 * 2];
        SoundBuffer.Samples = Samples;
        SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
        SoundBuffer.SampleCount = SoundOutput.SamplesPerSecond / 30;

        /*_________________
        | GRAPHICS         |
        |__________________|*/
        game_offscreen_buffer Buffer = {};
        Buffer.Memory = GlobalBackBuffer.Memory;
        Buffer.Width = GlobalBackBuffer.Width;
        Buffer.Height = GlobalBackBuffer.Height;
        Buffer.Pitch = GlobalBackBuffer.Pitch;
        GameUpdateAndRender(&Buffer, &SoundBuffer);
        DWORD PlayCursor;
        DWORD WriteCursor;

        if (SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor,
                                                                &WriteCursor)))
        {
            DWORD ByteToLock = SoundOutput.RunningSampleIndex *
                               SoundOutput.BytesPerSample %
                               SoundOutput.SecondaryBufferSize;
            DWORD BytesToWrite;
            DWORD TargetCursor = (PlayCursor + (SoundOutput.LatencySampleCount *
                                                SoundOutput.BytesPerSample)) %
                                 SoundOutput.SecondaryBufferSize;
            if (ByteToLock > TargetCursor)
            {
                BytesToWrite = (SoundOutput.SecondaryBufferSize - ByteToLock);
                BytesToWrite += TargetCursor;
            }
            else
            {
                BytesToWrite = TargetCursor - ByteToLock;
            }
            Win32FillSoundBuffer(SoundOutput, ByteToLock, BytesToWrite);
        }
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
        uint64_t EndCycleCount = __rdtsc();
        uint64_t CyclesElapsed = EndCycleCount - LastCycleCount;
        LastCycleCount = EndCycleCount;
        float MCPF = (float)CyclesElapsed / (1000.0f * 1000.0f);

        QueryPerformanceCounter(&End);
        float FPS = PerfCountFrequency.QuadPart /
                    (float)(End.QuadPart - Start.QuadPart);
        printf("Time: %f\n", (float)(End.QuadPart - Start.QuadPart) * 1000 /
                                 (float)PerfCountFrequency.QuadPart);
        printf("FPS: %f\n", FPS);
    }

    return 0;
}
