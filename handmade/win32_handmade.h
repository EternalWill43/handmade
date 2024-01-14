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

struct game_button_state
{
    int HalfTransitionCount;
    bool EndedDown;
};

struct game_controller_input
{
    bool IsConnected;
    bool IsAnalog;
    float StickAverageX;
    float StickAverageY;

    union
    {
        game_button_state Buttons[12];
        struct
        {
            game_button_state MoveUp;
            game_button_state MoveDown;
            game_button_state MoveLeft;
            game_button_state MoveRight;

            game_button_state ActionUp;
            game_button_state ActionDown;
            game_button_state ActionLeft;
            game_button_state ActionRight;

            game_button_state LeftShoulder;
            game_button_state RightShoulder;

            game_button_state Back;
            game_button_state Start;

            // NOTE: All buttons must be added above this line

            game_button_state Terminator;
        };
    };
};

struct game_input
{
    game_controller_input Controllers[5];
};