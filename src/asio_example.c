/* 
 * This file is a simple example of implementing the functionality provided by asio.c.
 * It will provide you with a command line prompt that prints your installed ASIO drivers
 * and will ask you to pick one. It will then play 3 seconds of audio to the first 2 outputs
 * on your device.
 */

#pragma warning(push, 0)
#include <Windows.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>
#pragma warning(pop)

// NOTE(robin): Fixed size typedefs required by asio.c
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef signed char s8;
typedef signed short s16;
typedef signed int s32;
typedef signed long long s64;
typedef float f32;
typedef double f64;
typedef u16 wchar;

#include "asio.c"

// NOTE(robin): Since ASIO doesn't support passing user data to the callback, we store the information we need
// in a global struct
typedef struct
{
  asio_driver* Driver;
  asio_buffer_info* Inputs;
  asio_buffer_info* Outputs;
  asio_channel_info* Channels;

  s32 InputChannels;
  s32 OutputChannels;
  s32 BufferSize;
  s32 SupportsOutputReady;
  f64 SampleRate;
} asio_device;
asio_device ASIODevice;

// NOTE(robin): This is the actual audio callback. The ASIO hardware will call this periodically
// and you should fill the hardware buffers before the next callback (otherwise you will have buffer underflow).
asio_time* ASIOAudioCallback(asio_time* Time, s32 BufferIndex, s32 DoDirectProcess)
{
  // NOTE(robin): Here we store the phase of 2 oscillators
  static f64 Phase[] = {0, 0};
  f64 PhaseDelta[] =
  {
    220.0/ASIODevice.SampleRate, // NOTE(robin): Frequency / SampleRate
    330.0/ASIODevice.SampleRate,
  };

  for (s32 FrameIndex = 0; FrameIndex < ASIODevice.BufferSize; FrameIndex++)
  {
    // NOTE(robin): Advance our sin oscillators
    for (u32 i = 0; i < 2; i++)
    {
      Phase[i] += PhaseDelta[i];
      if (Phase[i] >= 1)
        Phase[i] -= 1;
    }

    f64 Samples[] =
    {
      sin(2 * 3.1415926538 * Phase[0]),
      sin(2 * 3.1415926538 * Phase[1]),
    };

    // NOTE(robin): Just output to the first 2 outputs since this is probably what
    // the speakers/headphones are plugged into
    for (u32 ChannelIndex = 0; ChannelIndex < 2; ChannelIndex++)
    {
      void* OutputBuffer = ASIODevice.Outputs[ChannelIndex].Buffers[BufferIndex];
      void* InputBuffer = ASIODevice.Inputs[0].Buffers[BufferIndex]; // NOTE(robin): Get data from input 0 here

      f64 Volume = 0.1f; // NOTE(robin): Turn it down a bit
      f64 Sample = Volume * Samples[ChannelIndex]; // NOTE(robin): The floating point sample we want to write

      // NOTE(robin): InputChannels + ChannelIndex is because the output channels come directly after the inputs
      asio_sample_format HardwareFormat =
        ASIOGetSampleFormat(ASIODevice.Channels[ASIODevice.InputChannels + ChannelIndex].SampleType);

      asio_error Error = ASIOWriteSampleToHardwareBuffer(OutputBuffer, FrameIndex, Sample, HardwareFormat);

      if (Error != ASIOErrorOK)
        printf("Failed to convert to the hardware sample format\n");
    }
  }

  if (ASIODevice.SupportsOutputReady)
    ASIODevice.Driver->VMT->OutputReady(ASIODevice.Driver);

  return Time;
}

// NOTE(robin): Newer ASIO devices support a callback that passes timing information automatically.
// This function is merely a fallback for when that behavior is not supported by the hardware.
void ASIOAudioCallbackOldStyle(s32 Index, s32 DoDirectProcess)
{
  asio_time Time = {0};

  asio_error Result = ASIODevice.Driver->VMT->GetSamplePosition(ASIODevice.Driver,
      &Time.Info.SamplePosition, &Time.Info.SystemTime);

  if (Result == ASIOErrorOK)
  {
    Time.Info.Flags = (asio_time_info_flags)(ASIOTimeInfoSystemTimeValid | ASIOTimeInfoSamplePositionValid);
  }

  ASIOAudioCallback(&Time, Index, DoDirectProcess);
}

void ASIOChangeSampleRate(f64 SampleRate)
{
  // NOTE(robin): Code that is run when the sample rate changes goes here...
  ASIODevice.SampleRate = SampleRate;
}

// NOTE(robin): The hardware will send us messages through this callback
s32 ASIOMessage(asio_message_selector Selector, s32 Value, void* Message, f64* Opt)
{
  s32 Result = 0;
  switch (Selector)
  {
    // NOTE(robin): The hardware will send us this message if it supports the new style callback
    case ASIOSelectorSupportsTimeInfo:
    {
      Result = 1;
    } break;

    default:
    {
    }
  }
  return Result;
}

int main(void)
{
  u32 DriverCount = 0;
  ASIOGetDriverCount(&DriverCount);

  // NOTE(robin): Define the maximum length of the strings populated by ASIOGetDriverInfo
  u32 ClassIDMax = 128;
  u32 DriverNameMax = 128;
  u32 DllPathMax = 1024;

  // NOTE(robin): We allocate the memory for the strings ourselves because ASIOGetDriverInfo does
  // not do any dynamic memory allocation
  wchar** ClassIDStrings = malloc(DriverCount * sizeof(wchar*));
  wchar** DriverNames = malloc(DriverCount * sizeof(wchar*));
  wchar** DllPaths = malloc(DriverCount * sizeof(wchar*));
  for (u32 i = 0; i < DriverCount; i++)
  {
    ClassIDStrings[i] = malloc(ClassIDMax * sizeof(wchar));
    DriverNames[i] = malloc(DriverNameMax * sizeof(wchar));
    DllPaths[i] = malloc(DllPathMax * sizeof(wchar));
  }

  ASIOGetDriverInfo(DriverCount, ClassIDStrings, ClassIDMax, DriverNames, DriverNameMax, DllPaths, DllPathMax);

  printf("Found drivers:\n");
  for (u32 i = 0; i < DriverCount; i++)
    wprintf(L"%d: %s\n", i, DriverNames[i]);

  printf("\nPlease choose a driver to use: ");
  char Response = (char)getchar(); // NOTE(robin): Only supports up to 10 drivers
  int SelectedDriverIndex = atoi(&Response);

  assert(SelectedDriverIndex < (int)DriverCount);

  wprintf(L"You have selected: %s\n", DriverNames[SelectedDriverIndex]);

  asio_driver* ASIODriver = 0;
  ASIOLoadDriver(&ASIODriver, ClassIDStrings[SelectedDriverIndex], DllPaths[SelectedDriverIndex]);

  // NOTE(robin): Now do the actual ASIO initialisation...
  // NOTE(robin): This is mostly all boilerplate initialisation code for ASIO

  // NOTE(robin): This ASIODriver->VMT->MethodName(ASIODriver, ...) pattern is a result of
  // calling a C++ API from C. You can think of this as the equivalent of ASIODriver->Init(0) in C++.
  ASIODriver->VMT->Init(ASIODriver, 0);

  // NOTE(robin): Get how many hardware channels we have
  s32 InputChannels, OutputChannels;
  ASIODriver->VMT->GetChannels(ASIODriver, &InputChannels, &OutputChannels);
  s32 TotalChannels = InputChannels + OutputChannels;

  // NOTE(robin): Get the hardware buffer size
  s32 MinBufferSize, MaxBufferSize, PreferredBufferSize, BufferGranularity;
  ASIODriver->VMT->GetBufferSize(ASIODriver,
      &MinBufferSize, &MaxBufferSize, &PreferredBufferSize, &BufferGranularity);
  s32 BufferSize = PreferredBufferSize;

  // NOTE(robin): Get the hardware sample rate
  f64 SampleRate; // NOTE(robin): Driver may not store this and need to be set manually
  ASIODriver->VMT->GetSampleRate(ASIODriver, &SampleRate);

  // NOTE(robin): Does the hardware support the OutputReady optimisation?
  s32 SupportsOutputReady = ASIODriver->VMT->OutputReady(ASIODriver) == ASIOErrorOK;

  asio_buffer_info* BufferInfos = malloc(TotalChannels * sizeof(asio_buffer_info));
  asio_channel_info* ChannelInfos = malloc(TotalChannels * sizeof(asio_channel_info));

  // NOTE(robin): Fill out the asio_buffer_info structs to be read by CreateBuffers
  for (s32 i = 0; i < TotalChannels; i++)
  {
    s32 IsInput = i < InputChannels;
    BufferInfos[i].IsInput = IsInput;
    BufferInfos[i].ChannelIndex = IsInput ? i : i - InputChannels;
    BufferInfos[i].Buffers[0] = 0;
    BufferInfos[i].Buffers[1] = 0;
  }

  // NOTE(robin): Tell the hardware which functions we want it to call
  asio_callbacks Callbacks = {0};
  Callbacks.BufferSwitch = ASIOAudioCallbackOldStyle;
  Callbacks.BufferSwitchTimeInfo = ASIOAudioCallback;
  Callbacks.SampleRateDidChange = ASIOChangeSampleRate;
  Callbacks.ASIOMessage = ASIOMessage;

  ASIODriver->VMT->CreateBuffers(ASIODriver, BufferInfos, TotalChannels, BufferSize, &Callbacks);

  // NOTE(robin): Now fill out our channel infos. We need this in order to know the sample format
  // for each channel
  for (int i = 0; i < TotalChannels; i++)
  {
    ChannelInfos[i].ChannelIndex = BufferInfos[i].ChannelIndex;
    ChannelInfos[i].IsInput = BufferInfos[i].IsInput;
    ASIODriver->VMT->GetChannelInfo(ASIODriver, &ChannelInfos[i]);
  }

  // NOTE(robin): Fill out our global struct with the values we need
  ASIODevice.Driver = ASIODriver;
  ASIODevice.InputChannels = InputChannels;
  ASIODevice.OutputChannels = OutputChannels;
  ASIODevice.BufferSize = BufferSize;
  ASIODevice.SampleRate = SampleRate;
  ASIODevice.SupportsOutputReady = SupportsOutputReady;
  ASIODevice.Inputs = &BufferInfos[0];              // NOTE(robin): Input buffers come first
  ASIODevice.Outputs = &BufferInfos[InputChannels]; // NOTE(robin): Outputs come after input buffers
  ASIODevice.Channels = ChannelInfos;

  printf("Sample rate: %f\n", SampleRate);
  printf("Input channels: %d\n", InputChannels);
  printf("Output channels: %d\n", OutputChannels);
  printf("Buffer size: %d\n", BufferSize);

  // NOTE(robin): Tell the hardware to start calling our callbacks
  ASIODriver->VMT->Start(ASIODriver);

  Sleep(3000);

  // IMPORTANT(robin): You must call these 3 when your application terminates otherwise
  // the user will have no audio!
  ASIODriver->VMT->Stop(ASIODriver);
  ASIODriver->VMT->DisposeBuffers(ASIODriver);
  ASIODriver->VMT->Release(ASIODriver);

  return 0;
}
