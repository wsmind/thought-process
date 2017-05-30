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

//#define CAPTURE_FRAMES

#define UNIFORM_COUNT 6

#include "shader.h"

#define TRACKER_PERIOD 4725 // 140 bpm (44100 * 60 / 140 / 4)
#define TRACKER_PATTERN_LENGTH 16 // 16 periods (16th) per pattern
#define TRACKER_SONG_LENGTH 85 // in patterns
#define AUDIO_SAMPLES (TRACKER_PERIOD * TRACKER_PATTERN_LENGTH * TRACKER_SONG_LENGTH * 2)

//#define AUDIO_DEBUG

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

float rand(float f)
{
	f = sin(f * 12.9898) * 43758.5453;
	return f - (float)(int)f;
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

#define SAW2_VOLUME_DIVIDER 10
short saw2(unsigned int frame, unsigned int period)
{
    int adsr = (frame < 4096) ? 65535 - (frame << 4) : 0;
    return (frame % period) * adsr / period / SAW2_VOLUME_DIVIDER - (adsr >> 1) / SAW2_VOLUME_DIVIDER;
}

#define SQUARE_VOLUME_DIVIDER 12
short square(unsigned int frame, unsigned int period)
{
    return ((frame / (period >> 1)) & 1 * 2 - 1) * 32767 / SQUARE_VOLUME_DIVIDER;
}

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
	
    return (short)(out * 30000.0f);
}

/*#define KICK_VOLUME_DIVIDER 4
short stupidkick(unsigned int frame, unsigned int period)
{
    int adsr = 65535;//(frame < 4096) ? 65535 - (frame << 4) : 0;
    period = (frame < 4096) ? 1024 - (frame >> 2) : 1;
    return (frame % period) * adsr / period / KICK_VOLUME_DIVIDER - (adsr >> 1) / KICK_VOLUME_DIVIDER;
    //return ((frame / (period >> 1)) & 1 * 2 - 1) * 32767 / KICK_VOLUME_DIVIDER;
}*/

#define SINE_VOLUME_DIVIDER 6
short sine(unsigned int frame, unsigned int period)
{
	float phase = TAU * (float)frame / (float)period;
	float adsr = fakeexp(-(float)frame * 0.001f);
	float out = adsr * sin(phase);// + 0.2f * sin(phase * 1.1f);
    return (short)(out * 32767.0f) / SINE_VOLUME_DIVIDER;
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

#define NOISE_VOLUME_DIVIDER 32
short noise(unsigned int frame, unsigned int period)
{
	float phase = TAU * (float)frame / (float)period;
	float adsr = fakeexp(-phase * 0.01f);
	float out = adsr * rand(phase) * 2.0 - 1.0;
	
    return (short)(out * 32767.0f) / NOISE_VOLUME_DIVIDER;
}

Instrument instruments[] = {
    /* 0 */ silence,
    /* 1 */ saw,
    /* 2 */ saw2,
	/* 3 */ square,
	/* 4 */ kick,
	/* 5 */ sine,
	/* 6 */ reese,
	/* 7 */ noise
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
        NOTE(0), 0,
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
	
	// 1 - semi-fast beat
    {
        NOTE(37), 0xc4,
        0, 0,
        0, 0,
        0, 0,
        NOTE(37), 0xc4,
        0, 0,
        NOTE(37), 0xc4,
        0, 0,
        NOTE(37), 0xc4,
        0, 0,
        0, 0,
        0, 0,
        NOTE(37), 0xc4,
        0, 0,
        NOTE(37), 0xc4,
        NOTE(37), 0xc4,
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
	
	// 3 - smi-fast beat 2
    {
        NOTE(37), 0xc4,
        0, 0,
        0, 0,
        NOTE(37), 0xc4,
        NOTE(37), 0xc4,
        0, 0,
        0, 0,
        0, 0,
        NOTE(37), 0xc4,
        0, 0,
        NOTE(37), 0xc4,
        0, 0,
        0, 0,
        0, 0,
        NOTE(37), 0xc4,
        0, 0,
    },
	
	// 4 - beat
    {
        NOTE(37), 0xc4,
        0, 0,
        0, 0,
        0, 0,
        NOTE(37), 0xc4,
        0, 0,
        0, 0,
        0, 0,
        NOTE(37), 0xc4,
        0, 0,
        0, 0,
        0, 0,
        NOTE(37), 0xc4,
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
	
	// 10 - high pitch
	{
        NOTE(52), 0xe5,
        0, 0,
        NOTE(44), 0xe5,
        0, 0,
        NOTE(49), 0xe5,
        0, 0,
        NOTE(44), 0xe5,
        0, 0,
        NOTE(52), 0xe5,
        0, 0,
        NOTE(44), 0xe5,
        0, 0,
        NOTE(49), 0xe5,
        0, 0,
        NOTE(44), 0xe5,
        0, 0,
    },
	
	// 11 - lead fill
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
        NOTE(44), 0xe2,
        0, 0,
        NOTE(47), 0xe2,
        0, 0,
        NOTE(49), 0xe2,
        0, 0
    },
	
	// 12 - lead variation
	{
        NOTE(42), 0xe2,
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
        NOTE(43), 0xe2,
        0, 0,
        0, 0,
        0, 0
    },
	
	// 13 - bass variation
    {
        NOTE(6), 0xc3,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        NOTE(0), 0,
        0, 0,
        0, 0,
        0, 0,
        NOTE(6), 0xc3,
        0, 0,
        0, 0,
        0, 0
    },
	
	// 14 - bass variation 2
    {
        NOTE(0), 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        NOTE(4), 0xc3,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0
    },
	
	// 15 - reese variation
    {
        NOTE(18), 0xc6,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        NOTE(0), 0,
        0, 0,
        0, 0,
        0, 0,
        NOTE(18), 0xc6,
        0, 0,
        0, 0,
        0, 0
    },
	
	// 16 - reese variation 2
    {
        NOTE(0), 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        NOTE(16), 0xc6,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0
    },
	
	// 17 - faster beat
    {
        NOTE(37), 0xc4,
        0, 0,
        0, 0,
        NOTE(37), 0xc4,
        NOTE(37), 0xc4,
        0, 0,
        0, 0,
        NOTE(37), 0xc4,
        NOTE(37), 0xc4,
        0, 0,
        0, 0,
        NOTE(37), 0xc4,
        NOTE(37), 0xc4,
        0, 0,
        0, 0,
        NOTE(37), 0xc4,
    },
	
	// 18 - faster chords
	{
        NOTE(30), 0xe5,
        0, 0,
        NOTE(18), 0xe5,
        0, 0,
        NOTE(30), 0xe5,
        0, 0,
        NOTE(42), 0xe5,
        0, 0,
        NOTE(30), 0xe5,
        0, 0,
        NOTE(18), 0xe5,
        0, 0,
        NOTE(30), 0xe5,
        0, 0,
        NOTE(42), 0xe5,
        0, 0,
    },
	
	// 19 - high pitch variation
	{
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        NOTE(47), 0xe5,
        0, 0,
        NOTE(47), 0xe5,
        NOTE(47), 0xe5,
        0, 0,
        0, 0,
        NOTE(47), 0xe5,
        0, 0,
    },
	
	// 20 - noise FX
	{
        NOTE(30), 0xe7,
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
    },
	
	// 21 - high pitch FX
	{
        NOTE(61), 0xe5,
        0, 0,
        NOTE(61), 0xe5,
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
    },
	
	// 22 - hi-hat
	{
        NOTE(120), 0xe7,
        0, 0,
        0, 0,
        NOTE(120), 0xe7,
        NOTE(120), 0xe7,
        0, 0,
        0, 0,
        0, 0,
        NOTE(120), 0xe7,
        NOTE(120), 0xe7,
        0, 0,
        0, 0,
        NOTE(120), 0xe7,
        0, 0,
        0, 0,
        0, 0,
    },
	
	// 23 - lead crazy
	{
        NOTE(52), 0xe2,
        0, 0,
        0, 0,
        0, 0,
        NOTE(51), 0xe2,
        0, 0,
        NOTE(49), 0xe2,
        0, 0,
        0, 0,
        0, 0,
        NOTE(44), 0xe2,
        0, 0,
        0, 0,
        0, 0,
        NOTE(42), 0xe2,
        0, 0
    },
	
	// 24 - lead crazy 2
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
        NOTE(40), 0xe2,
        0, 0,
        NOTE(42), 0xe2,
        0, 0,
        NOTE(44), 0xe2,
        0, 0
    },
	
	// 25 - lasers
	{
        NOTE(40), 0xf3,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        NOTE(32), 0xf3,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
    },
	
	// 26 - lead crazy 3
	{
        NOTE(52), 0xe2,
        0, 0,
        0, 0,
        0, 0,
        NOTE(51), 0xe2,
        0, 0,
        NOTE(49), 0xe2,
        0, 0,
        0, 0,
        0, 0,
        NOTE(44), 0xe2,
        0, 0,
        0, 0,
        0, 0,
        0, 0,
        0, 0
    },
	
	// 27 - lead crazy 4
	{
        NOTE(54), 0xe2,
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
        NOTE(52), 0xe2,
        0, 0,
        0, 0,
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
    { 4, 6, 7, 8, 2, 0, 0, 20 },
    { 4, 6, 7, 8, 2, 0, 0, 0 },
    { 4, 6, 7, 8, 2, 0, 0, 0 },
    { 4, 9, 7, 8, 2, 0, 0, 0 },
    { 4, 6, 7, 8, 2, 0, 0, 0 },
    { 4, 6, 7, 8, 2, 0, 0, 0 },
    { 4, 6, 7, 8, 2, 0, 0, 0 },
    { 4, 5, 7, 8, 2, 0, 0, 0 },
	
	// 68 - balls (32)
    { 4, 6, 7, 8, 2, 10, 0, 21 },
    { 4, 6, 7, 8, 2, 10, 0, 0 },
    { 4, 6, 7, 8, 2, 10, 0, 0 },
    { 4, 9, 7, 8, 2, 10, 0, 0 },
    { 4, 6, 7, 8, 2, 10, 0, 0 },
    { 4, 6, 7, 8, 2, 10, 0, 0 },
    { 4, 6, 7, 8, 2, 10, 0, 0 },
    { 4, 5, 7, 8, 2, 10, 0, 22 },
	
	// 100 - more balls! (32)
    { 4, 6, 7, 8, 2, 10, 0, 21 },
    { 4, 6, 7, 8, 2, 10, 0, 0 },
    { 4, 6, 7, 8, 2, 10, 0, 0 },
    { 4, 9, 7, 8, 2, 10, 0, 0 },
    { 4, 6, 7, 8, 2, 10, 0, 21 },
    { 4, 6, 7, 8, 2, 10, 0, 0 },
    { 4, 6, 7, 8, 2, 10, 0, 0 },
    { 4, 11, 7, 8, 2, 10, 0, 22 },
	
	// 132 - fire from hell (32)
    { 17, 12, 15, 18, 13, 0, 0, 20 },
    { 17, 12, 16, 18, 14, 0, 0, 0 },
    { 17, 12, 15, 18, 13, 0, 0, 0 },
    { 17, 12, 16, 18, 14, 19, 0, 0 },
    { 17, 12, 15, 18, 13, 0, 0, 0 },
    { 17, 12, 16, 18, 14, 0, 0, 21 },
    { 17, 12, 15, 18, 13, 0, 0, 0 },
    { 17, 12, 16, 18, 14, 19, 0, 0 },
	
	// 164 - more fire (32)
    { 17, 12, 15, 18, 13, 00, 25, 20 },
    { 17, 12, 16, 18, 14, 00, 0, 00 },
    { 17, 12, 15, 18, 13, 00, 0, 21 },
    { 17, 12, 16, 18, 14, 19, 0, 00 },
    { 17, 12, 15, 18, 13, 00, 25, 20 },
    { 17, 12, 16, 18, 14, 00, 0, 21 },
    { 17, 12, 15, 18, 13, 00, 0, 00 },
    { 1, 12, 16, 18, 14, 19, 20, 22 },
	
	// 196 - craziness (32)
    { 1, 23, 7, 8, 2, 10, 22, 21 },
    { 3, 24, 7, 8, 2, 10, 22, 0 },
    { 4, 6, 7, 8, 2, 10, 22, 0 },
    { 1, 6, 7, 8, 2, 10, 22, 0 },
    { 3, 26, 7, 8, 2, 10, 22, 21 },
    { 1, 27, 7, 8, 2, 10, 22, 0 },
    { 4, 6, 7, 8, 2, 10, 22, 0 },
    { 3, 9, 7, 8, 2, 10, 22, 0 },
	
	// 228 - craziness (32)
    { 3, 23, 7, 8, 2, 10, 2, 20 },
    { 1, 24, 7, 8, 2, 10, 22, 0 },
    { 4, 6, 7, 8, 2, 10, 22, 21 },
    { 3, 6, 7, 8, 2, 10, 22, 19 },
    { 1, 26, 7, 8, 2, 10, 22, 0 },
    { 4, 27, 7, 8, 2, 10, 22, 21 },
    { 3, 6, 7, 8, 2, 10, 22, 0 },
    { 1, 11, 5, 8, 2, 10, 22, 19 },
	
	// 260 - back to the roots, but crazy (32) - greets?
    { 4, 6, 7, 8, 0, 0, 0, 0 },
    { 4, 6, 7, 8, 0, 0, 0, 0 },
    { 4, 6, 7, 8, 0, 0, 0, 0 },
    { 4, 9, 7, 8, 0, 0, 0, 0 },
    { 4, 6, 7, 8, 0, 0, 0, 0 },
    { 4, 6, 7, 8, 0, 0, 0, 0 },
    { 4, 6, 7, 8, 0, 0, 0, 0 },
    { 4, 5, 7, 8, 0, 0, 0, 0 },
	
	// 292 - blue again! (32)
    { 4, 6, 7, 8, 2, 0, 0, 20 },
    { 4, 6, 7, 8, 2, 0, 0, 0 },
    { 4, 6, 7, 8, 2, 0, 0, 0 },
    { 4, 9, 7, 8, 2, 0, 0, 0 },
    { 4, 6, 7, 8, 2, 0, 0, 0 },
    { 4, 6, 7, 8, 2, 0, 0, 0 },
    { 4, 6, 7, 8, 2, 0, 0, 0 },
    { 4, 5, 7, 8, 2, 0, 0, 0 },
	
	// 324 - end
    { 0, 6, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0, 0 }
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
                    short s = (short)((int)instruments[channels[j].instrument & 0x0f](channels[j].frame, channels[j].period) * 3 / 4);
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

#ifdef CAPTURE_FRAMES
char *frameFilename(int n)
{
    static char *name = "frame00000.raw";
    char *ptr = name + 9;
    
    while (n > 0)
    {
        *ptr-- = (n - (n / 10) * 10) + '0';
        n /= 10;
    }
    
    return name;
}
#endif

void entry()
{
    HWND hwnd;
    HDC hdc;
    unsigned int i;
    GLint program;
    GLint fragmentShader;
    DWORD startTime;
	DWORD width;
	DWORD height;
    float u[UNIFORM_COUNT];
    
#ifdef AUDIO_DEBUG
    HANDLE audioFile;
    DWORD bytesWritten;
#endif
    
#ifdef CAPTURE_FRAMES
    int frameNumber;
    HANDLE frameFile;
    DWORD frameBytesWritten;
    char *frameBuffer;
#endif

	width = GetSystemMetrics(SM_CXSCREEN);
	height = GetSystemMetrics(SM_CYSCREEN);
	
#ifdef CAPTURE_FRAMES
    frameNumber = 0;
    frameBuffer = HeapAlloc(GetProcessHeap(), 0, width * height * 3 /* RGB8 */);
#endif

    hwnd = CreateWindow("static", NULL, WS_POPUP | WS_VISIBLE, 0, 0, width, height, NULL, NULL, NULL, 0);
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
    
#ifdef AUDIO_DEBUG
    // debug audio output
    audioFile = CreateFile("audio.wav", GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    WriteFile(audioFile, audioBuffer, sizeof(audioBuffer), &bytesWritten, NULL);
    CloseHandle(audioFile);
#endif
    
    sndPlaySound((LPCSTR)audioBuffer, SND_ASYNC | SND_MEMORY);
    
	ShowCursor(FALSE);
	
    startTime = timeGetTime();
    while (!GetAsyncKeyState(VK_ESCAPE))
    {
        float time;
        
		// avoid 'not responding' system messages
		MSG msg;
		PeekMessage(&msg, NULL, 0, 0, PM_REMOVE);
		
#ifdef CAPTURE_FRAMES
        // capture at a steady 60fps
        time = (float)frameNumber / 60.0f * 140.0f / 60.0f;
        
        // stop at the end of the music
        if (((float)frameNumber / 60.0f) > (AUDIO_SAMPLES / 2 / 44100.0f))
            break;
#else
        time = (float)(timeGetTime() - startTime) * 0.001f * 140.0f / 60.0f;
#endif
        
		//time += 164.0f;
		
        u[0] = time; // time
		u[1] = (float)(time < 4.0f); // black
		u[1] += (time >= 324.0f) ? (time - 324.0f) * 0.25f : 0.0f;
		u[2] = 1.0f - (float)(time >= 68.0f && time < 260.0f); // spheres
		
		u[3] = 0.0f;
		if (time >= 164.0f && time < 168.0f)
			u[3] = 4.0 - (time - 164.0f);
		else if (time >= 180.0f && time < 184.0f)
			u[3] = 4.0 - (time - 180.0f);

        u[4] = (float)width;
        u[5] = (float)height;
        
        // hack - assume that the uniforms u[] will always be linked to locations [0-n]
        // given that they are the only uniforms in the shader, it is likely to work on all drivers
        for (i = 0; i < UNIFORM_COUNT; i++)
            glUniform1f(i, u[i]);
        
        glRects(-1, -1, 1, 1);
        
#ifdef CAPTURE_FRAMES
        // read back pixels
        glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, frameBuffer);
        
        // write ouput frame (skip existing ones)
        frameFile = CreateFile(frameFilename(frameNumber), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
		if (frameFile)
		{
			WriteFile(frameFile, frameBuffer, width * height * 3, &frameBytesWritten, NULL);
			CloseHandle(frameFile);
		}
        
        frameNumber++;
#endif
        
        wglSwapLayerBuffers(hdc, WGL_SWAP_MAIN_PLANE);
    }
    
	ExitProcess(0);
}
