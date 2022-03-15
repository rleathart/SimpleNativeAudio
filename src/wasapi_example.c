#define COBJMACROS

#include <Windows.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <avrt.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <assert.h>

#pragma comment(lib, "ole32")
#pragma comment(lib, "avrt")

typedef struct
{
  IAudioClient* OutputClient;
  IAudioClient* InputClient;
  IAudioRenderClient* AudioRenderClient;
  IAudioCaptureClient* AudioCaptureClient;
  WAVEFORMATEX* InputFormat;
  WAVEFORMATEX* OutputFormat;
} wasapi_data;

// NOTE(robin): We write data from the input device into this global buffer
float MicData[2048];
int MicIndex;

void AudioInputCallback(int FrameCount, wasapi_data* Data)
{
  BYTE* AudioBuffer;
  DWORD Flags;
  IAudioCaptureClient_GetBuffer(Data->AudioCaptureClient, &AudioBuffer, &FrameCount, &Flags, 0, 0);

  int SampleRate = Data->InputFormat->nSamplesPerSec;
  int BytesPerSample = Data->InputFormat->wBitsPerSample / 8;
  int ChannelCount = Data->InputFormat->nChannels;

  MicIndex = 0;
  int ChannelSelect = 0; // NOTE(robin): Which input to write into the global buffer

  for (int FrameIndex = 0; FrameIndex < FrameCount; FrameIndex++)
  {
    for (int i = 0; i < ChannelCount; i++)
    {
      int SampleIndex = FrameIndex * ChannelCount + i;
      float Sample = 0;

      // NOTE(robin): Convert the hardware format to 32 bit float
      switch (BytesPerSample)
      {
        case 2: // NOTE(robin): 16 bit
        {
          short* InputBuffer = (short*)AudioBuffer;
          short InputSample = InputBuffer[SampleIndex];
          Sample = InputSample / (float)0x7FFF;
        } break;

        case 3: // NOTE(robin): 24 bit
        {
          char* InputBuffer = AudioBuffer + 3 * SampleIndex;
          int IntSample = 0;
          char* SampleBytes = (char*)&IntSample;

          SampleBytes[0] = InputBuffer[0];
          SampleBytes[1] = InputBuffer[1];
          SampleBytes[2] = InputBuffer[2];

          Sample = IntSample / (float)0x7FFFFF;
        } break;

        case 4: // NOTE(robin): 32 bit
        {
          int* InputBuffer = (int*)AudioBuffer;
          short InputSample = InputBuffer[SampleIndex];
          Sample = InputSample / (float)0x7FFFFFFF;
        } break;

        default:
        {
          assert(!"Unsupported sample format!");
        }
      }

      if (i == ChannelSelect)
        MicData[MicIndex++] = Sample;
    }
  }

  IAudioCaptureClient_ReleaseBuffer(Data->AudioCaptureClient, FrameCount);
}

void AudioOutputCallback(int FrameCount, wasapi_data* Data)
{
  BYTE* AudioBuffer;
  IAudioRenderClient_GetBuffer(Data->AudioRenderClient, FrameCount, &AudioBuffer);

  int SampleRate = Data->OutputFormat->nSamplesPerSec;
  int BytesPerSample = Data->OutputFormat->wBitsPerSample / 8;

  static float Phase[] = {0, 0};
  float PhaseDelta[] =
  {
    220.0f/SampleRate, // NOTE(robin): Frequency / SampleRate
    330.0f/SampleRate,
  };

  for (int FrameIndex = 0; FrameIndex < FrameCount; FrameIndex++)
  {
    for (int i = 0; i < 2; i++)
    {
      Phase[i] += PhaseDelta[i];
      if (Phase[i] >= 1)
        Phase[i] -= 1;
    }

    float Samples[] =
    {
      sin(2 * 3.1415926538 * Phase[0]),
      sin(2 * 3.1415926538 * Phase[1]),
    };

    float Volume = 0.1f;

    for (int i = 0; i < 2; i++)
    {
      int SampleIndex = FrameIndex * 2 + i;

      // NOTE(robin): Uncomment to write some input to the output
      // Samples[0] = MicData[FrameIndex];

      // NOTE(robin): Convert our 32 bit float samples to the hardware format
      switch (BytesPerSample)
      {
        case 2: // NOTE(robin): 16 bit
        {
          short Sample = Volume * Samples[i] * 0x7FFF;
          short* OutputBuffer = (short*)AudioBuffer;
          OutputBuffer[SampleIndex] = Sample;
        } break;

        case 3: // NOTE(robin): 24 bit
        {
          int Sample = Volume * Samples[i] * 0x7FFFFF;
          char* SampleBytes = (char*)&Sample;

          char* OutputBuffer = AudioBuffer + 3 * SampleIndex;
          OutputBuffer[0] = SampleBytes[0];
          OutputBuffer[1] = SampleBytes[1];
          OutputBuffer[2] = SampleBytes[2];
        } break;

        case 4: // NOTE(robin): 32 bit
        {
          int Sample = Volume * Samples[i] * 0x7FFFFFFF;
          int* OutputBuffer = (int*)AudioBuffer;
          OutputBuffer[SampleIndex] = Sample;
        } break;

        default:
        {
          assert(!"Unsupported sample format!");
        }
      }
    }

  }

  IAudioRenderClient_ReleaseBuffer(Data->AudioRenderClient, FrameCount, 0);
}

int main(int argc, char** argv)
{
  // NOTE(robin): The Windows SDK forgot to include these????
  CLSID IID_IMMDeviceEnumerator =
  {
    0xA95664D2, 0x9614, 0x4F35, {0xA7, 0x46, 0xDE, 0x8D, 0xB6, 0x36, 0x17, 0xE6},
  };
  CLSID IID_MMDeviceEnumerator =
  {
    0xBCDE0395, 0xE52F, 0x467C, {0x8E, 0x3D, 0xC4, 0x57, 0x92, 0x91, 0x69, 0x2E},
  };
  CLSID IID_IAudioClient =
  {
    0x1CB9AD4C, 0xDBFA, 0x4c32, {0xB1, 0x78, 0xC2, 0xF5, 0x68, 0xA7, 0x03, 0xB2},
  };

  CLSID IID_IAudioRenderClient =
  {
    0xF294ACFC, 0x3146, 0x4483, {0xA7, 0xBF, 0xAD, 0xDC, 0xA7, 0xC2, 0x60, 0xE2},
  };

  CLSID IID_IAudioCaptureClient =
  {
    0xC8ADBD64, 0xE71E, 0x48a0, {0xA4, 0xDE, 0x18, 0x5C, 0x39, 0x5C, 0xD3, 0x17},
  };

  PROPERTYKEY PKEY_AudioEngine_DeviceFormat =
  {
    0xf19f064d, 0x82c, 0x4e27, 0xbc, 0x73, 0x68, 0x82, 0xa1, 0xbb, 0x8e, 0x4c, 0,
  };

  HRESULT Error = 0;

  CoInitialize(0);

  IMMDeviceEnumerator* DeviceEnumerator;
  CoCreateInstance(&IID_MMDeviceEnumerator, 0, CLSCTX_ALL, &IID_IMMDeviceEnumerator, &DeviceEnumerator);

  IMMDevice* DefaultOutputDevice;
  IMMDeviceEnumerator_GetDefaultAudioEndpoint(DeviceEnumerator, eRender, eConsole, &DefaultOutputDevice);

  IMMDevice* DefaultInputDevice;
  IMMDeviceEnumerator_GetDefaultAudioEndpoint(DeviceEnumerator, eCapture, eConsole, &DefaultInputDevice);

  IAudioClient* OutputClient;
  IMMDevice_Activate(DefaultOutputDevice, &IID_IAudioClient, CLSCTX_ALL, 0, &OutputClient);

  IAudioClient* InputClient;
  IMMDevice_Activate(DefaultInputDevice, &IID_IAudioClient, CLSCTX_ALL, 0, &InputClient);

  // NOTE(robin): This is absolutely crazy, we can't just have a simple call to the
  // device to give us the hardware format, we have to query the property store and
  // then check that the format we get back is actually supported by the hardware.
  // Madness...

  IPropertyStore* Store = 0;
  IMMDevice_OpenPropertyStore(DefaultOutputDevice, STGM_READ, &Store);

  PROPVARIANT Prop;
  IPropertyStore_GetValue(Store, &PKEY_AudioEngine_DeviceFormat, &Prop);

  WAVEFORMATEX* OutputSampleFormat = (WAVEFORMATEX*)Prop.blob.pBlobData;
  Error = IAudioClient_IsFormatSupported(OutputClient,
      AUDCLNT_SHAREMODE_EXCLUSIVE, OutputSampleFormat, 0);

  if (Error)
  {
    printf("Failed to find a supported output sample format. Error: 0x%x\n", Error);
    return 1;
  }

  IMMDevice_OpenPropertyStore(DefaultInputDevice, STGM_READ, &Store);
  IPropertyStore_GetValue(Store, &PKEY_AudioEngine_DeviceFormat, &Prop);

  WAVEFORMATEX* InputSampleFormat = (WAVEFORMATEX*)Prop.blob.pBlobData;
  Error = IAudioClient_IsFormatSupported(InputClient,
      AUDCLNT_SHAREMODE_EXCLUSIVE, InputSampleFormat, 0);

  if (Error)
  {
    printf("Failed to find a supported input sample format. Error: 0x%x\n", Error);
    return 1;
  }

  int BufferSize = 512; // NOTE(robin): In samples

  // NOTE(robin): WASAPI expects buffer sizes to be specified in terms of a duration
  // as a multiple of 100ns intervals.
  double HundredNanosecondsPerSample = 10000000.0 / OutputSampleFormat->nSamplesPerSec;
  long long BufferDuration = BufferSize * HundredNanosecondsPerSample;

  // NOTE(robin): This call might actually fail so we check the error
  Error = IAudioClient_Initialize(OutputClient, AUDCLNT_SHAREMODE_EXCLUSIVE, AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
      BufferDuration, BufferDuration, OutputSampleFormat, 0);

  if (Error)
  {
    printf("Failed to initialise output audio client. Error: 0x%x\n", Error);
    return 1;
  }

  Error = IAudioClient_Initialize(InputClient, AUDCLNT_SHAREMODE_EXCLUSIVE, AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
      BufferDuration, BufferDuration, InputSampleFormat, 0);

  if (Error)
  {
    printf("Failed to initialise input audio client. Error: 0x%x\n", Error);
    return 1;
  }

  IAudioClient_GetBufferSize(OutputClient, (LPDWORD)&BufferSize);
  printf("Buffer size: %d\n", BufferSize);
  printf("Sample rate: %d\n", OutputSampleFormat->nSamplesPerSec);

  IAudioRenderClient* AudioRenderClient;
  IAudioClient_GetService(OutputClient, &IID_IAudioRenderClient, &AudioRenderClient);

  IAudioCaptureClient* AudioCaptureClient;
  IAudioClient_GetService(InputClient, &IID_IAudioCaptureClient, &AudioCaptureClient);

  HANDLE AudioCallbackEvent = CreateEvent(0, 0, 0, 0);
  HANDLE AudioInputCallbackEvent = CreateEvent(0, 0, 0, 0);
  IAudioClient_SetEventHandle(OutputClient, AudioCallbackEvent);
  IAudioClient_SetEventHandle(InputClient, AudioInputCallbackEvent);

  wasapi_data WASAPIData = {0};
  WASAPIData.OutputClient = OutputClient;
  WASAPIData.InputClient = InputClient;
  WASAPIData.AudioRenderClient = AudioRenderClient;
  WASAPIData.AudioCaptureClient = AudioCaptureClient;
  WASAPIData.OutputFormat = OutputSampleFormat;
  WASAPIData.InputFormat = InputSampleFormat;

  IAudioClient_Start(OutputClient);
  IAudioClient_Start(InputClient);

  // NOTE(robin): Tell the OS scheduler that we're doing pro-audio stuff in this thread
  // in a hope to have fewer buffer underflows.
  DWORD TaskIndex = 0;
  AvSetMmThreadCharacteristicsA("Pro Audio", &TaskIndex);

  // NOTE(robin): You would have this in another high priority thread
  for (;;)
  {
    WaitForSingleObject(AudioCallbackEvent, INFINITE);
    AudioOutputCallback(BufferSize, &WASAPIData);
    WaitForSingleObject(AudioInputCallbackEvent, INFINITE);
    AudioInputCallback(BufferSize, &WASAPIData);
  }

  IAudioClient_Stop(OutputClient);
  IAudioClient_Stop(InputClient);

  return 0;
}
