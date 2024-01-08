

struct game_offscreen_buffer
{
    void *Memory;
    int Width;
    int Height;
    int Pitch;
};

struct game_sound_output_buffer
{
    int SamplesPerSecond;
    int SampleCount;
    int16_t *Samples;
};

static void GameUpdateAndRender(game_offscreen_buffer *Buffer, int XOffset,
                                int YOffset,
                                game_sound_output_buffer *SoundBuffer,
                                int ToneHz);