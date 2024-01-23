#if HANDMADE_INTERNAL
struct debug_read_file_result
{
    uint32_t ContentsSize;
    void *Contents;
};
static debug_read_file_result DEBUGPlatformReadEntireFile(char *Filename);
static int DEBUGPlatformWriteEntireFile(char *Filename, uint32_t MemorySize,
                                        void *Memory);
static void DEBUGPlatformFreeFile(void *Memory);
#endif

#if HANDMADE_SLOW
#define Assert(Expression)                                                     \
    if (!(Expression))                                                         \
    {                                                                          \
        *(int *)0 = 0;                                                         \
    }
#else
#define Assert(Expression)
#endif

#define ArraySize(Array) (sizeof(Array) / sizeof((Array)[0]))
#define Kilobytes(Value) ((Value) * 1024LL)
#define Megabytes(Value) (Kilobytes(Value) * 1024LL)
#define Gigabytes(Value) (Megabytes(Value) * 1024LL)
#define Terabytes(Value) (Gigabytes(Value) * 1024LL)

inline uint32_t SafeTruncateUInt64(uint64_t value)
{
    Assert(value <= 0xFFFFFFFF);
    uint32_t Result = (uint32_t)value;
    return Result;
}

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

struct game_state
{
    int ToneHz;
    int XOffset;
    int YOffset;
};

struct game_memory
{
    bool IsInitialized;
    uint64_t PermanentStorageSize;
    void *PermanentStorage; // NOTE: REQUIRED to be cleared to zero at startup
    uint64_t TransientStorageSize;
    void *TransientStorage; // NOTE: REQUIRED to be cleared to zero at startup
};