#pragma region Typedefs, Structs and Defines
#include "Handmade_Platform.h"

#include <malloc.h>
#include <Windows.h>
#include <Xinput.h>
#include <dsound.h>
#include <stdio.h>

#include "Win32_Handmade.h"


//NOTE: XInputGetState
#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE* pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
	return(ERROR_DEVICE_NOT_CONNECTED);
}
global_var x_input_get_state *XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

//NOTE: XInputSetState
#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
	return(ERROR_DEVICE_NOT_CONNECTED);
}
global_var x_input_set_state *XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID pcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter);
typedef DIRECT_SOUND_CREATE(direct_sound_create);

void CatStrings(size_t sourceACount, char* sourceA, size_t sourceBCount, char* sourceB, size_t destCount, char* dest)
{
	for (int index = 0; index < sourceACount; ++index)
	{
		*dest++ = *sourceA++;
	}
	for (int index = 0; index < sourceBCount; ++index)
	{
		*dest++ = *sourceB++;
	}

	*dest++ = 0;
}

internal_func void Win32GetExeFileName(win32_state *state)
{
	DWORD SizeOfFileName = GetModuleFileNameA(0, state->exeFileName, sizeof(state->exeFileName));
	state->onePastLastExeFileNameSlash = state->exeFileName;
	for (char* scan = state->exeFileName; *scan; ++scan)
	{
		if (*scan == '\\')
		{
			state->onePastLastExeFileNameSlash = scan + 1;
		}
	}
}

internal_func int StringLength(char *string)
{
	int count = 0;
	while (*string++)
	{
		++count;
	}

	return count;
}

internal_func void Win32BuildExePathFileName(win32_state *state, char* fileName, int destCount, char* dest)
{
	CatStrings(state->onePastLastExeFileNameSlash - state->exeFileName, state->exeFileName,
		StringLength(fileName), fileName,
		destCount, dest);
}

DEBUG_PLATFORM_FREE_FILE_MEMORY(DEBUG_PlatformFreeFileMemory)
{
	if (memory) VirtualFree(memory, 0, MEM_RELEASE);
}

DEBUG_PLATFORM_READ_ENTIRE_FILE(DEBUG_PlatformReadEntireFile)
{
	debug_read_file_result result = {};

	HANDLE fileHandle = CreateFileA(fileName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	if(fileHandle != INVALID_HANDLE_VALUE)
	{
		LARGE_INTEGER fileSize;
		if(GetFileSizeEx(fileHandle, &fileSize))
		{
			uint32 fileSize32 = SafeTruncateUint64(fileSize.QuadPart);
			result.contents = VirtualAlloc(0,fileSize32, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
			if(result.contents)
			{
				DWORD bytesRead;
				if(ReadFile(fileHandle, result.contents, fileSize32, &bytesRead, 0) && (fileSize32 == bytesRead))
				{
					result.contentsSize = fileSize32;
				}
				else
				{
					DEBUG_PlatformFreeFileMemory(thread, result.contents);
					result.contents = 0;
				}
			}
			else
			{
				//TODO: Logging
			}
		}
		else
		{
			//TODO: Logging
		}

		CloseHandle(fileHandle);
	}
	else
	{
		//TODO:Logging
	}

	return result;
}

DEBUG_PLATFORM_WRITE_ENTIRE_FILE(DEBUG_PlatformWriteEntireFile)
{
	bool32 result = false;

	HANDLE fileHandle = CreateFileA(fileName, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
	if (fileHandle != INVALID_HANDLE_VALUE)
	{
		DWORD bytesWritten;
		if (WriteFile(fileHandle, memory, memorySize, &bytesWritten, 0))
		{
			result = (bytesWritten == memorySize);
		}
		else
		{
			//TODO: Logging
		}
	}
	else
	{
		//TODO: Logging
	}

	CloseHandle(fileHandle);

	return result;
}
#pragma endregion

//TODO: This is a global for now
global_var bool32 g_Running;
global_var bool32 g_Pause;
global_var win32_offscreen_buffer g_BackBuffer;
global_var LPDIRECTSOUNDBUFFER g_SecondaryBuffer;
global_var int64 g_PerfCountFrequency;
global_var bool32 g_DEBUGShowCursor;
global_var WINDOWPLACEMENT g_WindowPos = { sizeof(g_WindowPos) };

internal_func void ToggleFullscreen(HWND window)
{
	DWORD style = GetWindowLong(window, GWL_STYLE);
	if (style & WS_OVERLAPPEDWINDOW)
	{
		MONITORINFO monitorInfo = { sizeof(monitorInfo) };
		if (GetWindowPlacement(window, &g_WindowPos) && GetMonitorInfo(MonitorFromWindow(window, MONITOR_DEFAULTTOPRIMARY), &monitorInfo))
		{
			SetWindowLong(window, GWL_STYLE, style & ~WS_OVERLAPPEDWINDOW);
			SetWindowPos(window, HWND_TOP, monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top,
				monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
				SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
		}
	}
	else
	{
		SetWindowLong(window, GWL_STYLE, style | WS_OVERLAPPEDWINDOW);
		SetWindowPlacement(window, &g_WindowPos);
		SetWindowPos(window, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
	}
}

inline FILETIME Win32GetLastWriteTime(char* fileName)
{
	FILETIME lastWriteTime = {};
#if 0
	WIN32_FIND_DATA findData;
	HANDLE findHandle = FindFirstFileA(fileName, &findData);
	if(findHandle != INVALID_HANDLE_VALUE)
	{
		lastWriteTime = findData.ftLastWriteTime;
		FindClose(findHandle);
	}
#else
	WIN32_FILE_ATTRIBUTE_DATA data;
	if(GetFileAttributesEx(fileName, GetFileExInfoStandard,&data))
	{
		lastWriteTime = data.ftLastWriteTime;
	}
#endif
	return lastWriteTime;
}

internal_func win32_game_code Win32LoadGameCode(char* sourceDllName, char* tempDllName, char* lockFileName)
{
	win32_game_code result = {};

	WIN32_FILE_ATTRIBUTE_DATA ignored;
	if (!GetFileAttributesEx(lockFileName, GetFileExInfoStandard, &ignored))
	{

		result.dllLastWriteTime = Win32GetLastWriteTime(sourceDllName);
		Assert(CopyFile(sourceDllName, tempDllName, false) != 0);
		result.gameCodeDll = LoadLibraryA(tempDllName);
		if (result.gameCodeDll)
		{
			result.UpdateAndRender = (game_update_and_render*)GetProcAddress(result.gameCodeDll, "GameUpdateAndRender");
			result.GetSoundSamples = (game_get_sound_samples*)GetProcAddress(result.gameCodeDll, "GameGetSoundSamples");

			result.isValid = (result.UpdateAndRender && result.GetSoundSamples);
		}

		if (!result.isValid)
		{
			result.UpdateAndRender = 0;
			result.GetSoundSamples = 0;
		}
	}

	return result;
}

internal_func void Win32UnloadGameCode(win32_game_code* gameCode)
{
	if (gameCode->gameCodeDll)
	{
		FreeLibrary(gameCode->gameCodeDll);
		gameCode->gameCodeDll = 0;
	}

	gameCode->isValid = false;
	gameCode->UpdateAndRender = 0;
	gameCode->GetSoundSamples = 0;
}

internal_func void
Win32LoadXInput()
{
	//TODO: Test this on windows 8
	HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
	if (!XInputLibrary)
	{
		//TODO: Diagnostic
		XInputLibrary = LoadLibraryA("xinput1_3.dll");
	}

	if (XInputLibrary)
	{
		XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
		XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
	}
	else
	{
		//TODO: Diagnostic
	}
}

internal_func void
Win32InitDSound(HWND window, int32 samplesPerSecond, int32 bufferSize)
{
	//Load the library
	HMODULE dSoundLibrary = LoadLibraryA("dsound.dll");
	if (dSoundLibrary)
	{
		//Get a DirectSound Object
		direct_sound_create *DirectSoundCreate = (direct_sound_create*)GetProcAddress(dSoundLibrary, "DirectSoundCreate");

		//TODO: Double-check that this works on XP
		LPDIRECTSOUND directSound;
		if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &directSound, 0)))
		{
			WAVEFORMATEX waveFormat;
			waveFormat.wFormatTag = WAVE_FORMAT_PCM;
			waveFormat.nChannels = 2;
			waveFormat.nSamplesPerSec = samplesPerSecond;
			waveFormat.wBitsPerSample = 16;
			waveFormat.nBlockAlign = (waveFormat.nChannels * waveFormat.wBitsPerSample) / 8;
			waveFormat.nAvgBytesPerSec = waveFormat.nBlockAlign * waveFormat.nSamplesPerSec;
			waveFormat.cbSize = 0;

			if(SUCCEEDED(directSound->SetCooperativeLevel(window, DSSCL_PRIORITY)))
			{
				DSBUFFERDESC bufferDesc = {};
				bufferDesc.dwSize = sizeof(bufferDesc);
				bufferDesc.dwFlags = DSBCAPS_PRIMARYBUFFER;

				//'Create' a primary buffer
				//TODO: DSBCAPS_GLOBALFOCUS?
				LPDIRECTSOUNDBUFFER primaryBuffer;
				if(SUCCEEDED(directSound->CreateSoundBuffer(&bufferDesc, &primaryBuffer, 0)))
				{
					HRESULT result = primaryBuffer->SetFormat(&waveFormat);

					if (SUCCEEDED(result))
					{
						//We have finally set the format
						OutputDebugStringA("Primary buffer format was set!\n");
					}
					else
					{
						//TODO: Diagnostic
					}
				}
				else
				{
					//TODO: Diagnostic
				}
			}
			else
			{
				//TODO: Diagnostic
			}

			//'Create' a secondary buffer
			//TODO: DSBCAPS_GETCURRENTPOSITION2
			DSBUFFERDESC bufferDesc = {};
			bufferDesc.dwSize = sizeof(bufferDesc);
			bufferDesc.dwFlags = 0;
			bufferDesc.dwBufferBytes = bufferSize;
			bufferDesc.lpwfxFormat = &waveFormat;

			HRESULT result = directSound->CreateSoundBuffer(&bufferDesc, &g_SecondaryBuffer, 0);
			if (SUCCEEDED(result))
			{
				OutputDebugStringA("Secondary buffer created succesfully!\n");
			}
		}
		else
		{
			//TODO: Diagnostic
		}
	}
	else
	{
		//TODO: Diagnostic
	}
}

internal_func win32_window_dimension 
Win32GetWindowDimension(HWND window)
{
	win32_window_dimension result;

	RECT clientRect;
	GetClientRect(window, &clientRect);
	result.width = clientRect.right - clientRect.left;
	result.height = clientRect.bottom - clientRect.top;
	return result;
}

internal_func void
Win32ResizeDIBSection(win32_offscreen_buffer *buffer, int width, int height)
{
	//TODO: Bulletproof this
	//Maybe dont free first, free after, then free first if that fails (idk what this means lol)

	if (buffer->memory)
	{
		VirtualFree(buffer->memory, 0, MEM_RELEASE);
	}

	buffer->width = width;
	buffer->height = height;
	
	int bytesPerPixel = 4;
	buffer->bytesPerPixel = bytesPerPixel;

	buffer->info.bmiHeader.biSize = sizeof(buffer->info.bmiHeader);
	buffer->info.bmiHeader.biWidth = buffer->width;
	buffer->info.bmiHeader.biHeight = -buffer->height;
	buffer->info.bmiHeader.biPlanes = 1;
	buffer->info.bmiHeader.biBitCount = 32;
	buffer->info.bmiHeader.biCompression = BI_RGB;

	int bitmapMemorySize = (width * height) * bytesPerPixel;
	buffer->memory = VirtualAlloc(0, bitmapMemorySize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
	buffer->pitch = width * bytesPerPixel;

	//TODO: Probably clear this to black
}

internal_func void
Win32DisplayBufferInWindow(win32_offscreen_buffer *buffer, HDC deviceContext, int windowWidth, int windowHeight)
{
	if ((windowWidth == buffer->width * 2) && (windowHeight == buffer->height * 2))
	{
		StretchDIBits(deviceContext,
			0, 0, 2*buffer->width, 2*buffer->height,
			0, 0, buffer->width, buffer->height,
			buffer->memory,
			&buffer->info,
			DIB_RGB_COLORS, SRCCOPY
		);
	}
	else
	{
		int offsetX = 10;
		int offsetY = 10;

		PatBlt(deviceContext, 0, 0, windowWidth, offsetY, BLACKNESS);
		PatBlt(deviceContext, 0, offsetY + buffer->height, windowWidth, windowHeight, BLACKNESS);
		PatBlt(deviceContext, 0, 0, offsetX, windowHeight, BLACKNESS);
		PatBlt(deviceContext, offsetX + buffer->width, 0, windowWidth, windowHeight, BLACKNESS);

		StretchDIBits(deviceContext,
			offsetX, offsetY, buffer->width, buffer->height,
			0, 0, buffer->width, buffer->height,
			buffer->memory,
			&buffer->info,
			DIB_RGB_COLORS, SRCCOPY
		);
	}
}

LRESULT CALLBACK 
Win32MainWindowCallback(	HWND	window,
			UINT   uMsg,
			WPARAM wParam,
			LPARAM lParam)
{
	LRESULT result = 0;

	switch (uMsg)
	{
		case WM_CLOSE:
		{
		//TODO: Handle this with a message to the user?
		g_Running = false;
		} break;

		case WM_SETCURSOR:
		{
			if (g_DEBUGShowCursor)
			{
				result = DefWindowProcA(window, uMsg, wParam, lParam);
			}
			else
			{
				SetCursor(0);
			}
		} break;

		case WM_ACTIVATEAPP:
		{
#if 0
			if(wParam == TRUE)
			{
				SetLayeredWindowAttributes(window, RGB(0, 0, 0), 255, LWA_ALPHA);
			}
			else
			{
				SetLayeredWindowAttributes(window, RGB(0, 0, 0), 128, LWA_ALPHA);
			}
#endif
		} break;

		case WM_DESTROY:
		{
			//TODO: Handle this as an error - recreate window?
			g_Running = false;
		} break;

		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP:
		{
			Assert(!"Keyboard input came in through non-dispatch message!");
		}	break;

		case WM_PAINT:
		{
			PAINTSTRUCT paint;
			HDC deviceContext = BeginPaint(window, &paint);
			win32_window_dimension dimension = Win32GetWindowDimension(window);
			Win32DisplayBufferInWindow(&g_BackBuffer, deviceContext, dimension.width, dimension.height);

			EndPaint(window, &paint);
		} break;

		default:
		{
			result = DefWindowProc(window,uMsg,wParam,lParam);
		} break;
	}

	return result;
}

internal_func void Win32ClearBuffer(win32_sound_output *soundOutput)
{
	void *pRegion1;
	DWORD region1Size;
	void *pRegion2;
	DWORD region2Size;

	if (SUCCEEDED(g_SecondaryBuffer->Lock(0, soundOutput->secondaryBufferSize, &pRegion1, &region1Size, &pRegion2, &region2Size, 0)))
	{
		uint8 *pDestSample = (uint8*)pRegion1;
		for (DWORD byteIndex = 0; byteIndex < region1Size; ++byteIndex)
		{
			*pDestSample++ = 0;
		}

		pDestSample = (uint8*)pRegion2;
		for (DWORD byteIndex = 0; byteIndex < region2Size; ++byteIndex)
		{
			*pDestSample++ = 0;
		}

		g_SecondaryBuffer->Unlock(pRegion1, region1Size, pRegion2, region2Size);
	}
}


internal_func void Win32FillSoundBuffer(win32_sound_output *soundOutput, DWORD byteToLock, DWORD bytesToWrite, game_soundOutput_buffer *sourceBuffer)
{
	void *pRegion1;
	DWORD region1Size;
	void *pRegion2;
	DWORD region2Size;

	if (SUCCEEDED(g_SecondaryBuffer->Lock(byteToLock, bytesToWrite, &pRegion1, &region1Size, &pRegion2, &region2Size, 0)))
	{
		//TODO: assert that region1Size/region2Size is valid
		DWORD region1SampleCount = region1Size / soundOutput->bytesPerSample;
		int16 *pDestSample = (int16 *)pRegion1;
		int16 *pSrcSample = sourceBuffer->pSamples;
		for (DWORD sampleIndex = 0; sampleIndex < region1SampleCount; ++sampleIndex)
		{
			*pDestSample++ = *pSrcSample++;
			*pDestSample++ = *pSrcSample++;
			++soundOutput->runningSampleIndex;
		}

		DWORD region2SampleCount = region2Size / soundOutput->bytesPerSample;
		pDestSample = (int16 *)pRegion2;
		for (DWORD sampleIndex = 0; sampleIndex < region2SampleCount; ++sampleIndex)
		{
			*pDestSample++ = *pSrcSample++;
			*pDestSample++ = *pSrcSample++;
			++soundOutput->runningSampleIndex;
		}

		g_SecondaryBuffer->Unlock(pRegion1, region1Size, pRegion2, region2Size);
	}
}

internal_func void Win32ProcessKeyboardMessage(game_button_state* newState, bool32 isDown)
{
	if (newState->endedDown != isDown)
	{
		newState->endedDown = isDown;
		++newState->halfTransitionCount;
	}
}

internal_func void Win32ProcessXInputDigitalButton(DWORD xInputButtonState, game_button_state* oldState, DWORD buttonBit, game_button_state* newState)
{
	newState->endedDown = ((xInputButtonState & buttonBit) == buttonBit);
	newState->halfTransitionCount = (oldState->endedDown == newState->endedDown) ? 1 : 0;
}

internal_func real32 Win32ProcessXInputStickValue(SHORT value, SHORT deadZoneThreshold)
{
	real32 result = 0;
	if (value < -deadZoneThreshold)
	{
		result =  (real32)value / 32768.f;
	}
	else if (value > deadZoneThreshold)
	{
		result = (real32)value / 32767.f;
	}

	return result;
}

internal_func void Win32GetInputFileLocation(win32_state *state, bool inputStream, int slotIndex, int destCount, char* dest)
{
	char temp[64];
	wsprintf(temp, "loop_edit_%d_%s.hmi", slotIndex, inputStream? "input" : "state");
	Win32BuildExePathFileName(state, temp, destCount, dest);
}

internal_func win32_replay_buffer *Win32GetReplayBuffer(win32_state *state, int unsigned index)
{
	Assert(index < ArrayCount(state->replayBuffers));
	win32_replay_buffer *result = &state->replayBuffers[index];
	return result;
}

internal_func void Win32BeginRecordingInput(win32_state* state, int inputRecordingIndex)
{
	win32_replay_buffer *replayBuffer = Win32GetReplayBuffer(state, inputRecordingIndex);
	if (replayBuffer->memoryBlock)
	{
		state->inputRecordingIndex = inputRecordingIndex;

		char fileName[WIN32_STATE_FILE_NAME_COUNT];
		Win32GetInputFileLocation(state, true, inputRecordingIndex, sizeof(fileName), fileName);

		state->recordingHandle = CreateFileA(fileName, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
#if 0
		LARGE_INTEGER filePosition;
		filePosition.QuadPart = state->totalSize;
		SetFilePointerEx(state->recordingHandle, filePosition, 0, FILE_BEGIN);
#endif

		CopyMemory(replayBuffer->memoryBlock, state->gameMemoryBlock, state->totalSize);
	}
}

internal_func void Win32EndRecordingInput(win32_state* win32State)
{
	CloseHandle(win32State->recordingHandle);
	win32State->inputRecordingIndex = 0;
}

internal_func void Win32BeginInputPlayback(win32_state* state, int inputPlayingIndex)
{
	win32_replay_buffer* replayBuffer = Win32GetReplayBuffer(state, inputPlayingIndex);
	if (replayBuffer->memoryBlock)
	{
		state->inputPlayingIndex = inputPlayingIndex;

		char fileName[WIN32_STATE_FILE_NAME_COUNT];
		Win32GetInputFileLocation(state, true, inputPlayingIndex, sizeof(fileName), fileName);
		state->playBackHandle = CreateFileA(fileName, GENERIC_READ, 0, 0, OPEN_EXISTING, 0, 0);

#if 0
		LARGE_INTEGER filePosition;
		filePosition.QuadPart = state->totalSize;
		SetFilePointerEx(state->playBackHandle, filePosition, 0, FILE_BEGIN);
#endif

		CopyMemory(state->gameMemoryBlock, replayBuffer->memoryBlock, state->totalSize);
	}
}

internal_func void Win32EndInputPlayback(win32_state* win32State)
{
	CloseHandle(win32State->playBackHandle);
	win32State->inputPlayingIndex = 0;
}

internal_func void Win32RecordInput(win32_state* win32State, game_input *input)
{
	DWORD bytesWritten;
	WriteFile(win32State->recordingHandle, input, sizeof(*input), &bytesWritten, 0);
}

internal_func void Win32PlayBackInput(win32_state* win32State, game_input* input)
{
	DWORD bytesRead;
	if (ReadFile(win32State->playBackHandle, input, sizeof(*input), &bytesRead, 0))
	{
		if (bytesRead == 0)
		{
			int playingIndex = win32State->inputPlayingIndex;
			Win32EndInputPlayback(win32State);
			Win32BeginInputPlayback(win32State, playingIndex);
		}
	}
}

internal_func void Win32ProcessPendingMessages(win32_state *win32State, game_controller_input* keyboardController)
{
	MSG message;
	while (PeekMessageA(&message, 0, 0, 0, PM_REMOVE))
	{
		switch (message.message)
		{
		case WM_QUIT:
		{
			g_Running = false;
		} break;
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_KEYDOWN:
		case WM_KEYUP:
		{
			uint32 vkCode = (uint32)message.wParam;
			bool wasDown = (message.lParam & (1u << 30u)) != 0;
			bool isDown = (message.lParam & (1u << 31u)) == 0;

			if (isDown != wasDown)
			{
				if (vkCode == 'W')
				{
					Win32ProcessKeyboardMessage(&keyboardController->moveUp, isDown);
				}
				else if (vkCode == 'A')
				{
					Win32ProcessKeyboardMessage(&keyboardController->moveLeft, isDown);
				}
				else if (vkCode == 'S')
				{
					Win32ProcessKeyboardMessage(&keyboardController->moveDown, isDown);
				}
				else if (vkCode == 'D')
				{
					Win32ProcessKeyboardMessage(&keyboardController->moveRight, isDown);
				}
				else if (vkCode == 'Q')
				{
					Win32ProcessKeyboardMessage(&keyboardController->leftShoulder, isDown);
				}
				else if (vkCode == 'E')
				{
					Win32ProcessKeyboardMessage(&keyboardController->rightShoulder, isDown);
				}
				else if (vkCode == VK_UP)
				{
					Win32ProcessKeyboardMessage(&keyboardController->actionUp, isDown);
				}
				else if (vkCode == VK_LEFT)
				{
					Win32ProcessKeyboardMessage(&keyboardController->actionLeft, isDown);
				}
				else if (vkCode == VK_DOWN)
				{
					Win32ProcessKeyboardMessage(&keyboardController->actionDown, isDown);
				}
				else if (vkCode == VK_RIGHT)
				{
					Win32ProcessKeyboardMessage(&keyboardController->actionRight, isDown);
				}
				else if (vkCode == VK_ESCAPE)
				{
					Win32ProcessKeyboardMessage(&keyboardController->start, isDown);
				}
				else if (vkCode == VK_SPACE)
				{
					Win32ProcessKeyboardMessage(&keyboardController->back, isDown);
				}
#if HANDMADE_INTERNAL
				else if (vkCode == 'P')
				{
					if (isDown)
					{
						g_Pause = !g_Pause;
					}
				}
				else if (vkCode == 'L')
				{
					if (isDown)
					{
						if (win32State->inputPlayingIndex == 0)
						{
							if (win32State->inputRecordingIndex == 0)
							{
								Win32BeginRecordingInput(win32State, 1);
							}
							else
							{
								Win32EndRecordingInput(win32State);
								Win32BeginInputPlayback(win32State, 1);
							}
						}
						else
						{
							Win32EndInputPlayback(win32State);
						}
					}
				}
#endif
				if (isDown)
				{
					//Alt + F4 functionality
					bool altKeyWasDown = (message.lParam & (1 << 29));
					if ((vkCode == VK_F4) && altKeyWasDown)
					{
						g_Running = false;
					}
					if ((vkCode == VK_RETURN) && altKeyWasDown)
					{
						if (message.hwnd)
						{
							ToggleFullscreen(message.hwnd);
						}
					}
				}

			}
		} break;
		default:
		{
			TranslateMessage(&message);
			DispatchMessage(&message);
		} break;
		}
	}
}


inline LARGE_INTEGER Win32GetWallClock()
{
	LARGE_INTEGER result;
	QueryPerformanceCounter(&result);
	return result;
}

inline real32 Win32GetSecondsElapsed(LARGE_INTEGER start, LARGE_INTEGER end)
{
	return (real32)(end.QuadPart - start.QuadPart) / (real32)g_PerfCountFrequency;
}

internal_func void Win32DebugDrawVertical(win32_offscreen_buffer* backBuffer, int x, int top, int bottom, uint32 color)
{
	if (top <= 0) top = 0;
	if (bottom > backBuffer->height) bottom = backBuffer->height - 1;

	if (x >= 0 && x < backBuffer->width)
	{
		uint8* pixel = (uint8*)backBuffer->memory + x * backBuffer->bytesPerPixel + top * backBuffer->pitch;
		for (int y = top; y < bottom; ++y)
		{
			*(uint32*)pixel = color;
			pixel += backBuffer->pitch;
		}
	}
}

inline void Win32DrawSoundBufferMarker(win32_offscreen_buffer *backBuffer, win32_sound_output *soundOutput, real32 c, int padX, int top, int bottom, DWORD value, uint32 color)
{
		int x = (int)(padX + (real32)(c * value));
		if ((x >= 0) && (x < backBuffer->width))
		{
			Win32DebugDrawVertical(backBuffer, x, top, bottom, color);
		}
}

#if 0
internal_func void Win32DebugSyncDisplay(win32_offscreen_buffer *backBuffer,int markerCount ,win32_debug_time_marker *markers,
	int currentMarkerIndex,
	win32_sound_output *soundOutput, real32 targetSecondsPerFrame)
{
	int padX = 16;
	int padY = 16;

	int lineHeight = 64;

	real32 c = (real32)(backBuffer->width - 2*padX )/ (real32)soundOutput->secondaryBufferSize;
	for(int markerIndex = 0; markerIndex< markerCount; ++markerIndex)
	{
		win32_debug_time_marker *thisMarker = &markers[markerIndex];
		Assert(thisMarker->outputPlayCursor < soundOutput->secondaryBufferSize);
		Assert(thisMarker->outputWriteCursor < soundOutput->secondaryBufferSize);
		Assert(thisMarker->outputLocation < soundOutput->secondaryBufferSize);
		Assert(thisMarker->outputByteCount < soundOutput->secondaryBufferSize);
		Assert(thisMarker->flipPlayCursor < soundOutput->secondaryBufferSize);
		Assert(thisMarker->flipWriteCursor < soundOutput->secondaryBufferSize);


		DWORD playColor = 0xFFFFFFFF;
		DWORD writeColor = 0xFFFF0000;
		DWORD expectedFlipColor = 0xFFFFFF00;
		DWORD playWindowColor = 0xFFFF00FF;
		int top = padY;
		int bottom = padY + lineHeight;

		if(markerIndex == currentMarkerIndex)
		{
			top += lineHeight + padY;
			bottom += lineHeight + padY;

			int firstTop = top;

			Win32DrawSoundBufferMarker(backBuffer, soundOutput, c, padX, top, bottom, thisMarker->outputPlayCursor, playColor);
			Win32DrawSoundBufferMarker(backBuffer, soundOutput, c, padX, top, bottom, thisMarker->outputWriteCursor, writeColor);

			top += lineHeight + padY;
			bottom += lineHeight + padY;

			Win32DrawSoundBufferMarker(backBuffer, soundOutput, c, padX, top, bottom, thisMarker->outputLocation, playColor);
			Win32DrawSoundBufferMarker(backBuffer, soundOutput, c, padX, top, bottom, thisMarker->outputLocation + thisMarker->outputByteCount, writeColor);

			top += lineHeight + padY;
			bottom += lineHeight + padY;

			Win32DrawSoundBufferMarker(backBuffer, soundOutput, c, padX, firstTop, bottom, thisMarker->expectedFlipPlayCursor, expectedFlipColor);
		}

		Win32DrawSoundBufferMarker(backBuffer, soundOutput, c, padX, top, bottom, thisMarker->flipPlayCursor, playColor);
		Win32DrawSoundBufferMarker(backBuffer, soundOutput, c, padX, top, bottom, thisMarker->flipPlayCursor + 480*soundOutput->bytesPerSample, playWindowColor);
		Win32DrawSoundBufferMarker(backBuffer, soundOutput, c, padX, top, bottom, thisMarker->flipWriteCursor, writeColor);

	}
}
#endif

int CALLBACK 
WinMain(HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR     lpCmdLine,
	int       nCmdShow)
{
	win32_state win32State = {};

	LARGE_INTEGER perfCountFrequencyResult;
	QueryPerformanceFrequency(&perfCountFrequencyResult);
	g_PerfCountFrequency = perfCountFrequencyResult.QuadPart;

	Win32GetExeFileName(&win32State);

	char sourceGameCodeDllFullPath[WIN32_STATE_FILE_NAME_COUNT];
	Win32BuildExePathFileName(&win32State, "Handmade.dll", sizeof(sourceGameCodeDllFullPath), sourceGameCodeDllFullPath);

	char tempGameCodeDllFullPath[WIN32_STATE_FILE_NAME_COUNT];
	Win32BuildExePathFileName(&win32State, "Handmade_Temp.dll", sizeof(tempGameCodeDllFullPath), tempGameCodeDllFullPath);

	char lockGameCodeDllFullPath[WIN32_STATE_FILE_NAME_COUNT];
	Win32BuildExePathFileName(&win32State, "lock.tmp", sizeof(lockGameCodeDllFullPath), lockGameCodeDllFullPath);

	//Set the windows scheduler granularity to 1 ms so that our Sleep() can be more granular
	UINT desiredSchedulerMS = 1;
	bool32 sleepIsGranular = (timeBeginPeriod(desiredSchedulerMS) == TIMERR_NOERROR);

	Win32LoadXInput();

#if HANDMADE_INTERNAL
	g_DEBUGShowCursor = true;
#endif
	WNDCLASS wndClass = {};

	Win32ResizeDIBSection(&g_BackBuffer, 960, 540);

	wndClass.style = CS_HREDRAW | CS_VREDRAW;
	wndClass.lpfnWndProc = Win32MainWindowCallback;
	wndClass.hInstance = hInstance;
	wndClass.hCursor = LoadCursor(0, IDC_ARROW);
	//wndClass.hIcon = ;
	wndClass.lpszClassName = "HandmadeHeroWindowClass";

	if (RegisterClass(&wndClass))
	{
		HWND window = CreateWindowEx(
			0, wndClass.lpszClassName, "Handmade Hero", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, 0, 0, hInstance, 0);
		if (window)
		{
			HDC refreshDC = GetDC(window);
			int monitorRefreshHz = 60;
			int win32RefreshRate = GetDeviceCaps(refreshDC, VREFRESH);
			if (win32RefreshRate > 1)
			{
				monitorRefreshHz = win32RefreshRate;
			}
			real32 gameUpdateHz = (real32)(monitorRefreshHz / 2);
			real32 targetSecondsPerFrame = 1.0f / (real32)gameUpdateHz;

			win32_sound_output soundOutput = {};
			soundOutput.samplesPerSec = 48000;
			soundOutput.bytesPerSample = sizeof(int16) * 2;
			soundOutput.secondaryBufferSize = soundOutput.samplesPerSec * soundOutput.bytesPerSample;
			soundOutput.safetyBytes = (int)((real32)(soundOutput.samplesPerSec*soundOutput.bytesPerSample) / gameUpdateHz) / 2;

			Win32InitDSound(window, soundOutput.samplesPerSec, soundOutput.secondaryBufferSize);
			Win32ClearBuffer(&soundOutput);
			g_SecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

			win32State.inputRecordingIndex = 0;
			win32State.inputPlayingIndex = 0;
			g_Running = true;

			int16 *samples = (int16 *)VirtualAlloc(0, soundOutput.secondaryBufferSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

#if HANDMADE_INTERNAL
			LPVOID baseAdress = (LPVOID)Terabytes((uint64)2);
#else
			LPVOID baseAdress = 0;
#endif

			game_memory gameMemory{};
			gameMemory.permanentStorageSize = Megabytes(64);
			gameMemory.transientStorageSize = Gigabytes((uint64)1);
			gameMemory.DEBUG_PlatformFreeFileMemory = DEBUG_PlatformFreeFileMemory;
			gameMemory.DEBUG_PlatformReadEntireFile = DEBUG_PlatformReadEntireFile;
			gameMemory.DEBUG_PlatformWriteEntireFile = DEBUG_PlatformWriteEntireFile;


			win32State.totalSize = gameMemory.permanentStorageSize + gameMemory.transientStorageSize;
			win32State.gameMemoryBlock = VirtualAlloc(baseAdress, SIZE_T(win32State.totalSize), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
			gameMemory.pPermanentStorage = win32State.gameMemoryBlock;
			gameMemory.pTransientStorage = ((uint8*)gameMemory.pPermanentStorage + gameMemory.permanentStorageSize);

			for(int replayIndex = 0; replayIndex < ArrayCount(win32State.replayBuffers); replayIndex++)
			{
				win32_replay_buffer *replayBuffer = &win32State.replayBuffers[replayIndex];

				char fileName[WIN32_STATE_FILE_NAME_COUNT];
				Win32GetInputFileLocation(&win32State, false, replayIndex, sizeof(replayBuffer->fileName), replayBuffer->fileName);

				replayBuffer->fileHandle = CreateFileA(fileName, GENERIC_READ|GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);

				DWORD maxSizeHigh = (win32State.totalSize >> 32);
				DWORD maxSizeLow = win32State.totalSize & 0xFFFFFFFF;
				replayBuffer->memoryMap = CreateFileMapping(replayBuffer->fileHandle, 0, PAGE_READWRITE, maxSizeHigh, maxSizeLow, 0);
				replayBuffer->memoryBlock = MapViewOfFile(replayBuffer->memoryMap, FILE_MAP_ALL_ACCESS, 0, 0, win32State.totalSize);

				if (replayBuffer->memoryBlock)
				{
				}
				else
				{
					//TODO: Diagnostic
				}
				Assert(replayBuffer->memoryBlock);
			}

			if (samples && gameMemory.pPermanentStorage && gameMemory.pTransientStorage)
			{
				game_input input[2]{};
				game_input* newInput = &input[0];
				game_input* oldInput = &input[1];

				LARGE_INTEGER lastCounter = Win32GetWallClock();
				LARGE_INTEGER flipWallClock = Win32GetWallClock();

				int debugTimeMarkerIndex = 0;
				win32_debug_time_marker debugTimeMarkers[30] = {0};

				//TODO: Handle startup specially
				DWORD audioLatencyBytes;
				real32 audioLatencySeconds;
				bool32 soundIsValid = false;

				win32_game_code game = Win32LoadGameCode(sourceGameCodeDllFullPath, tempGameCodeDllFullPath, lockGameCodeDllFullPath);

				uint64 lastCycleCount = __rdtsc();
				while (g_Running)
				{
					newInput->dtForFrame = targetSecondsPerFrame;

					FILETIME newDllWriteTime = Win32GetLastWriteTime(sourceGameCodeDllFullPath);
					if(CompareFileTime(&newDllWriteTime, &game.dllLastWriteTime) != 0)
					{
						Win32UnloadGameCode(&game);
						game = Win32LoadGameCode(sourceGameCodeDllFullPath, tempGameCodeDllFullPath, lockGameCodeDllFullPath);
					}

					//TODO: Zeroing macro
					game_controller_input* oldKeyboardController = GetController(oldInput, 0);
					game_controller_input* newKeyboardController = GetController(newInput, 0);
					game_controller_input zeroController = {};
					*newKeyboardController = zeroController;
					newKeyboardController->isConnected = true;
					for (int buttonIndex = 0; buttonIndex < ArrayCount(newKeyboardController->buttons); buttonIndex++)
					{
						newKeyboardController->buttons[buttonIndex].endedDown = oldKeyboardController->buttons[buttonIndex].endedDown;
					}

					Win32ProcessPendingMessages(&win32State, newKeyboardController);

					if (!g_Pause)
					{
						POINT mousePos;
						GetCursorPos(&mousePos);
						ScreenToClient(window, &mousePos);

						newInput->mouseX = mousePos.x;
						newInput->mouseY = mousePos.y;
						newInput->mouseZ = 0; //TODO: Support mousewheel?
						Win32ProcessKeyboardMessage(&newInput->mouseButtons[0], GetKeyState(VK_LBUTTON) & (1 << 15));
						Win32ProcessKeyboardMessage(&newInput->mouseButtons[1], GetKeyState(VK_MBUTTON) & (1 << 15));
						Win32ProcessKeyboardMessage(&newInput->mouseButtons[2], GetKeyState(VK_RBUTTON) & (1 << 15));
						Win32ProcessKeyboardMessage(&newInput->mouseButtons[3], GetKeyState(VK_XBUTTON1) & (1 << 15));
						Win32ProcessKeyboardMessage(&newInput->mouseButtons[4], GetKeyState(VK_XBUTTON2) & (1 << 15));

						//TODO: Should we pull this more frequently
						unsigned int maxControllerCount = XUSER_MAX_COUNT;
						if (maxControllerCount > (ArrayCount(newInput->controllers) - 1))
						{
							maxControllerCount = ArrayCount(newInput->controllers);
						}
						for (DWORD controllerIndex = 0; controllerIndex < maxControllerCount; ++controllerIndex)
						{
							int ourControllerIndex = controllerIndex + 1;
							game_controller_input* oldController = GetController(oldInput, ourControllerIndex);
							game_controller_input* newController = GetController(newInput, ourControllerIndex);

							XINPUT_STATE controllerState;
							if (XInputGetState(controllerIndex, &controllerState) == ERROR_SUCCESS)
							{
								newController->isConnected = true;
								newController->isAnalog = oldController->isAnalog;

								//NOTE: This controller is plugged in
								//TODO: See if controllerState.dwPacketNumber increments too rapidly
								XINPUT_GAMEPAD *pad = &controllerState.Gamepad;

								newController->stickAverageX = Win32ProcessXInputStickValue(pad->sThumbLX, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
								newController->stickAverageY = Win32ProcessXInputStickValue(pad->sThumbLY, XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);;

								if(newController->stickAverageX != 0.0f || newController->stickAverageY != 0.0f)
								{
									newController->isAnalog = true;
								}
								if (pad->wButtons & XINPUT_GAMEPAD_DPAD_UP)
								{
									newController->stickAverageY = 1.0f;
									newController->isAnalog = false;
								}
								if (pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN)
								{
									newController->stickAverageY = -1.0f;
									newController->isAnalog = false;
								}
								if (pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT)
								{
									newController->stickAverageX = 1.0f;
									newController->isAnalog = false;
								}
								if (pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT)
								{
									newController->stickAverageX = -1.0f;
									newController->isAnalog = false;
								}

								real32 threshold = 0.5f;
								Win32ProcessXInputDigitalButton((newController->stickAverageX < -threshold) ? 1 : 0,
									&oldController->moveLeft, 1, &newController->moveLeft);
								Win32ProcessXInputDigitalButton((newController->stickAverageX > threshold) ? 1 : 0,
									&oldController->moveRight, 1, &newController->moveRight);
								Win32ProcessXInputDigitalButton((newController->stickAverageY < -threshold) ? 1 : 0,
									&oldController->moveDown, 1, &newController->moveDown);
								Win32ProcessXInputDigitalButton((newController->stickAverageY > threshold) ? 1 : 0,
									&oldController->moveUp, 1, &newController->moveUp);

								Win32ProcessXInputDigitalButton(pad->wButtons, &oldController->actionDown, XINPUT_GAMEPAD_A, &newController->actionDown);
								Win32ProcessXInputDigitalButton(pad->wButtons, &oldController->actionRight, XINPUT_GAMEPAD_B, &newController->actionRight);
								Win32ProcessXInputDigitalButton(pad->wButtons, &oldController->actionLeft, XINPUT_GAMEPAD_X, &newController->actionLeft);
								Win32ProcessXInputDigitalButton(pad->wButtons, &oldController->actionUp, XINPUT_GAMEPAD_Y, &newController->actionUp);
								Win32ProcessXInputDigitalButton(pad->wButtons, &oldController->leftShoulder, XINPUT_GAMEPAD_LEFT_SHOULDER, &newController->leftShoulder);
								Win32ProcessXInputDigitalButton(pad->wButtons, &oldController->rightShoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER, &newController->rightShoulder);
								Win32ProcessXInputDigitalButton(pad->wButtons, &oldController->start, XINPUT_GAMEPAD_START, &newController->start);
								Win32ProcessXInputDigitalButton(pad->wButtons, &oldController->back, XINPUT_GAMEPAD_BACK, &newController->back);

							}
							else
							{
								//NOTE: The controller is not available
								newController->isConnected = false;
							}
						}
						thread_context thread = {};

						game_offscreen_buffer buffer = {};
						buffer.memory = g_BackBuffer.memory;
						buffer.width = g_BackBuffer.width;
						buffer.height = g_BackBuffer.height;
						buffer.pitch = g_BackBuffer.pitch;
						buffer.bytesPerPixel = g_BackBuffer.bytesPerPixel;

						if(win32State.inputRecordingIndex)
						{
							Win32RecordInput(&win32State, newInput);
						}

						if(win32State.inputPlayingIndex)
						{
							Win32PlayBackInput(&win32State, newInput);
						}
						if (game.UpdateAndRender)
						{
							game.UpdateAndRender(&thread, &gameMemory, newInput, &buffer);
						}

						LARGE_INTEGER audioWallClock = Win32GetWallClock();
						real32 fromBeginToAudioSeconds = Win32GetSecondsElapsed(flipWallClock, audioWallClock);

						DWORD playCursor;
						DWORD writeCursor;
						win32_debug_time_marker* marker = 0;
						if (g_SecondaryBuffer->GetCurrentPosition(&playCursor, &writeCursor) == DS_OK)
						{
							if (!soundIsValid)
							{
								soundOutput.runningSampleIndex = writeCursor / soundOutput.bytesPerSample;
								soundIsValid = true;
							}

							DWORD byteToLock = (soundOutput.runningSampleIndex * soundOutput.bytesPerSample) % soundOutput.secondaryBufferSize;

							DWORD expectedSoundBytesPerFrame = (DWORD)((soundOutput.samplesPerSec * soundOutput.bytesPerSample) / gameUpdateHz);
							real32 secondsLeftUntilFlip = (targetSecondsPerFrame - fromBeginToAudioSeconds);
							DWORD expectedBytesUntilFlip = (DWORD)(secondsLeftUntilFlip * (real32)expectedSoundBytesPerFrame);
							DWORD expectedFrameBoundaryByte = playCursor + expectedBytesUntilFlip;

							DWORD safeWriteCursor = writeCursor;
							if (safeWriteCursor < playCursor)
							{
								safeWriteCursor += soundOutput.secondaryBufferSize;
							}
							Assert(safeWriteCursor >= playCursor);
							safeWriteCursor += soundOutput.safetyBytes;
							bool32 audioCardIsLowLatency = (safeWriteCursor < expectedFrameBoundaryByte);

							DWORD targetCursor = 0;
							if (audioCardIsLowLatency)
							{
								targetCursor = expectedFrameBoundaryByte + expectedSoundBytesPerFrame;
							}
							else
							{
								targetCursor = writeCursor + expectedSoundBytesPerFrame + soundOutput.safetyBytes;
							}
							targetCursor = targetCursor % soundOutput.secondaryBufferSize;

							DWORD bytesToWrite = 0;
							if (byteToLock > targetCursor)
							{
								bytesToWrite = (soundOutput.secondaryBufferSize - byteToLock);
								bytesToWrite += targetCursor;
							}
							else
							{
								bytesToWrite = targetCursor - byteToLock;
							}

							game_soundOutput_buffer soundBuffer = {};
							soundBuffer.samplesPerSecond = soundOutput.samplesPerSec;
							soundBuffer.sampleCount = bytesToWrite / soundOutput.bytesPerSample;
							soundBuffer.pSamples = samples;
							if (game.GetSoundSamples)
							{
								game.GetSoundSamples(&thread, &gameMemory, &soundBuffer);
							}

	#if HANDMADE_INTERNAL
							g_SecondaryBuffer->GetCurrentPosition(&playCursor, &writeCursor);

							marker = &debugTimeMarkers[debugTimeMarkerIndex];
							marker->outputPlayCursor = playCursor;
							marker->outputWriteCursor = writeCursor;
							marker->outputLocation = byteToLock;
							marker->outputByteCount = bytesToWrite;
							marker->expectedFlipPlayCursor = expectedFrameBoundaryByte;

							DWORD unwrappedWriteCursor = writeCursor;
							if (unwrappedWriteCursor < playCursor)
							{
								unwrappedWriteCursor += soundOutput.secondaryBufferSize;
							}
							audioLatencyBytes = unwrappedWriteCursor - playCursor;
							audioLatencySeconds = (((real32)audioLatencyBytes / (real32)soundOutput.bytesPerSample) / (real32)soundOutput.samplesPerSec);

#if 0
							char textBuffer[256];
							sprintf_s(textBuffer, sizeof(textBuffer), "BTL: %u TC: %u BTW: %u - PC: %u WC: %u DELTA: %u (%fs)\n",
								byteToLock, targetCursor, bytesToWrite, playCursor, writeCursor, audioLatencyBytes, audioLatencySeconds);
							OutputDebugStringA(textBuffer);
#endif
	#endif
							Win32FillSoundBuffer(&soundOutput, byteToLock, bytesToWrite, &soundBuffer);
						}
						else
						{
							soundIsValid = false;
						}

						LARGE_INTEGER workCounter = Win32GetWallClock();

						real32 workSecondsElapsed = Win32GetSecondsElapsed(lastCounter, workCounter);

						real32 secondsElapsedForFrame = workSecondsElapsed;
						if (secondsElapsedForFrame < targetSecondsPerFrame)
						{
							if (sleepIsGranular)
							{
								DWORD sleepMs = (DWORD)(1000.f * (targetSecondsPerFrame - secondsElapsedForFrame));

								if (sleepMs > 0)
								{
									Sleep(sleepMs);
								}
							}

							real32 testSecondsElapsedForFrame = Win32GetSecondsElapsed(lastCounter, Win32GetWallClock());

							if (testSecondsElapsedForFrame < targetSecondsPerFrame)
							{
								//TODO: Log missed sleep here
							}

							while (secondsElapsedForFrame < targetSecondsPerFrame)
							{
								secondsElapsedForFrame = Win32GetSecondsElapsed(lastCounter, Win32GetWallClock());
							}
						}
						else
						{
							//TODO: Missed frame rate!!
							//TODO: Logging
						}

						LARGE_INTEGER endCounter = Win32GetWallClock();
						real32 msPerFrame = (real32)(1000.f * Win32GetSecondsElapsed(lastCounter, endCounter));
						lastCounter = endCounter;

						win32_window_dimension dimension = Win32GetWindowDimension(window);
	#if HANDMADE_INTERNAL
						//Win32DebugSyncDisplay(&g_BackBuffer, (int)ArrayCount(debugTimeMarkers), debugTimeMarkers, debugTimeMarkerIndex - 1, &soundOutput, targetSecondsPerFrame);
	#endif
						HDC deviceContext = GetDC(window);
						Win32DisplayBufferInWindow(&g_BackBuffer, deviceContext, dimension.width, dimension.height);
						ReleaseDC(window, deviceContext);

						flipWallClock = Win32GetWallClock();
	#if HANDMADE_INTERNAL
						{
							DWORD tempPlayCursor;
							DWORD tempWriteCursor;
							if (g_SecondaryBuffer->GetCurrentPosition(&tempPlayCursor, &tempWriteCursor) == DS_OK)
							{
								marker->flipPlayCursor = tempPlayCursor;
								marker->flipWriteCursor = tempWriteCursor;
							}
						}
	#endif

						game_input* temp = newInput;
						newInput = oldInput;
						oldInput = temp;

#if 0
						uint64 endCycleCount = __rdtsc();
						uint64 cyclesElapsed = endCycleCount - lastCycleCount;
						lastCycleCount = endCycleCount;

						real32 fps = 0.f;
						real32 mcpf = real32(cyclesElapsed / (1000.f * 1000.f));

						char fpsBuffer[256];
						sprintf_s(fpsBuffer, "%.02fms/f,  %.02ff/s,  %.02fmc/f\n", msPerFrame, fps, mcpf);
						OutputDebugStringA(fpsBuffer);
#endif

	#if HANDMADE_INTERNAL
						Assert(debugTimeMarkerIndex < ArrayCount(debugTimeMarkers));
						++debugTimeMarkerIndex;
						if (debugTimeMarkerIndex >= ArrayCount(debugTimeMarkers))
						{
							debugTimeMarkerIndex = 0;
						}
#endif
					}

				}
			}
			else
			{
				//TODO: logging
			}
		}
		else
		{
			//TODO: logging
		}
	}
	else
	{
		//TODO: Logging
	}

	return 0;
}