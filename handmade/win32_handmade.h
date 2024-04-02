#pragma once

#include <cstdint>
#include <windows.h>

struct win32_offscreen_buffer
{
    // NOTE: Pixels are always 32bit wide
    BITMAPINFO Info;
    void *Memory;
    int Width;
    int Height;
    int Pitch;
    int BytesPerPixel;
};

struct win32_window_dimension
{
    int Width;
    int Height;
};

struct win32_sound_output
{
    int SamplesPerSecond;
    uint32_t RunningSampleIndex;
    int BytesPerSample;
    DWORD SecondaryBufferSize;
    DWORD SafetyBytes;
    float tSine;
    int LatencySampleCount;
};

struct win32_debug_time_marker
{
    DWORD OutputPlayCursor;
    DWORD OutputWriteCursor;
    DWORD OutputLocation;
    DWORD OutputByteCount;
    DWORD ExpectedFlipPlayCursor;

    DWORD FlipPlayCursor;
    DWORD FlipWriteCursor;
};
