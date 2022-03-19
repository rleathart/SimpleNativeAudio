#!/bin/sh

if [ ! -d build ]; then
  mkdir build
fi

pushd build > /dev/null

CommonFlags="
-g
-lm
"

if [ `uname` == "Darwin" ]; then
  CoreAudioFlags="-framework CoreAudio"
  clang $CommonFlags $CoreAudioFlags ../src/coreaudio_example.c -o coreaudio_example
  let ErrorCode+=$?
fi

if [ `uname` == "Linux" ]; then
  JackFlags="-ljack"
  clang $CommonFlags $JackFlags ../src/jack_example.c -o jack_example
  let ErrorCode+=$?

  AlsaFlags="-lasound"
  clang $CommonFlags $AlsaFlags ../src/alsa_example.c -o alsa_example
  let ErrorCode+=$?
fi

popd > /dev/null

exit $ErrorCode
