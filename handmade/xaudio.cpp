#pragma once

#include <fstream>
#include <objbase.h>
#include <string>
#include <vector>
#include <windows.h>
#include <xaudio2.h>

using namespace std;

enum AudioStatus
{
    VOL_UP, // 0
    VOL_DOWN,
    AUDIO_PAUSE,
    AUDIO_PLAY,
    AUDIO_RESTART,
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

IXAudio2SourceVoice *GlobalSourceVoice;
void AudioControl(AudioStatus status)
{
    printf("Changing volume with arg %d\n", status);
    switch (status)
    {
        case VOL_UP:
        {
            float volume;
            GlobalSourceVoice->GetVolume(&volume);
            if (volume < 1.0f)
            {
                printf("Setting volume to: %f\n", volume + 1.0f);
                HRESULT result = GlobalSourceVoice->SetVolume(volume + 1.0f);
            }
        }
        break;
        case VOL_DOWN:
        {
            float volume;
            GlobalSourceVoice->GetVolume(&volume);
            if (volume > 0.0f)
            {
                GlobalSourceVoice->SetVolume(volume - 1.0f);
            }
        }
        break;
        case AUDIO_PAUSE:
        {
            GlobalSourceVoice->Stop();
        }
        break;
        case AUDIO_PLAY:
        {
            GlobalSourceVoice->Start();
        }
        break;
        case AUDIO_RESTART:
        {
            // FIXME: Decide what to play, calling Start() without a buffer
            // crashes app.
            GlobalSourceVoice->Stop();
            GlobalSourceVoice->FlushSourceBuffers();
            GlobalSourceVoice->Start();
        }
        break;
    }
}

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

static void XAudioThread(HWND Window, IXAudio2 *pIXAudio)
{
    PlayWavFile2(pIXAudio, "oblique.wav");
    // Destroy master voice
    pIXAudio->Release();
    CoUninitialize();
}