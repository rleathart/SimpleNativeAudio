#include <CoreAudio/CoreAudio.h>

// NOTE(robin): The CoreAudio API for querying device data is absolutely insane
// so we provide some wrapper functions here, you can mostly ignore the implementation.
// The idea is that we define a selector, e.g. kAudioDevicePropertyBufferFrameSize, which
// we then pass to AudioObjectGetPropertyData which will write the data of the selected
// property at the pointer we provide.

OSStatus CoreAudioGetBufferSize(AudioDeviceID Device, UInt32* BufferSize)
{
  AudioObjectPropertyAddress Property =
  {
    kAudioDevicePropertyBufferFrameSize, // NOTE(robin): This is the thing you care about

    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster,
  };

  UInt32 PropertySize = sizeof(*BufferSize);
  OSStatus Result = AudioObjectGetPropertyData(Device,
      &Property, 0, 0, &PropertySize, BufferSize);
  return Result;
}

OSStatus CoreAudioSetBufferSize(AudioDeviceID Device, UInt32 BufferSize)
{
  AudioObjectPropertyAddress Property =
  {
    kAudioDevicePropertyBufferFrameSize,

    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster,
  };

  UInt32 PropertySize = sizeof(BufferSize);
  OSStatus Result = AudioObjectSetPropertyData(Device,
      &Property, 0, 0, PropertySize, &BufferSize);
  return Result;
}

OSStatus CoreAudioGetSampleRate(AudioDeviceID Device, double* SampleRate)
{
  AudioObjectPropertyAddress Property =
  {
    kAudioDevicePropertyNominalSampleRate,

    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster,
  };

  UInt32 PropertySize = sizeof(*SampleRate);
  OSStatus Result = AudioObjectGetPropertyData(Device,
      &Property, 0, 0, &PropertySize, SampleRate);
  return Result;
}

// NOTE(robin): Scope is either kAudioObjectPropertyScopeInput (for an input device) or
// kAudioObjectPropertyScopeOutput (for an output device).
OSStatus CoreAudioGetSampleFormat(AudioDeviceID Device,
    AudioObjectPropertyScope Scope,
    AudioStreamBasicDescription* Format)
{
  AudioObjectPropertyAddress Property =
  {
    kAudioStreamPropertyVirtualFormat,

    Scope,
    kAudioObjectPropertyElementMaster,
  };

  UInt32 PropertySize = sizeof(*Format);
  OSStatus Result = AudioObjectGetPropertyData(Device,
      &Property, 0, 0, &PropertySize, Format);
  return Result;
}

OSStatus CoreAudioGetDefaultOutputDevice(AudioDeviceID* Device)
{
  AudioObjectPropertyAddress Property =
  {
    kAudioHardwarePropertyDefaultOutputDevice,

    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster,
  };

  UInt32 PropertySize = sizeof(*Device);
  OSStatus Result = AudioObjectGetPropertyData(kAudioObjectSystemObject,
      &Property, 0, 0, &PropertySize, Device);
  return Result;
}

OSStatus CoreAudioGetDefaultInputDevice(AudioDeviceID* Device)
{
  AudioObjectPropertyAddress Property =
  {
    kAudioHardwarePropertyDefaultInputDevice,

    kAudioObjectPropertyScopeGlobal,
    kAudioObjectPropertyElementMaster,
  };

  UInt32 PropertySize = sizeof(*Device);
  OSStatus Result = AudioObjectGetPropertyData(kAudioObjectSystemObject,
      &Property, 0, 0, &PropertySize, Device);
  return Result;
}

float MicBuffer[512];
int MicIndex;

// IMPORTANT(robin): You need to run this program from a process that has microphone
// permissions in order to access the microphone samples since this program will not
// generate a permissons popup. e.g. you can grant Terminal.app microphone permissions
// in system preferences.
OSStatus AudioInputCallback(AudioDeviceID Device,
                  const AudioTimeStamp*   Now,
                  const AudioBufferList*  InputData,
                  const AudioTimeStamp*   InputTime,
                  AudioBufferList*        OutputData,
                  const AudioTimeStamp*   OutputTime,
                  void*                   UserData)
{
  UInt32 FrameCount = 0;
  CoreAudioGetBufferSize(Device, &FrameCount);

  AudioStreamBasicDescription StreamFormat = {0};
  OSStatus Error = CoreAudioGetSampleFormat(Device, kAudioObjectPropertyScopeInput, &StreamFormat);

  assert(!Error && "Failed to get input device sample format");

  UInt32 StreamIsFloat = StreamFormat.mFormatFlags & kAudioFormatFlagIsFloat;
  UInt32 SampleRate = StreamFormat.mSampleRate;
  UInt32 BytesPerSample = StreamFormat.mBytesPerFrame / StreamFormat.mChannelsPerFrame;
  UInt32 ChannelCount = InputData->mBuffers[0].mNumberChannels;

  MicIndex = 0;
  int MicChannelIndex = 0;
  float* InputBuffer = (float*)InputData->mBuffers[0].mData;
  for (UInt32 FrameIndex = 0; FrameIndex < FrameCount; FrameIndex++)
  {
    for (UInt32 ChannelIndex = 0; ChannelIndex < ChannelCount; ChannelIndex++)
    {
      int SampleIndex = FrameIndex * ChannelCount + ChannelIndex;
      float Sample = InputBuffer[SampleIndex];

      if (ChannelIndex == MicChannelIndex)
        MicBuffer[MicIndex++] = Sample;
    }
  }

  return 0;
}

OSStatus AudioOutputCallback(AudioDeviceID Device,
                  const AudioTimeStamp*    Now,
                  const AudioBufferList*   InputData,
                  const AudioTimeStamp*    InputTime,
                  AudioBufferList*         OutputData,
                  const AudioTimeStamp*    OutputTime,
                  void*                    UserData)
{
  AudioStreamBasicDescription StreamFormat = {0};
  OSStatus Error = CoreAudioGetSampleFormat(Device, kAudioObjectPropertyScopeOutput, &StreamFormat);

  assert(!Error && "Failed to get output device sample format");

  UInt32 StreamIsFloat = StreamFormat.mFormatFlags & kAudioFormatFlagIsFloat;
  UInt32 SampleRate = StreamFormat.mSampleRate;
  UInt32 BytesPerSample = StreamFormat.mBytesPerFrame / StreamFormat.mChannelsPerFrame;
  UInt32 ChannelCount = OutputData->mBuffers[0].mNumberChannels;

  if (!StreamIsFloat || BytesPerSample != 4)
  {
    // TODO(robin): Does CoreAudio ever deal with anything that isn't 32 bit float?
    printf("\n*** Unsupported stream format! (%d bit %s) ***\n\n",
        BytesPerSample * 8, StreamIsFloat ? "float" : "int");
    assert(0);
  }

  float* OutputBuffer = (float*)OutputData->mBuffers[0].mData;

  static float Phase[2];
  float PhaseDelta[] =
  {
    220.0/SampleRate, // NOTE(robin): Frequency/SampleRate
    330.0/SampleRate,
  };

  UInt32 FrameCount = 0;
  CoreAudioGetBufferSize(Device, &FrameCount);

  for (UInt32 i = 0; i < FrameCount; i++)
  {
    for (UInt32 i = 0; i < sizeof(Phase)/sizeof(Phase[0]); i++)
    {
      Phase[i] += PhaseDelta[i];
      if (Phase[i] >= 1)
        Phase[i] -= 1;
    }

    // NOTE(robin): We use SampleIndex and the magical indices 0 and 1 here because
    // the samples are interleaved. i.e. Left Right Left Right
    UInt32 SampleIndex = StreamFormat.mChannelsPerFrame * i;

    float Volume = 0.2;
    OutputBuffer[SampleIndex + 0] = Volume * sin(Phase[0] * 2 * M_PI); // NOTE(robin): Left
    OutputBuffer[SampleIndex + 1] = Volume * sin(Phase[1] * 2 * M_PI); // NOTE(robin): Right

    // NOTE(robin): Uncomment to hear input instead
    // OutputBuffer[SampleIndex + 0] = MicBuffer[i];
    // OutputBuffer[SampleIndex + 1] = MicBuffer[i];
  }

  return 0;
}

int main(int argc, char* argv[])
{
  AudioDeviceID OutputDeviceID = 0;
  AudioDeviceID InputDeviceID = 0;

  // NOTE(robin): These are the devices you have selected as default in sound preferences
  CoreAudioGetDefaultOutputDevice(&OutputDeviceID);
  CoreAudioGetDefaultInputDevice(&InputDeviceID);

  UInt32 BufferSize = 0;
  CoreAudioGetBufferSize(OutputDeviceID, &BufferSize);
  printf("Default output buffer size: %d\n", BufferSize);

  // NOTE(robin): Now override the default buffer size to anything you want
  BufferSize = 128;
  CoreAudioSetBufferSize(OutputDeviceID, BufferSize);
  CoreAudioSetBufferSize(InputDeviceID, BufferSize);

  CoreAudioGetBufferSize(OutputDeviceID, &BufferSize);
  printf("Actual output buffer size was set to: %d\n", BufferSize);
  CoreAudioGetBufferSize(InputDeviceID, &BufferSize);
  printf("Actual input buffer size was set to: %d\n", BufferSize);

  // NOTE(robin): We have a separate callback for audio input and output since
  // CoreAudio sees the input/output as separate devices.
  AudioDeviceIOProcID OutputIOProcID = NULL;
  AudioDeviceIOProcID InputIOProcID = NULL;

  // NOTE(robin): Register the callbacks
  AudioDeviceCreateIOProcID(OutputDeviceID, AudioOutputCallback, 0, &OutputIOProcID);
  AudioDeviceCreateIOProcID(InputDeviceID, AudioInputCallback, 0, &InputIOProcID);

  // NOTE(robin): Tell the hardware to start calling our callbacks
  AudioDeviceStart(OutputDeviceID, OutputIOProcID);
  AudioDeviceStart(InputDeviceID, InputIOProcID);

  sleep(3);

  AudioDeviceStop(OutputDeviceID, OutputIOProcID);
  AudioDeviceStop(InputDeviceID, InputIOProcID);

  return 0;
}
