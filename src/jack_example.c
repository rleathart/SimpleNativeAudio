#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <assert.h>
#include <jack/jack.h>

typedef struct
{
  jack_port_t* OutputPorts[2];
  jack_port_t* InputPorts[2];
  jack_client_t* JackClient;
} jack_callback_data;

// NOTE(robin): This needs to be a global sadly so it can be accessed in the
// SignalHandler
jack_callback_data JackData;

void OnJackShutdown(void* Context)
{
  exit(1);
}

int AudioCallback(uint32_t FrameCount, void* Context)
{
  jack_callback_data* JackData = Context;

  float* Left = jack_port_get_buffer(JackData->OutputPorts[0], FrameCount);
  float* Right = jack_port_get_buffer(JackData->OutputPorts[1], FrameCount);

  // NOTE(robin): Data for the first channel of input
  float* Input1 = jack_port_get_buffer(JackData->InputPorts[0], FrameCount);

  float SampleRate = jack_get_sample_rate(JackData->JackClient);

  static float Phase[2];
  float PhaseDelta[] =
  {
    220.0f/SampleRate,
    330.0f/SampleRate,
  };

  for (uint32_t i = 0; i < FrameCount; i++)
  {
    for (int i = 0; i < 2; i++)
    {
      Phase[i] += PhaseDelta[i];
      if (Phase[i] >= 1.0f)
        Phase[i] -= 1.0f;
    }

    float Volume = 0.1f;

    Left[i] = Volume * sin(Phase[0] * 2 * M_PI);
    Right[i] = Volume * sin(Phase[1] * 2 * M_PI);
  }

  return 0;
}

int main(int argc, char** argv)
{
  jack_status_t JackStatus;

  JackData.JackClient = jack_client_open("SimpleNativeAudio", JackNullOption, &JackStatus, 0);
  assert(JackData.JackClient);

  jack_set_process_callback(JackData.JackClient, AudioCallback, &JackData);
  jack_on_shutdown(JackData.JackClient, OnJackShutdown, 0);

  uint32_t BufferSize = jack_get_buffer_size(JackData.JackClient);
  printf("Default buffer size is: %d\n", BufferSize);

  BufferSize = 128; // NOTE(robin): Override the default buffer size here
  jack_set_buffer_size(JackData.JackClient, BufferSize);

  BufferSize = jack_get_buffer_size(JackData.JackClient);
  printf("Actual buffer size was set to: %d\n", BufferSize);

  JackData.OutputPorts[0] = jack_port_register(JackData.JackClient, "Output1",
      JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);
  JackData.OutputPorts[1] = jack_port_register(JackData.JackClient, "Output2",
      JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0);

  JackData.InputPorts[0] = jack_port_register(JackData.JackClient, "Input1",
      JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
  JackData.InputPorts[1] = jack_port_register(JackData.JackClient, "Input2",
      JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);

  // NOTE(robin): Tell the hardware to start calling our callback
  jack_activate(JackData.JackClient);

  const char** JackPorts = 0;
  // NOTE(robin): Audio outputs
  // NOTE(robin): JackPortIsInput refers to an input to the backend. Thus
  // outputs from our program are inputs to the backend and vice versa.
  JackPorts = jack_get_ports(JackData.JackClient, NULL, NULL, JackPortIsPhysical|JackPortIsInput);
  assert(JackPorts);

  for (int i = 0; i < 2; i++)
  {
    int Error = jack_connect(JackData.JackClient, jack_port_name(JackData.OutputPorts[i]), JackPorts[i]);
    assert(!Error);
  }

  jack_free(JackPorts);

  // NOTE(robin): Audio inputs
  JackPorts = jack_get_ports(JackData.JackClient, NULL, NULL,
      JackPortIsPhysical|JackPortIsOutput);
  assert(JackPorts);

  for (int i = 0; i < 2; i++)
  {
    int Error = jack_connect(JackData.JackClient, JackPorts[i], jack_port_name(JackData.InputPorts[i]));
    assert(!Error);
  }

  jack_free(JackPorts);

  sleep(3);

  jack_client_close(JackData.JackClient);

  return 0;
}
