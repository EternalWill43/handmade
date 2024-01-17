#pragma once
#include <stdint.h>
#include <windows.h>

struct win32_window_dimension
{
    int Width;
    int Height;
};

struct win32_offscreen_buffer
{
    // NOTE: Pixels are always 32bit wide
    BITMAPINFO Info;
    void *Memory;
    int Width;
    int Height;
    int Pitch;
};

struct win32_sound_output
{
    int SamplesPerSecond;
    uint32_t RunningSampleIndex;
    int BytesPerSample;
    int SecondaryBufferSize;
    float tSine;
    int LatencySampleCount;
};