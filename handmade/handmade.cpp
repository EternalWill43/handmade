#pragma once
#include "handmade.h"

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

static void GameUpdateAndRender(game_offscreen_buffer *Buffer)
{
    int XOffset = 0;
    int YOffset = 0;
    RenderWeirdGradient(Buffer, XOffset, YOffset);
}