

struct game_offscreen_buffer
{
    void *Memory;
    int Width;
    int Height;
    int Pitch;
};

static void GameUpdateAndRender(game_offscreen_buffer *Buffer);