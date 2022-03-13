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
  snd_pcm_t* PlaybackHandle;
  snd_pcm_hw_params_t* HardwareParams;
  snd_pcm_open(&PlaybackHandle, "plughw:0,0", SND_PCM_STREAM_PLAYBACK, 0);
  alsa_data ALSAData = {0};
  ALSAData.PlaybackHandle = PlaybackHandle;

  snd_pcm_hw_params_malloc(&HardwareParams);
  snd_pcm_hw_params_any(PlaybackHandle, HardwareParams);

  snd_pcm_hw_params_set_access(PlaybackHandle, HardwareParams, SND_PCM_ACCESS_RW_INTERLEAVED);
  snd_pcm_hw_params_set_format(PlaybackHandle, HardwareParams, SND_PCM_FORMAT_FLOAT_LE);
  unsigned int SampleRate = 44100;
  snd_pcm_hw_params_set_rate_near(PlaybackHandle, HardwareParams, &SampleRate, 0);
  snd_pcm_hw_params_set_channels(PlaybackHandle, HardwareParams, 2);
  snd_pcm_hw_params (PlaybackHandle, HardwareParams);

  long BufferSize = 512;

  snd_pcm_sw_params_t* SoftwareParams;
  snd_pcm_sw_params_malloc(&SoftwareParams);
  snd_pcm_sw_params_current(PlaybackHandle, SoftwareParams);
  snd_pcm_sw_params_set_avail_min(PlaybackHandle, SoftwareParams, BufferSize);
  snd_pcm_sw_params_set_start_threshold(PlaybackHandle, SoftwareParams, 0);
  snd_pcm_sw_params(PlaybackHandle, SoftwareParams);

  snd_pcm_prepare(PlaybackHandle);

  snd_pcm_hw_params_get_rate(HardwareParams, &ALSAData.SampleRate, 0);

  // NOTE(robin): You would probably have this in a separate thread
  while (1)
  {
    snd_pcm_wait(PlaybackHandle, -1); // NOTE(robin): Block until buffer is ready
    AudioCallback(BufferSize, &ALSAData);
  }

  snd_pcm_close (PlaybackHandle);
  return 0;
}
