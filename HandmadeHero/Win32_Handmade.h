#pragma once
struct win32_offscreen_buffer
{
	BITMAPINFO info;
	void *memory;
	int width;
	int height;
	int pitch;
	int bytesPerPixel;
};

struct win32_window_dimension
{
	int width;
	int height;
};

struct win32_sound_output
{
	int samplesPerSec;
	uint32 runningSampleIndex;
	int bytesPerSample;
	DWORD secondaryBufferSize;
	DWORD safetyBytes;
};

struct win32_debug_time_marker
{
	DWORD outputPlayCursor;
	DWORD outputWriteCursor;
	DWORD outputLocation;
	DWORD outputByteCount;
	DWORD expectedFlipPlayCursor;
	
	DWORD flipPlayCursor;
	DWORD flipWriteCursor;
};

struct win32_game_code
{
	HMODULE gameCodeDll;
	FILETIME dllLastWriteTime;

	game_update_and_render *UpdateAndRender;
	game_get_sound_samples *GetSoundSamples;

	bool32 isValid;
};

struct win32_recorded_input
{
	int inputCount;
	game_input *inputStream;
};

#define WIN32_STATE_FILE_NAME_COUNT MAX_PATH
struct win32_replay_buffer
{
	HANDLE fileHandle;
	HANDLE memoryMap;
	char fileName[WIN32_STATE_FILE_NAME_COUNT];
	void *memoryBlock;
};
struct win32_state
{
	uint64 totalSize;
	void* gameMemoryBlock;
	win32_replay_buffer replayBuffers[4];

	HANDLE recordingHandle;
	int inputRecordingIndex;

	HANDLE playBackHandle;
	int inputPlayingIndex;

	char exeFileName[WIN32_STATE_FILE_NAME_COUNT];
	char *onePastLastExeFileNameSlash;
};