#pragma once
#include <cmath>
#include <cstdint>

#include "handmade.h"

static void GameOutputSound(const game_sound_output_buffer &SoundBuffer,
                            float ToneHz)
{
    static float tSine = 0.0f;
    int16_t ToneVolume = 3000;
    float WavePeriod = (float)SoundBuffer.SamplesPerSecond / ToneHz;

    int16_t *SampleOut = SoundBuffer.Samples;
    for (int SampleIndex = 0; SampleIndex < SoundBuffer.SampleCount;
         ++SampleIndex)
    {
        float SineValue = sinf(tSine);
        int16_t SampleValue = (int16_t)(SineValue * ToneVolume);
        *SampleOut++ = SampleValue;
        *SampleOut++ = SampleValue;

        tSine += 2.0f * Pi32 * (1.0f / (float)WavePeriod);
        if (tSine > (2.0f * Pi32))
        {
            tSine -= (2.0f * Pi32);
        }
    }
}

static void RenderWeirdGradiend(const game_offscreen_buffer &Buffer,
                                int XOffset, int YOffset)
{
    // TODO: Lets see what the optimizer does

    uint8_t *Row = (uint8_t *)Buffer.Memory;
    for (int Y = 0; Y < Buffer.Height; ++Y)
    {
        uint32_t *Pixel = (uint32_t *)Row;
        for (int X = 0; X < Buffer.Width; ++X)
        {
            uint8_t Green = (uint8_t)(Y + YOffset);
            uint8_t Blue = (uint8_t)(X + XOffset);
            /*
            Memory:   BB GG RR xx
            Register: xx RR GG BB
            Pixel (32-bits)
            */
            *Pixel++ = (Green << 8) | Blue;
        }
        Row += Buffer.Pitch;
    }
}

static void GameUpdateAndRender(game_memory *Memory,
                                game_offscreen_buffer &Buffer,
                                game_input &Input)
{
    ASSERT(
        (&Input.Controllers[0].Terminator - &Input.Controllers[0].Buttons[0]) ==
        ARRAY_COUNT(Input.Controllers->Buttons))
    ASSERT(sizeof(game_state) <= Memory->PermanentStorageSize);

    game_state *GameState = (game_state *)Memory->PermanentStorage;
    if (!Memory->IsInitialized)
    {
        char *Filename = __FILE__;
        debug_read_file_result Bitmap = DEBUGPlatformReadEntireFile(Filename);
        if (Bitmap.Contents)
        {
            DEBUGPlatformWriteEntireFile("test.txt", Bitmap.ContentsSize,
                                         Bitmap.Contents);
            DEBUGPlatformFreeFileMemory(Bitmap.Contents);
        }

        GameState->ToneHz = 512.0f;
        Memory->IsInitialized = true;
    }

    for (int ControllerIndex = 0;
         ControllerIndex < ARRAY_COUNT(Input.Controllers); ++ControllerIndex)
    {
        game_controller_input *Controller =
            GetController(&Input, ControllerIndex);
        if (Controller->IsAnalog)
        {
            // NOTE: Use analog movement tuning
            GameState->ToneHz = 512.0f + (128.0f * Controller->StickAverageY);
            GameState->BlueOffset += (int)(4.0f * Controller->StickAverageX);
        }
        else
        {
            if (Controller->MoveLeft.EndedDown)
            {
                GameState->BlueOffset -= 1;
            }
            if (Controller->MoveRight.EndedDown)
            {
                GameState->BlueOffset += 1;
            }

            // NOTe: Use digital movement tuning
        }

        if (Controller->ActionDown.EndedDown)
        {
            GameState->GreenOffset += 1;
        }
        if (Controller->ActionUp.EndedDown)
        {
            GameState->GreenOffset -= 1;
        }
    }

    RenderWeirdGradiend(Buffer, GameState->BlueOffset, GameState->GreenOffset);
}

static void GameGetSoundSamples(game_memory *Memory,
                                const game_sound_output_buffer &SoundOutput)
{
    game_state *GameState = (game_state *)Memory->PermanentStorage;
    GameOutputSound(SoundOutput, GameState->ToneHz);
}
