#include <alsa/asoundlib.h>
#include <math.h>

typedef struct
{
  snd_pcm_t* PlaybackHandle;
  unsigned int SampleRate;
} alsa_data;

int AudioCallback(long FrameCount, void* UserData)
{
  alsa_data* ALSAData = UserData;
  static float Phase[2] = {0, 0};
  float PhaseDelta[] =
  {
    220.0f/(float)ALSAData->SampleRate,
    330.0f/(float)ALSAData->SampleRate,
  };

  // NOTE(robin): You'd want to create this in a smarter way
  float AudioBuffer[2048];
  // NOTE(robin): Interleaved so we have Channels * FrameCount samples to write
  for (int i = 0; i < 2 * FrameCount; i++)
  {
    for (int i = 0; i < 2; i++)
    {
      Phase[i] += PhaseDelta[i];
      if (Phase[i] >= 1.0f)
        Phase[i] -= 1.0f;
    }

    float Volume = 0.2;
    AudioBuffer[i] = Volume * sin(Phase[0] * 2 * M_PI);
    AudioBuffer[++i] = Volume * sin(Phase[1] * 2 * M_PI);
  }

  int FramesWritten = snd_pcm_writei(ALSAData->PlaybackHandle, AudioBuffer, FrameCount);
  return FramesWritten;
}

int main(int argc, char* argv[])
{
  int Error = 0;

  // NOTE(robin): "plughw:0,0" vs "hw:0,0" allows us to set a virtual sample format
  // which will be converted to the hardware sample format by the kernel/device
  // driver. If you want to write directly to the hardware buffer (which
  // requires obtaining the hardware sample format and converting to it), then
  // you can use hw:0,0. "default" is also an option.
  snd_pcm_t* PlaybackHandle;
  Error = snd_pcm_open(&PlaybackHandle, "plughw:0,0", SND_PCM_STREAM_PLAYBACK, 0);

  if (Error)
  {
    printf("%s\n", snd_strerror(Error));
    assert(!"Failed to open your device, perhaps it is already busy?");
  }

  snd_pcm_hw_params_t* HardwareParams;
  snd_pcm_hw_params_malloc(&HardwareParams);
  snd_pcm_hw_params_any(PlaybackHandle, HardwareParams);

  // NOTE(robin): Set the sample format to interleaved 32 bit float (little endian).
  snd_pcm_hw_params_set_access(PlaybackHandle, HardwareParams, SND_PCM_ACCESS_RW_INTERLEAVED);
  snd_pcm_hw_params_set_format(PlaybackHandle, HardwareParams, SND_PCM_FORMAT_FLOAT_LE);

  unsigned int SampleRate = 44100;
  snd_pcm_hw_params_set_rate_near(PlaybackHandle, HardwareParams, &SampleRate, 0);
  snd_pcm_hw_params_set_channels(PlaybackHandle, HardwareParams, 2);
  snd_pcm_hw_params (PlaybackHandle, HardwareParams);

  snd_pcm_sw_params_t* SoftwareParams;
  snd_pcm_sw_params_malloc(&SoftwareParams);
  snd_pcm_sw_params_current(PlaybackHandle, SoftwareParams);

  long BufferSize = 512;
  snd_pcm_sw_params_set_avail_min(PlaybackHandle, SoftwareParams, BufferSize);

  snd_pcm_sw_params_set_start_threshold(PlaybackHandle, SoftwareParams, 0);
  snd_pcm_sw_params(PlaybackHandle, SoftwareParams);

  snd_pcm_prepare(PlaybackHandle);

  snd_pcm_hw_params_get_rate(HardwareParams, &SampleRate, 0);

  alsa_data ALSAData = {0};
  ALSAData.PlaybackHandle = PlaybackHandle;
  ALSAData.SampleRate = SampleRate;

  printf("Sample rate: %u\n", SampleRate);
  printf("Buffer size: %ld\n", BufferSize);

  // NOTE(robin): You would probably have this in a separate thread
  while (1)
  {
    snd_pcm_wait(PlaybackHandle, -1); // NOTE(robin): Block until buffer is ready
    AudioCallback(BufferSize, &ALSAData);
  }

  snd_pcm_close (PlaybackHandle);
  return 0;
}
