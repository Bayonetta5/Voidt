#include <stdint.h> // defines precise compiler-independent sizes of primitive types
#include <math.h>

#define internal        static
#define local_persist   static
#define global_variable static

#define Pi32 3.141592665359f

typedef int8_t  int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;
typedef int32   bool32;

typedef uint8_t  uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef float  real32;
typedef double real64;

// NOTE(Joey): utilize a unity build approach 
// (I personally dislike the approach and support a more modular
//  linkage build generating an array of link object files, with
//  the notable requirement of each .cpp and .h file being 
//  largely self-contained s.t. the build configuration only 
//  renders small number of files which keeps things as fast and
//  probably faster in the end, while keeping the code modular)
//  - I still take the approach seeing as it is an interesting
//    educational oppertunity regarding build/compiler configs.
#include "voidt.cpp"

#include <windows.h>
#include <xinput.h> // xbox 360 controller
#include <dsound.h> // DirectSound
#include <stdio.h> // c standard runtime/library

#include "win32_voidt.h"


inline uint32 SafeTruncateUInt64(uint64 value)
{
    Assert(value < 0xFFFFFFFF); // make sure size is less than 32 bits 4GB (or we have to loop for larger files)
    uint32 result = (uint32)value;
    return result;
}

internal debug_read_file_result DEBUGPlatformReadEntireFile(char *fileName)
{
    debug_read_file_result result = {};
    
    HANDLE fileHandle = CreateFileA(fileName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if(fileHandle != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER fileSize;
        if(GetFileSizeEx(fileHandle, &fileSize))
        {
            // TODO(Joey): defines for maximum values
            uint32 fileSize32 = SafeTruncateUInt64(fileSize.QuadPart);
            result.Contents = VirtualAlloc(0, fileSize32, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            if(result.Contents)
            {
                DWORD bytesRead;
                if(ReadFile(fileHandle, result.Contents, fileSize32, &bytesRead, 0) && fileSize32 == bytesRead)
                {
                    result.ContentSize = fileSize32;
                }
                else
                {
                    DEBUGPlatformFreeFileMemory(result.Contents);
                    result.ContentSize = 0;
                }
            }
        }        
        CloseHandle(fileHandle);
    }
    return result;
}

internal void DEBUGPlatformFreeFileMemory(void *memory)
{
    if(memory)
    {
        VirtualFree(memory, 0, MEM_RELEASE);
        memory = 0;
    }
}

internal bool32 DEBUGPlatformWriteEntireFile(char * fileName, uint32 memorySize, void *memory)
{
    bool32 result = false;
    
    HANDLE fileHandle = CreateFileA(fileName, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    if(fileHandle != INVALID_HANDLE_VALUE)
    {            
        DWORD bytesWritten;
        if(WriteFile(fileHandle, memory, memorySize, &bytesWritten, 0))
        {
            result = (bytesWritten == memorySize);
        }
        else
        {
            // TODO(Joey): diagnostics
        }

        CloseHandle(fileHandle);
    }
    return result;
}

// tries to find XInput function definition from DLL if present
internal void Win32LoadXInput()
{
    HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll");
    if(!XInputLibrary)
    {
        // TODO(Joey): Diagnostic
        XInputLibrary = LoadLibraryA("xinput1_3.dll");
    }
    if(XInputLibrary)
    {
        XInputGetState = (x_input_get_state*)GetProcAddress(XInputLibrary, "XInputGetState");
        XInputSetState = (x_input_set_state*)GetProcAddress(XInputLibrary, "XInputSetState");
        
        // TODO(Joey): Diagnostic
    }
    else
    {
        // TODO(Joey): Diagnostic
    }
}

internal void Win32InitDSound(HWND window, int32 samplesPerSecond, int32 bufferSize)
{
    // load the library
    HMODULE dSoundLibrary = LoadLibrary("dsound.dll");
    if(dSoundLibrary)
    {
         // get a DirectSound object
        direct_sound_create *directSoundCreate = (direct_sound_create*)GetProcAddress(dSoundLibrary, "DirectSoundCreate");
        LPDIRECTSOUND directSound;
        if(directSoundCreate && SUCCEEDED(directSoundCreate(0, &directSound, 0)))
        {
            WAVEFORMATEX waveFormat    = {};
            waveFormat.wFormatTag      = WAVE_FORMAT_PCM;
            waveFormat.nChannels       = 2;
            waveFormat.nSamplesPerSec  = samplesPerSecond;
            waveFormat.wBitsPerSample  = 16;
            waveFormat.nBlockAlign     = (waveFormat.nChannels * waveFormat.wBitsPerSample) / 8;
            waveFormat.nAvgBytesPerSec = waveFormat.nSamplesPerSec * waveFormat.nBlockAlign;
            waveFormat.cbSize          = 0;         
        
            // we can call other functions of the loaded directSound object using the (manual) DirectSoundCreate 
            // function pointer instead of loading these functions as well, because the other functions are
            // defined as virutal functions and thus have their memory address stored in the vtable; in which
            // case we know their addresses from the directSound object itself; this is why it works.
            if(!SUCCEEDED(directSound->SetCooperativeLevel(window, DSSCL_PRIORITY)))
            {
                // TODO(Joey): diagnostic
                return;
            }
            // "create" a primary (ring) buffer
            DSBUFFERDESC bufferDescription = { };
            bufferDescription.dwSize  = sizeof(bufferDescription);
            bufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;
            
            LPDIRECTSOUNDBUFFER primaryBuffer;
            if(SUCCEEDED(directSound->CreateSoundBuffer(&bufferDescription, &primaryBuffer, 0)))
            {   
                
                if(!SUCCEEDED(primaryBuffer->SetFormat(&waveFormat)))
                {
                    // TODO(Joey): diagnostics
                    return;
                }
            }        
            else
            {
                // TODO(Joey): diagnostics
                return;
            }
            
            // "create" a secondary buffer

            bufferDescription = {};
            bufferDescription.dwSize  = sizeof(bufferDescription);
            bufferDescription.dwBufferBytes = bufferSize;
            bufferDescription.lpwfxFormat = &waveFormat;
            
            if(SUCCEEDED(directSound->CreateSoundBuffer(&bufferDescription, &GlobalSecondaryBuffer, 0)))
            {      
                if(!SUCCEEDED(GlobalSecondaryBuffer->SetFormat(&waveFormat)))
                {
                    // TODO(Joey): diagnostics
                    return;
                }
            }               
        }
        else
        {
            // TODO(Joey): diagnostic
            return;
        }
    }
}

internal win32_window_dimension Win32GetWindowDimension(HWND Window)
{
    win32_window_dimension result;
    
    RECT clientRect;
    GetClientRect(Window, &clientRect);
    result.Width = clientRect.right - clientRect.left;
    result.Height = clientRect.bottom - clientRect.top;
    
    return result;
}

internal void Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int width, int height)
{
    // free previous memory if we're going to resize
    if(Buffer->Memory)
    {
        VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
    }
    
    Buffer->Width  = width;
    Buffer->Height = height;
    Buffer->BytesPerPixel = 4;
    
	Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
	Buffer->Info.bmiHeader.biWidth = Buffer->Width;
	Buffer->Info.bmiHeader.biHeight = -Buffer->Height; // negative biHeight sets bitmap origin to top-left corner
	Buffer->Info.bmiHeader.biPlanes = 1;
	Buffer->Info.bmiHeader.biBitCount = 32;
	Buffer->Info.bmiHeader.biCompression = BI_RGB;

	// creates a new bitmap to render to (allocate memory ourselves)
    int bitmapMemorySize = (width * height) * Buffer->BytesPerPixel;
    Buffer->Memory = VirtualAlloc(0, bitmapMemorySize, MEM_COMMIT, PAGE_READWRITE); // reserves/allocates complete pages
    
    Buffer->Pitch = Buffer->Width * Buffer->BytesPerPixel;
}

internal void Win32DisplayBufferInWindow(HDC device, win32_offscreen_buffer *Buffer, uint16 WindowWidth, uint16 WindowHeight)
{    
	// copies from one rectangle to the other, possibly stretching
	StretchDIBits(device, 
        0, 0, WindowWidth, WindowHeight,
        0, 0, Buffer->Width, Buffer->Height,
		Buffer->Memory, 
		&Buffer->Info,
        DIB_RGB_COLORS, 
        SRCCOPY
    );
            
    
}

LRESULT Win32MainWindowCallBack(
	HWND Window,
	UINT msg,
	WPARAM WParam,
	LPARAM LParam)
{
	LRESULT result = 0;
	
	switch(msg)
	{
		case WM_SIZE:
		{
            // win32_window_dimension Dimension = Win32GetWindowDimension(Window);
			// Win32ResizeDIBSection(&GlobalBackBuffer, Dimension.Width, Dimension.Height);
		} break;
		case WM_CLOSE:
		{
			GlobalRunning = false;
		} break;
		case WM_ACTIVATEAPP:
		{
			OutputDebugStringA("WM_ACTIVATEAPP\n");
		} break;
		case WM_DESTROY:
		{
			GlobalRunning = false;
		} break;
        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            uint32 VKCode = WParam;
            bool WasDown = (LParam & (1 << 30)) != 0;
            bool IsDown = (LParam & (1 << 31)) == 0;
            
            if(WasDown != IsDown)
            {
                if(VKCode == 'W')
                {
                    OutputDebugStringA("W\n");
                }
                else if (VKCode == 'A')
                {
                    
                }
                else if (VKCode == 'S')
                {
                    
                }
                else if (VKCode == 'D')
                {
                    
                }
                else if (VKCode == 'Q')
                {
                    
                }
                else if (VKCode == 'E')
                {
                    
                }
                else if (VKCode == VK_UP)
                {
                    
                }
                else if (VKCode == VK_LEFT)
                {
                    
                }
                else if (VKCode == VK_DOWN)
                {
                    
                }
                else if (VKCode == VK_RIGHT)
                {
                    
                }
                else if (VKCode == VK_ESCAPE)
                {
                    if(IsDown)
                    {
                        GlobalRunning = false;
                    }
                }
                else if (VKCode == VK_SPACE)
                {
                    
                }             
            }           
            bool32 AltKeyWasDown = (LParam & (1 << 29));
            if((VKCode == VK_F4) && AltKeyWasDown)
            {
                GlobalRunning = false;
            }
            
        } break;        
		case WM_PAINT:
		{
			PAINTSTRUCT Paint;
			HDC DeviceContext = BeginPaint(Window, &Paint);
			int x = Paint.rcPaint.left;
			int y = Paint.rcPaint.top;
			int height = Paint.rcPaint.bottom - Paint.rcPaint.top;
			int width = Paint.rcPaint.right - Paint.rcPaint.left;
            
            win32_window_dimension Dimension = Win32GetWindowDimension(Window);
            
			Win32DisplayBufferInWindow(DeviceContext, &GlobalBackBuffer, Dimension.Width, Dimension.Height);
			EndPaint(Window, &Paint);
		} break;
		default:
		{
			result = DefWindowProc(Window, msg, WParam, LParam);
		} break;	
	}		
	
	return result;
}


internal void Win32ClearBuffer(win32_sound_output *soundOutput)
{
    VOID *region1;
    DWORD region1Size;
    VOID *region2;
    DWORD region2Size;     
    if(SUCCEEDED(GlobalSecondaryBuffer->Lock(0, soundOutput->secondaryBufferSize,
                                             &region1, &region1Size,
                                             &region2, &region2Size,
                                             0)))
    {
        uint8 *destSample = (uint8 *)region1;
        for(DWORD byteIndex = 0; byteIndex < region1Size; ++byteIndex)
           *destSample++ = 0;
        destSample = (uint8 *)region2;
        for(DWORD byteIndex = 0; byteIndex < region2Size; ++byteIndex)
           *destSample++ = 0;
       
       GlobalSecondaryBuffer->Unlock(region1, region1Size, region2, region2Size);        
    }
}

internal void Win32FillSoundBuffer(win32_sound_output *soundOutput, DWORD byteToLock, DWORD bytesToWrite, game_sound_output_buffer *sourceBuffer)
{
    VOID *region1;
    DWORD region1Size;
    VOID *region2;
    DWORD region2Size;                                        
    if(SUCCEEDED(GlobalSecondaryBuffer->Lock(byteToLock, bytesToWrite,
                                             &region1, &region1Size,
                                             &region2, &region2Size,
                                             0)))
    {
        // first buffer region 
        DWORD region1SampleCount = region1Size / soundOutput->bytesPerSample;
        int16 *destSample = (int16 *)region1;
        int16 *sourceSample = sourceBuffer->Samples;
        for(DWORD sampleIndex = 0; sampleIndex < region1SampleCount; ++sampleIndex)
        {
            *destSample++ = *sourceSample++;
            *destSample++ = *sourceSample++;
            ++soundOutput->runningSampleIndex;
        }
        // second buffer region (if we're over the end of the ring buffer)
        DWORD region2SampleCount = region2Size / soundOutput->bytesPerSample;
        destSample = (int16 *)region2;
        for(DWORD sampleIndex = 0; sampleIndex < region2SampleCount; ++sampleIndex)
        {
            *destSample++ = *sourceSample++;
            *destSample++ = *sourceSample++;
             ++soundOutput->runningSampleIndex;
        }         

        GlobalSecondaryBuffer->Unlock(region1, region1Size, region2, region2Size);        
    }
    else
    {
        // NOTE(JOEY): diagnostics
    }
}

internal void Win32ProcessXInputDigitalButton(DWORD xInputButtonState, game_button_state *oldState, DWORD buttonBit, game_button_state *newState)
{
    newState->EndedDown = ((xInputButtonState & buttonBit) == buttonBit);
    newState->HalfTransitionCount = (oldState->EndedDown != newState->EndedDown) ? 1 : 0;
}

int CALLBACK WinMain(
	HINSTANCE instance,
	HINSTANCE prevInstance,
	LPSTR cmdLine,
	int cmdShow)
{
	// MessageBox(0, "Sup G", "Joey", MB_OK | MB_ICONINFORMATION);	
    Win32LoadXInput();
    
	WNDCLASS WindowClass = {};
    
    Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);
	
	WindowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	WindowClass.lpfnWndProc = Win32MainWindowCallBack;
	WindowClass.hInstance = instance;
	WindowClass.lpszClassName = "JoeyWindowClass";
		
	LARGE_INTEGER perfCountFrequency;
    QueryPerformanceFrequency(&perfCountFrequency);
    
    uint64 lastCycleCount = __rdtsc();    
        
	if(RegisterClass(&WindowClass))
	{
		HWND Window = CreateWindowEx(
			0,
			WindowClass.lpszClassName,
			"Joey game",
			WS_OVERLAPPEDWINDOW | WS_VISIBLE,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			0,
			0,
			instance,
			0
		);
		
		if(Window)
		{			
            HDC DeviceContext = GetDC(Window);
    
            // graphics testbed variables
            int XOffset = 0;
            int YOffset = 0;
            
            // sound testbed variables           
            win32_sound_output soundOutput = {};
                
            soundOutput.samplesPerSecond    = 48000;
            soundOutput.runningSampleIndex  = 0;
            soundOutput.bytesPerSample      = sizeof(int16) * 2;
            soundOutput.secondaryBufferSize = soundOutput.samplesPerSecond * soundOutput.bytesPerSample;
            soundOutput.latencySampleCount  = soundOutput.samplesPerSecond / 15;

            Win32InitDSound(Window, soundOutput.samplesPerSecond, soundOutput.secondaryBufferSize); 
            Win32ClearBuffer(&soundOutput);
            GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);
           
            int16 *samples = (int16 *)VirtualAlloc(0, soundOutput.secondaryBufferSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            
            #if INTERNAL
                LPVOID baseAddress = (LPVOID)Gigabytes(2048);
            #else
                LPVOID baseAddress = 0;
            #endif
            game_memory gameMemory = {};
            gameMemory.PermanentStorageSize = Megabytes(64);
            gameMemory.TransientStorageSize = Gigabytes(4);
            uint64 totalSize                = gameMemory.PermanentStorageSize + gameMemory.TransientStorageSize;
            gameMemory.PermanentStorage     = VirtualAlloc(baseAddress, totalSize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            gameMemory.TransientStorage     = (uint8 *)gameMemory.PermanentStorage + gameMemory.PermanentStorageSize;
            
            if(samples && gameMemory.PermanentStorage && gameMemory.TransientStorage)
            {
            
                game_input input[2] = {};
                game_input *newInput = &input[0];
                game_input *oldInput = &input[1];
                
                
                GlobalRunning = true;
                
                LARGE_INTEGER lastCounter;
                QueryPerformanceCounter(&lastCounter);
                while(GlobalRunning)
                {                
                    MSG Message;
                    // process all windows messages currently in Queue               
                    while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
                    {
                        if(Message.message == WM_QUIT)
                            GlobalRunning = false;
                        
                        TranslateMessage(&Message);
                        DispatchMessage(&Message);
                    }
                    
                    // poll xbox 360 controller(s)
                    int maxControllerCount = XUSER_MAX_COUNT;
                    if(maxControllerCount > ArrayCount(newInput->Controllers))
                    {
                        maxControllerCount = ArrayCount(newInput->Controllers);
                    }
                    for(DWORD i = 0; i < maxControllerCount; ++i)
                    {
                        game_controller_input *oldController = &oldInput->Controllers[i];
                        game_controller_input *newController = &newInput->Controllers[i];
                        
                        XINPUT_STATE ControllerState;
                        if(XInputGetState(i, &ControllerState) == ERROR_SUCCESS)
                        {
                            // the controller is plugged in
                            XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;
                                                    
                            
                            // TODO(Joey): DPad
                            // bool Up            = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
                            // bool Down          = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
                            // bool Left          = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
                            // bool Right         = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);
                            newController->IsAnalog = true;
                            real32 stickX;
                            real32 stickY;
                            if(Pad->sThumbLX < 0)
                                stickX = (real32)Pad->sThumbLX / 32768.0f;
                            else
                                stickX = (real32)Pad->sThumbLX / 32767.0f;
                            if(Pad->sThumbLY < 0)
                                stickY = (real32)Pad->sThumbLY / 32768.0f;
                            else
                                stickY = (real32)Pad->sThumbLY / 32767.0f;      

                            newController->StartX = oldController->EndX;
                            newController->StartY = oldController->EndY;
                            
                            newController->MinX = newController->MaxX = newController->EndX = stickX;
                            newController->MinY = newController->MaxY = newController->EndY = stickY;
                            
                            
                            // Win32ProcessXInputDigitalButton(Pad->wButtons, oldController->Down, XINPUT_GAMEPAD_START, newController->Down);
                            // Win32ProcessXInputDigitalButton(Pad->wButtons, oldController->Down, XINPUT_GAMEPAD_BACK, newController->Down);
                            Win32ProcessXInputDigitalButton(Pad->wButtons, &oldController->LeftShoulder, XINPUT_GAMEPAD_LEFT_SHOULDER, &newController->LeftShoulder);
                            Win32ProcessXInputDigitalButton(Pad->wButtons, &oldController->RightShoulder, XINPUT_GAMEPAD_RIGHT_SHOULDER, &newController->RightShoulder);
                            Win32ProcessXInputDigitalButton(Pad->wButtons, &oldController->Down, XINPUT_GAMEPAD_A, &newController->Down);
                            Win32ProcessXInputDigitalButton(Pad->wButtons, &oldController->Right, XINPUT_GAMEPAD_B, &newController->Right);
                            Win32ProcessXInputDigitalButton(Pad->wButtons, &oldController->Up, XINPUT_GAMEPAD_Y, &newController->Up);
                            Win32ProcessXInputDigitalButton(Pad->wButtons, &oldController->Left, XINPUT_GAMEPAD_Y, &newController->Left);                        
                        }
                        else
                        {
                            // the controller is not available
                        }
                    }

                    // render
                    // RenderWeirdGradiant(&GlobalBackBuffer, XOffset++, YOffset);         
                    DWORD byteToLock = 0;
                    DWORD targetCursor = 0;
                    DWORD bytesToWrite = 0;
                    DWORD playCursor = 0;
                    DWORD writeCursor = 0;
                    bool32 soundIsValid = false;
                    if(SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&playCursor, &writeCursor)))
                    {
                        byteToLock = (soundOutput.runningSampleIndex * soundOutput.bytesPerSample) % soundOutput.secondaryBufferSize;
                        
                        targetCursor = (playCursor + soundOutput.latencySampleCount * soundOutput.bytesPerSample) % soundOutput.secondaryBufferSize;
                        if(byteToLock > targetCursor)
                        {
                            bytesToWrite = (soundOutput.secondaryBufferSize - byteToLock); // from byteToLock to end
                            bytesToWrite += targetCursor;                                    // from 0 to position at PlayCursor
                        }
                        else
                        {
                            bytesToWrite = targetCursor - byteToLock;
                        }
                        soundIsValid = true;
                    }
                              
                    game_sound_output_buffer soundBuffer = {};
                    soundBuffer.SamplesPerSecond = soundOutput.samplesPerSecond;
                    soundBuffer.SampleCount = bytesToWrite / soundOutput.bytesPerSample;
                    soundBuffer.Samples = samples;
                    
                    
                    game_offscreen_buffer buffer = {};
                    buffer.Memory = GlobalBackBuffer.Memory;
                    buffer.Width = GlobalBackBuffer.Width;
                    buffer.Height = GlobalBackBuffer.Height;
                    buffer.Pitch = GlobalBackBuffer.Pitch;
                    
                    GameUpdateAndRender(&gameMemory, newInput, &buffer, &soundBuffer);
                                    
                    // audio
                    if(soundIsValid)
                    {                   
                        Win32FillSoundBuffer(&soundOutput, byteToLock, bytesToWrite, &soundBuffer);
                    }
                    else
                    {
                        // NOTE(JOEY): diagnostics
                    }          
                                    
                                    
                                    
                                    
                                    
                    win32_window_dimension Dimension = Win32GetWindowDimension(Window);                
                    Win32DisplayBufferInWindow(DeviceContext, &GlobalBackBuffer, Dimension.Width, Dimension.Height);           

                    
                    uint64 endCycleCount = __rdtsc();
                    
                    LARGE_INTEGER endCounter;
                    QueryPerformanceCounter(&endCounter);
                    
                    // display values
                    uint64 cyclesElapsed  = endCycleCount - lastCycleCount;
                    uint64 counterElapsed = endCounter.QuadPart - lastCounter.QuadPart;
                    real32 msPerFrame    = ((1000.0f * (real32)counterElapsed) / (real32)perfCountFrequency.QuadPart);
                    real32 fps           = (real32)perfCountFrequency.QuadPart / (real32)counterElapsed;
                    real32 mcpf          = (real32)(cyclesElapsed / (1000.0f * 1000.0f)); // mega-cycles per frame
                    
                    char charBuffer[256];
                    sprintf(charBuffer, "%.02fms/f / %.02ff/s  -  %.02fmc/f\n", msPerFrame, fps, mcpf); 
                    OutputDebugStringA(charBuffer);
                    
                    lastCounter = endCounter;
                    lastCycleCount = endCycleCount;
                    
                    game_input *temp = newInput;
                    newInput = oldInput;
                    oldInput = temp;
                }
            }
            else
            {
                // diagnostics: memory allocation failed
            }
		}
		else
		{
			OutputDebugStringA("WindowHandle creation FAILED\n");
		}
	}
	else
	{
		OutputDebugStringA("RegisterClass FAILED\n");
	}
	

	return (0);
}
