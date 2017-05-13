#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN
#include <windows.h>
#include <mmsystem.h>
#include <GL/gl.h>
#include <stdio.h>

// relevant glext.h fragment
#define APIENTRYP __stdcall *
typedef char GLchar;
#define GL_FRAGMENT_SHADER                0x8B30
typedef void (APIENTRYP PFNGLATTACHSHADERPROC) (GLuint program, GLuint shader);
typedef void (APIENTRYP PFNGLCOMPILESHADERPROC) (GLuint shader);
typedef GLuint (APIENTRYP PFNGLCREATEPROGRAMPROC) (void);
typedef GLuint (APIENTRYP PFNGLCREATESHADERPROC) (GLenum type);
typedef void (APIENTRYP PFNGLLINKPROGRAMPROC) (GLuint program);
typedef void (APIENTRYP PFNGLSHADERSOURCEPROC) (GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length);
typedef void (APIENTRYP PFNGLUSEPROGRAMPROC) (GLuint program);
typedef void (APIENTRYP PFNGLUNIFORM1FPROC) (GLint location, GLfloat v0);
// end of glext.h fragment

#define GL_EXT_FUNCTION_COUNT 8

static const char *glExtFunctionNames[] = {
    "glAttachShader",
    "glCompileShader",
    "glCreateProgram",
    "glCreateShader",
    "glLinkProgram",
    "glShaderSource",
    "glUseProgram",
    "glUniform1f"
};

static void *glExtFunctions[GL_EXT_FUNCTION_COUNT] = { 0 };

#define glAttachShader ((PFNGLATTACHSHADERPROC)glExtFunctions[0])
#define glCompileShader ((PFNGLCOMPILESHADERPROC)glExtFunctions[1])
#define glCreateProgram ((PFNGLCREATEPROGRAMPROC)glExtFunctions[2])
#define glCreateShader ((PFNGLCREATESHADERPROC)glExtFunctions[3])
#define glLinkProgram ((PFNGLLINKPROGRAMPROC)glExtFunctions[4])
#define glShaderSource ((PFNGLSHADERSOURCEPROC)glExtFunctions[5])
#define glUseProgram ((PFNGLUSEPROGRAMPROC)glExtFunctions[6])
#define glUniform1f ((PFNGLUNIFORM1FPROC)glExtFunctions[7])

static PIXELFORMATDESCRIPTOR pfd = {
    sizeof(PIXELFORMATDESCRIPTOR),
    1,
    PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
    PFD_TYPE_RGBA,
    32,
    0,
    0,
    0,
    0,
    0,
    0,
    8,
    0,
    0,
    0,
    0,
    0,
    0,
    32,
    0,
    0,
    PFD_MAIN_PLANE,
    0,
    0,
    0,
    0
};

#define WIDTH 1920
#define HEIGHT 1080
#define UNIFORM_COUNT 2

#include "shader.h"

#define TRACKER_PERIOD 4725 // 140 bpm (44100 * 60 / 140 / 4)
#define TRACKER_PATTERN_LENGTH 16 // 16 periods (16th) per pattern
#define TRACKER_SONG_LENGTH 33 // in patterns
#define AUDIO_SAMPLES (TRACKER_PERIOD * TRACKER_PATTERN_LENGTH * TRACKER_SONG_LENGTH * 2)

static const unsigned int riffHeader[11] = {
    0x46464952, /* RIFF */
    AUDIO_SAMPLES * 2 + 36,
    0x45564157, /* WAVE */
    
    /* fmt chunk */
    0x20746D66,
    16,
    0x00020001,
    44100,
    176400,
    0x00100004,
    
    /* data chunk */
    0x61746164,
    AUDIO_SAMPLES * 2
};

#define DELAY_LEFT (TRACKER_PERIOD * 2 * 2)
#define DELAY_RIGHT (TRACKER_PERIOD * 3 * 2)

short audioBuffer[AUDIO_SAMPLES + 22];
short auxBuffer[AUDIO_SAMPLES + DELAY_LEFT + DELAY_RIGHT];

float TAU = 2.0f * 3.14159265f;

void __declspec(naked) _CIpow()
{
	_asm
	{
		fxch st(1)
		fyl2x
		fld st(0)
		frndint
		fsubr st(1),st(0)
		fxch st(1)
		fchs
		f2xm1
		fld1
		faddp st(1),st(0)
		fscale
		fstp st(1)
		ret
	}
}

float fakeexp(float value)
{
	// complete fake approximation, but the curves look close enough ^^
	float result = 1.0f - pow(-value * 0.2f, 0.3f);
	return result > 0.0f ? result : 0.0f;
}

typedef short (*Instrument)(unsigned int frame, unsigned int frequency);

short silence(unsigned int frame, unsigned int period)
{
    return 0;
}

#define SAW_VOLUME_DIVIDER 5
short saw(unsigned int frame, unsigned int period)
{
    return (frame % period) * 65535 / period / SAW_VOLUME_DIVIDER - 32767 / SAW_VOLUME_DIVIDER
	     ;//+ (frame % (period + 1)) * 65535 / period / SAW_VOLUME_DIVIDER - 32767 / SAW_VOLUME_DIVIDER;
}

#define SAW2_VOLUME_DIVIDER 8
short saw2(unsigned int frame, unsigned int period)
{
    int adsr = (frame < 4096) ? 65535 - (frame << 4) : 0;
    return (frame % period) * adsr / period / SAW2_VOLUME_DIVIDER - (adsr >> 1) / SAW2_VOLUME_DIVIDER;
}

#define SQUARE_VOLUME_DIVIDER 6
short square(unsigned int frame, unsigned int period)
{
    return ((frame / (period >> 1)) & 1 * 2 - 1) * 32767 / SQUARE_VOLUME_DIVIDER;
}

#define KICK_VOLUME_DIVIDER 1
short kick(unsigned int frame, unsigned int period)
{
	/*float e = fakeexp(-2.0f);
	return (short)e;
    int adsr = 65536;//(frame < 4096) ? 65535 - (frame << 4) : 0;
	int offset = frame / 10000;
    period = 2048 + offset;
	//frame += offset;
    return (frame % period) * adsr / period / KICK_VOLUME_DIVIDER - (adsr >> 1) / KICK_VOLUME_DIVIDER;
    //return ((frame / (period >> 1)) & 1 * 2 - 1) * 32767 / KICK_VOLUME_DIVIDER;*/
	
	float t = (float)frame;
	float phase = TAU * t / (float)period;
	float out = fakeexp(-t * 0.001f) * sin(phase * fakeexp(-t * 0.0002f));
	
    return (short)(out * 32767.0f) / KICK_VOLUME_DIVIDER;
}

/*#define KICK_VOLUME_DIVIDER 4
short stupidkick(unsigned int frame, unsigned int period)
{
    int adsr = 65535;//(frame < 4096) ? 65535 - (frame << 4) : 0;
    period = (frame < 4096) ? 1024 - (frame >> 2) : 1;
    return (frame % period) * adsr / period / KICK_VOLUME_DIVIDER - (adsr >> 1) / KICK_VOLUME_DIVIDER;
    //return ((frame / (period >> 1)) & 1 * 2 - 1) * 32767 / KICK_VOLUME_DIVIDER;
}*/

#define SINE_VOLUME_DIVIDER 4
short sine(unsigned int frame, unsigned int period)
{
	float phase = TAU * (float)frame / (float)period;
    return (short)(sin(phase) * 32767.0f) / SINE_VOLUME_DIVIDER;
}

#define REESE_VOLUME_DIVIDER 4
short reese(unsigned int frame, unsigned int period)
{
	float detune = 1.01f;
	float phase = TAU * (float)frame / (float)period;
	float out = sin(phase) + 0.5f * sin(phase * 2.0) + 0.33f * sin(phase * 3.0)
	          + sin(phase * detune) + 0.5f * sin(phase * 2.0 * detune) + 0.33f * sin(phase * 3.0 * detune);
	
    return (short)(0.25f * out * 32767.0f) / REESE_VOLUME_DIVIDER;
}

Instrument instruments[] = {
    /* 0 */ silence,
    /* 1 */ saw,
    /* 2 */ saw2,
	/* 3 */ square,
	/* 4 */ kick,
	/* 5 */ sine,
	/* 6 */ reese
};

#define CHANNELS 8

typedef struct
{
    unsigned int frame;
    unsigned int period;
    unsigned char instrument;
} ChannelState;

ChannelState channels[CHANNELS];

// effect bitfield

// 0x8: left output
// 0x4: right output
// 0x2: stereo delay
// 0x1: pitch fall

unsigned short patterns[][TRACKER_PATTERN_LENGTH * 2] = {
    // note, effects (4 bits) + instrument (4 bits)
    {
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0
    },
	
	// 1
    {
        NOTE(37), 0xe2,
        0, 0,
        0, 0,
        0, 0,
        NOTE(40), 0xe2,
        0, 0,
        0, 0,
        0, 0,
        NOTE(42), 0xe2,
        0, 0,
        0, 0,
        0, 0,
        NOTE(44), 0xe2,
        0, 0,
        NOTE(47), 0xe1,
        0, 0
    },
	
	// 2 - bass
    {
        NOTE(13), 0xc6,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        NOTE(11), 0xc6,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        NOTE(16), 0xc6,
        0, 0
    },
	
	// 3
    {
        NOTE(21), 0xc1,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        NOTE(23), 0xc1,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0
    },
	
	// 4 - beat
    {
        NOTE(21), 0xc4,
        0, 0,
        0, 0,
        0, 0,
        NOTE(21), 0xc4,
        0, 0,
        0, 0,
        0, 0,
        NOTE(21), 0xc4,
        0, 0,
        0, 0,
        0, 0,
        NOTE(21), 0xc4,
        0, 0,
        0, 0,
        0, 0
    },
	
	// 5 - intro
	{
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        NOTE(42), 0xe2,
        NOTE(44), 0xe2,
        NOTE(42), 0xe2,
        0, 0,
        NOTE(40), 0xe2,
        0, 0,
        NOTE(37), 0xe2,
        0, 0,
        NOTE(35), 0xe2,
        0, 0
    },
	
	// 6 - start
	{
        NOTE(37), 0xe2,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0
    },
	
	// 7 - chords
	{
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        NOTE(25), 0xe2,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        NOTE(25), 0xe2,
        0, 0,
        0, 0,
        0, 0
    },
	
	// 8 - chords
	{
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        NOTE(32), 0xe2,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        NOTE(32), 0xe2,
        0, 0,
        0, 0,
        0, 0
    },
	
	// 9 - down
	{
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        NOTE(49), 0xe2,
        0, 0,
        NOTE(47), 0xe2,
        0, 0,
        NOTE(44), 0xe2,
        NOTE(42), 0xe2,
        NOTE(40), 0xe2,
        0, 0
    },
};

unsigned char song[TRACKER_SONG_LENGTH][CHANNELS] = {
	// 0 - intro (4)
    { 0, 5, 0, 0, 0, 0, 0, 0 },
	
	// 4 - black & white start (32)
    { 4, 6, 7, 8, 0, 0, 0, 0 },
    { 4, 6, 7, 8, 0, 0, 0, 0 },
    { 4, 6, 7, 8, 0, 0, 0, 0 },
    { 4, 9, 7, 8, 0, 0, 0, 0 },
    { 4, 6, 7, 8, 0, 0, 0, 0 },
    { 4, 6, 7, 8, 0, 0, 0, 0 },
    { 4, 6, 7, 8, 0, 0, 0, 0 },
    { 4, 5, 7, 8, 0, 0, 0, 0 },
	
	// 36 - blue! (32)
    { 4, 6, 7, 8, 2, 0, 0, 0 },
    { 4, 6, 7, 8, 2, 0, 0, 0 },
    { 4, 6, 7, 8, 2, 0, 0, 0 },
    { 4, 9, 7, 8, 2, 0, 0, 0 },
    { 4, 6, 7, 8, 2, 0, 0, 0 },
    { 4, 6, 7, 8, 2, 0, 0, 0 },
    { 4, 6, 7, 8, 2, 0, 0, 0 },
    { 4, 5, 7, 8, 2, 0, 0, 0 },
	
	// 68 - balls (32)
    { 4, 6, 7, 8, 2, 0, 0, 0 },
    { 4, 6, 7, 8, 2, 0, 0, 0 },
    { 4, 6, 7, 8, 2, 0, 0, 0 },
    { 4, 9, 7, 8, 2, 0, 0, 0 },
    { 4, 6, 7, 8, 2, 0, 0, 0 },
    { 4, 6, 7, 8, 2, 0, 0, 0 },
    { 4, 6, 7, 8, 2, 0, 0, 0 },
    { 4, 5, 7, 8, 2, 0, 0, 0 },
	
	//
    { 4, 6, 7, 8, 2, 0, 0, 0 },
    { 4, 6, 7, 8, 2, 0, 0, 0 },
    { 4, 6, 7, 8, 2, 0, 0, 0 },
    { 4, 9, 7, 8, 2, 0, 0, 0 },
    { 4, 6, 7, 8, 2, 0, 0, 0 },
    { 4, 6, 7, 8, 2, 0, 0, 0 },
    { 4, 6, 7, 8, 2, 0, 0, 0 },
    { 4, 5, 7, 8, 2, 0, 0, 0 }
};

static __forceinline void renderAudio()
{
    int i, j, k, l, delay;
    short *buffer = audioBuffer + 22;
    short *aux = auxBuffer + DELAY_LEFT + DELAY_RIGHT;
    char *songPosition;
    
    for (songPosition = (char *)song; songPosition != (char *)song + TRACKER_SONG_LENGTH * CHANNELS; songPosition += CHANNELS)
    {
        for (k = 0; k < TRACKER_PATTERN_LENGTH; k++)
        {
            for (j = 0; j < CHANNELS; j++)
            {
                short *periodBuffer = buffer;
                short *auxPeriodBuffer = aux;
                
                unsigned short *pattern = patterns[songPosition[j]];
                unsigned short period = pattern[k * 2 + 0];
                
                if (period)
                {
                    channels[j].frame = 0;
                    channels[j].period = period;
                    channels[j].instrument = pattern[k * 2 + 1];
                }
                
                for (l = 0; l < TRACKER_PERIOD; l++)
                {
                    short s = instruments[channels[j].instrument & 0x0f](channels[j].frame, channels[j].period);
                    short l = (channels[j].instrument & 0x80) ? s : 0;
                    short r = (channels[j].instrument & 0x40) ? s : 0;
                    
                    *periodBuffer++ += l;
                    *periodBuffer++ += r;
                    
                    *auxPeriodBuffer++ += (channels[j].instrument & 0x20) ? l : 0;
                    *auxPeriodBuffer++ += (channels[j].instrument & 0x20) ? r : 0;
                    
                    if (!(channels[j].frame % 1000) && (channels[j].instrument & 0x10))
                        channels[j].period++;
                    
                    channels[j].frame++;
                }
            }
            
            buffer += TRACKER_PERIOD * 2;
            aux += TRACKER_PERIOD * 2;
        }
    }
    
    delay = DELAY_LEFT;
    for (buffer = audioBuffer + 22, aux = auxBuffer + DELAY_LEFT + DELAY_RIGHT; aux != auxBuffer + AUDIO_SAMPLES + DELAY_LEFT + DELAY_RIGHT;)
    {
        short s = aux[-delay] * 3 / 4;
        *aux += s;
        
        delay ^= DELAY_LEFT;
        delay ^= DELAY_RIGHT;
        
        *buffer++ += *aux++;
    }
}

void entry()
{
    HWND hwnd;
    HDC hdc;
    unsigned int i;
    GLint program;
    GLint fragmentShader;
    DWORD startTime;
    float u[UNIFORM_COUNT];
    
    // debug
    HFILE audioFile;
    WORD bytesWritten;
    
    hwnd = CreateWindow("static", NULL, WS_POPUP | WS_VISIBLE, 0, 0, WIDTH, HEIGHT, NULL, NULL, NULL, 0);
    hdc = GetDC(hwnd);
    SetPixelFormat(hdc, ChoosePixelFormat(hdc, &pfd), &pfd);
    wglMakeCurrent(hdc, wglCreateContext(hdc));
    
    for (i = 0; i < GL_EXT_FUNCTION_COUNT; i++)
        glExtFunctions[i] = wglGetProcAddress(glExtFunctionNames[i]);
    
    program = glCreateProgram();
    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fs, 0);
    glCompileShader(fragmentShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    glUseProgram(program);
    
    renderAudio();
    memcpy(audioBuffer, &riffHeader, sizeof(riffHeader));
    
    // debug audio output
    audioFile = CreateFile("audio.wav", GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    WriteFile(audioFile, audioBuffer, sizeof(audioBuffer), &bytesWritten, NULL);
    CloseHandle(audioFile);
    
    sndPlaySound((LPCSTR)audioBuffer, SND_ASYNC | SND_MEMORY);
    
	ShowCursor(FALSE);
	
    startTime = timeGetTime();
    while (!GetAsyncKeyState(VK_ESCAPE))
    {
		// avoid 'not responding' system messages
		MSG msg;
		PeekMessage(&msg, NULL, 0, 0, PM_REMOVE);
		
        float time = (float)(timeGetTime() - startTime) * 0.001f * 140.0f / 60.0f;
        
        u[0] = time; // time
		u[1] = (float)(time < 4.0f); // black
        
        // hack - assume that the uniforms u[] will always be linked to locations [0-n]
        // given that they are the only uniforms in the shader, it is likely to work on all drivers
        for (i = 0; i < UNIFORM_COUNT; i++)
            glUniform1f(i, u[i]);
        
        glRects(-1, -1, 1, 1);
        wglSwapLayerBuffers(hdc, WGL_SWAP_MAIN_PLANE);
    }
    
	ExitProcess(0);
}
