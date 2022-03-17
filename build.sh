#!/bin/sh

if [ ! -d build ]; then
  mkdir build
fi

pushd build > /dev/null

CommonFlags="
-g
"

if [ `uname` == "Darwin" ]; then
  CoreAudioFlags="
  -framework CoreAudio
  "
  clang $CommonFlags $CoreAudioFlags ../src/coreaudio_example.c -o coreaudio_example
fi

if [ `uname` == "Linux" ]; then
  JackFlags="
  -ljack
  -lm
  "
  clang $CommonFlags $JackFlags ../src/jack_example.c -o jack_example
  AlsaFlags="
  -lasound
  -lm
  "
  clang $CommonFlags $AlsaFlags ../src/alsa_example.c -o alsa_example
fi

ErrorCode=$?

popd > /dev/null

exit $ErrorCode
