#pragma once
#include "handmade.h"
#include <stdint.h>
#include <stdio.h>
#include <windows.h>

static void RenderWeirdGradient(game_offscreen_buffer *Buffer, int XOffset,
                                int YOffset)
{
    uint8_t *Row = (uint8_t *)Buffer->Memory;
    for (int Y = 0; Y < Buffer->Height; ++Y)
    {
        uint32_t *Pixel = (uint32_t *)Row;
        for (int X = 0; X < Buffer->Width; ++X)
        {
            uint8_t Red = 0;
            uint8_t Green = (uint8_t)(X + XOffset);
            uint8_t Blue = (uint8_t)(Y + YOffset);
            *Pixel++ = ((Red << 16) | (Green << 8) | Blue);
        }
        Row += Buffer->Pitch;
    }
}

static void GameOutputSound(const game_sound_output_buffer &SoundBuffer,
                            int ToneHz)
{
    static float tSine = 0.0f;
    int16_t ToneVolume = 6000;
    int WavePeriod = SoundBuffer.SamplesPerSecond / ToneHz;

    int16_t *SampleOut = SoundBuffer.Samples;
    for (int SampleIndex = 0; SampleIndex < SoundBuffer.SampleCount;
         ++SampleIndex)
    {
        float SineValue = sinf(tSine);
        int16_t SampleValue = (int16_t)(SineValue * ToneVolume);
        *SampleOut++ = SampleValue;
        *SampleOut++ = SampleValue;

        tSine += 2.0f * 3.14159f * (1.0f / (float)WavePeriod);
    }
}

static void GameUpdateAndRender(game_memory *Memory, game_input *Input,
                                game_offscreen_buffer *Buffer,
                                game_sound_output_buffer *SoundBuffer)
{
    Assert(sizeof(game_state) <= Memory->PermanentStorageSize);
    game_state *GameState = (game_state *)Memory->PermanentStorage;
    if (!Memory->IsInitialized)
    {
#if HANDMADE_INTERNAL
        char *Filename = __FILE__;
        debug_read_file_result FileMemory =
            DEBUGPlatformReadEntireFile(Filename);
        if (FileMemory.Contents)
        {
            DEBUGPlatformWriteEntireFile("test.out", FileMemory.ContentsSize,
                                         FileMemory.Contents);
            DEBUGPlatformFreeFile(FileMemory.Contents);
        }
#endif
        GameState->ToneHz = 256;
        Memory->IsInitialized = true;
    }
    static int ToneHz = 256;
    static int XOffset;
    static int YOffset;
    if (Input->Controllers[0].MoveUp.EndedDown)
    {
        YOffset -= 100;
        printf("Up ended");
    }

    GameOutputSound(*SoundBuffer, ToneHz);
    RenderWeirdGradient(Buffer, XOffset, YOffset);
}