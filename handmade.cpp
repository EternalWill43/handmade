#include <cassert>
#include <dsound.h>
#include <fstream>
#include <objbase.h>
#include <stdint.h>
#include <string>
#include <thread>
#include <vector>
#include <windows.h>
#include <xaudio2.h>

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
global_variable bool MINUS_STATE;
global_variable bool PLUS_STATE;

LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;

struct win32_offscreen_buffer
{
    BITMAPINFO Info;
    void *Memory;
    int Width;
    int Height;
    int Pitch;
};

struct WAVHeader
{
    char riff[4];
    unsigned int fileSize;
    char wave[4];
    char fmt[4];
    unsigned int fmtSize;
    unsigned short format;
    unsigned short channels;
    unsigned int sampleRate;
    unsigned int byteRate;
    unsigned short blockAlign;
    unsigned short bitsPerSample;
    char data[4];
    unsigned int dataSize;
};

bool ParseWAVHeader(const std::string &filename, WAVEFORMATEX &waveFormat,
                    std::vector<char> &audioData)
{
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open())
    {
        OutputDebugStringA("Failed to open file: ");
        return false;
    }

    WAVHeader header;
    file.read(reinterpret_cast<char *>(&header), sizeof(WAVHeader));

    if (strncmp(header.riff, "RIFF", 4) != 0 ||
        strncmp(header.wave, "WAVE", 4) != 0)
    {
        OutputDebugStringA("Invalid WAV file");
        return false;
    }

    waveFormat.wFormatTag = header.format;
    waveFormat.nChannels = header.channels;
    waveFormat.nSamplesPerSec = header.sampleRate;
    waveFormat.wBitsPerSample = header.bitsPerSample;
    waveFormat.nBlockAlign = header.blockAlign;
    waveFormat.nAvgBytesPerSec = header.byteRate;
    waveFormat.cbSize = 0;
#ifdef DEBUG
#include <stdio.h>
    AttachConsole(ATTACH_PARENT_PROCESS);

    // Redirect standard output to the console
    freopen("CONOUT$", "w", stdout);

    printf("%s\n", header.riff);
    char buffer[5];
    memcpy(buffer, header.riff, 4);
    buffer[4] = '\0';
    OutputDebugStringA(buffer);
    OutputDebugStringA("----------");
    OutputDebugStringA(header.wave);
    OutputDebugStringA(header.fmt);
    OutputDebugStringA(header.data);
    OutputDebugStringA("Format: ");
    OutputDebugStringA(std::to_string(header.format).c_str());
    OutputDebugStringA("Channels: ");
    OutputDebugStringA(std::to_string(header.channels).c_str());
    OutputDebugStringA("Sample Rate: ");
    OutputDebugStringA(std::to_string(header.sampleRate).c_str());
    OutputDebugStringA("Bits Per Sample: ");
    OutputDebugStringA(std::to_string(header.bitsPerSample).c_str());
    OutputDebugStringA("Byte Rate: ");
    OutputDebugStringA(std::to_string(header.byteRate).c_str());
    OutputDebugStringA("Block Align: ");
    OutputDebugStringA(std::to_string(header.blockAlign).c_str());
    OutputDebugStringA("Data Size: ");
    OutputDebugStringA(std::to_string(header.dataSize).c_str());
#endif
    // Read audio data
    audioData.resize(header.dataSize);
    file.read(audioData.data(), header.dataSize);

    return true;
}

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

enum SetVol
{
    VOL_UP,
    VOL_DOWN,
};

global_variable IXAudio2SourceVoice *GlobalSourceVoice;
void ChangeVolume(SetVol volume)
{
    printf("Changing volume\n");
    switch (volume)
    {
        case VOL_UP:
        {
            float volume;
            GlobalSourceVoice->GetVolume(&volume);
            if (volume < 1.0f)
            {
                printf("Setting volume to: %f\n", volume);
                HRESULT result = GlobalSourceVoice->SetVolume(volume + 1.0f);
                assert(result == S_OK);
            }
        }
        case VOL_DOWN:
        {
            float volume;
            GlobalSourceVoice->GetVolume(&volume);
            if (volume > 0.0f)
            {
                volume -= 0.1f;
                GlobalSourceVoice->SetVolume(volume);
            }
        }
    }
}

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

void Win32InitXAudio2(HWND Window, IXAudio2 **pXAudio2)
{
    // TODO: Import dynamically with funciton pointer
    if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)))
    {
        // TODO: Handle Failure
        return;
    }

    if (FAILED(XAudio2Create(pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR)))
    {
        // Handle failure
        CoUninitialize();
        return;
    }

    IXAudio2MasteringVoice *pMasterVoice = nullptr;
    if (FAILED((*pXAudio2)->CreateMasteringVoice(&pMasterVoice)))
    {
        // Handle failure
        (*pXAudio2)->Release();
        CoUninitialize();
        return;
    }

    // XAudio2 is successfully initialized and ready to use.
    // pXAudio2 and pMasterVoice are now initialized and can be used to play
    // audio.
    (*pXAudio2)->CreateMasteringVoice(&pMasterVoice);

    // When done, clean up:
    // pMasterVoice->DestroyVoice();
    // pXAudio2->Release();
    // CoUninitialize();
}

void PlayWavFile2(IXAudio2 *pIXAudio, const std::string &filename)
{
    // Open the WAV file
    std::ifstream file(filename, std::ios::binary);
    OutputDebugStringA("Playing file: ");
    if (!file.is_open())
    {
        OutputDebugStringA("Failed to open file: ");
        return;
    }

    // Read and parse the WAV header to fill this structure
    WAVEFORMATEX waveFormat = {};
    std::vector<char> audioData;
    if (!ParseWAVHeader(filename, waveFormat, audioData))
    {
        OutputDebugStringA("Failed to parse WAV header: ");
        return;
    }

    if (FAILED(pIXAudio->CreateSourceVoice(&GlobalSourceVoice, &waveFormat)))
    {
        OutputDebugStringA("Failed to create source voice: ");
        return;
    }

    std::vector<char> audioData2(std::istreambuf_iterator<char>(file), {});

    XAUDIO2_BUFFER buffer = {0};
    buffer.AudioBytes = static_cast<UINT32>(audioData2.size());
    buffer.pAudioData = reinterpret_cast<const BYTE *>(audioData2.data());
    buffer.Flags = XAUDIO2_END_OF_STREAM;

    GlobalSourceVoice->SubmitSourceBuffer(&buffer);

    GlobalSourceVoice->SetVolume(0.5f);
    GlobalSourceVoice->Start(0);
    XAUDIO2_VOICE_STATE state;
    do
    {
        GlobalSourceVoice->GetState(&state);
        Sleep(100);
    } while (state.BuffersQueued > 0);
    GlobalSourceVoice->Stop();
    GlobalSourceVoice->DestroyVoice();
    OutputDebugStringA("Done playing file: ");
}

void PlayXAudioSquareWave(IXAudio2 *pIXAudio)
{
    WAVEFORMATEX WaveFormat = {};
    DWORD SamplesPerSecond = 48000;
    WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
    WaveFormat.nChannels = 2;
    WaveFormat.nSamplesPerSec = SamplesPerSecond;
    WaveFormat.wBitsPerSample = 16;
    WaveFormat.nBlockAlign =
        (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8;
    WaveFormat.nAvgBytesPerSec =
        WaveFormat.nSamplesPerSec * WaveFormat.nBlockAlign;
    WaveFormat.cbSize = 0;
    IXAudio2SourceVoice *pSourceVoice;
    if (FAILED(pIXAudio->CreateSourceVoice(&pSourceVoice, &WaveFormat)))
    {
        return;
    }
    pSourceVoice->SetVolume(0.2f);
    float frequency = 440.0f;
    const float duration = 2.0f;
    const int totalSamples = 48000 * 2;
    int samplesPerWaveLength = WaveFormat.nSamplesPerSec / frequency;
    short amplitude = 50;
    int16_t audioData[48000 * 2 * 2];
    for (int i = 0; i < totalSamples; ++i)
    {
        short value =
            (i / samplesPerWaveLength) % 2 == 0 ? amplitude : -amplitude;
        for (int channel = 0; channel < WaveFormat.nChannels; ++channel)
        {
            audioData[i * WaveFormat.nChannels + channel] = value;
        }
    }

    XAUDIO2_BUFFER buffer = {0};
    buffer.AudioBytes = static_cast<UINT32>(
        totalSamples * WaveFormat.nChannels * sizeof(int16_t));
    buffer.pAudioData = reinterpret_cast<const BYTE *>(audioData);
    buffer.Flags = XAUDIO2_END_OF_STREAM;

    pSourceVoice->SubmitSourceBuffer(&buffer);

    pSourceVoice->Start(0);
    Sleep(2000);
    pSourceVoice->Stop();
    pSourceVoice->DestroyVoice();
}

internal void XAudioThread(HWND Window, IXAudio2 *pIXAudio)
{
    PlayWavFile2(pIXAudio, "oblique.wav");
    // Destroy master voice
    pIXAudio->Release();
    CoUninitialize();
}

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
                if (DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY))
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

internal void RenderWeirdGradient(win32_offscreen_buffer *Buffer, int XOffset,
                                  int YOffset)
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
                if (VKCode == 'W') // W Key
                {
                    W_STATE = true;
                    S_STATE = false;
                }
                else if (VKCode == 'S') // S Key
                {
                    W_STATE = false;
                    S_STATE = true;
                }
                else if (VKCode == 'A') // A Key
                {
                    A_STATE = true;
                    D_STATE = false;
                }
                else if (VKCode == 'D') // D Key
                {
                    D_STATE = true;
                    A_STATE = false;
                }
                else if (VKCode == 'U')
                {
                    ChangeVolume(VOL_UP);
                }
                else if (VKCode == VK_OEM_MINUS)
                {
                    ChangeVolume(VOL_DOWN);
                }
                else
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

int CALLBACK WinMain(HINSTANCE Instance, HINSTANCE PrevInstance,
                     PSTR CommandLine, int ShowCommand)
{
    // Register the window class.

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

    GlobalRunning = true;
    int SamplesPerSecond = 48000;
    int XOffset = 0;
    int YOffset = 0;
    uint32_t RunningSampleIndex = 0;
    int Hz = 256;
    int16_t ToneVolume = 3000;
    int SquareWaveCounter = 0;
    int SquareWavePeriod = SamplesPerSecond / Hz;
    int BytesPerSample = sizeof(int16_t) * 2;
    int SecondaryBufferSize = SamplesPerSecond * BytesPerSample;
    Win32InitDSound(hwnd, SamplesPerSecond, SecondaryBufferSize);
    // GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);
    IXAudio2 *pIXAudio;
    Win32InitXAudio2(hwnd, &pIXAudio);
    std::thread AudioThread(XAudioThread, hwnd, pIXAudio);
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
        RenderWeirdGradient(&GlobalBackBuffer, XOffset, YOffset);

        DWORD PlayCursor;
        DWORD WriteCursor;

        GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor);
        VOID *Region1;
        DWORD Region1Size;
        VOID *Region2;
        DWORD Region2Size;
        DWORD BytesToWrite;
        DWORD ByteToLock =
            RunningSampleIndex * BytesPerSample % SecondaryBufferSize;
        if (ByteToLock > PlayCursor)
        {

            BytesToWrite = SecondaryBufferSize - ByteToLock;
            BytesToWrite += PlayCursor;
        }
        else
        {
            BytesToWrite = PlayCursor - ByteToLock;
        }
        GlobalSecondaryBuffer->Lock(ByteToLock, BytesToWrite, &Region1,
                                    &Region1Size, &Region2, &Region2Size, 0);
        int16_t *SampleOut = (int16_t *)Region1;
        DWORD Region1SampleCount = Region1Size / BytesPerSample;
        DWORD Region2SampleCount = Region2Size / BytesPerSample;
        for (DWORD SampleIndex = 0; SampleIndex < Region1SampleCount;
             ++SampleIndex)
        {
            if (SquareWaveCounter)
            {
                SquareWaveCounter = SquareWavePeriod;
            }
            int16_t SampleValue =
                ((RunningSampleIndex / (SquareWavePeriod / 2)) % 2)
                    ? ToneVolume
                    : -ToneVolume;
            *SampleOut++ = SampleValue;
            *SampleOut++ = SampleValue;
            ++RunningSampleIndex;
        }
        for (DWORD SampleIndex = 0; SampleIndex < Region2SampleCount;
             ++SampleIndex)
        {
            if (SquareWaveCounter)
            {
                SquareWaveCounter = SquareWavePeriod;
            }
            int16_t SampleValue =
                ((RunningSampleIndex / (SquareWavePeriod / 2)) % 2)
                    ? ToneVolume
                    : -ToneVolume;
            *SampleOut++ = SampleValue;
            *SampleOut++ = SampleValue;
            ++RunningSampleIndex;
        }
        GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2,
                                      Region2Size);
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
