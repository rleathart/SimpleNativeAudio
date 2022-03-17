# SimpleNativeAudio
Example code for accessing audio hardware with ASIO, CoreAudio, and JACK.

Tested with Clang 12.0.0 (macOS/Linux) and MSVC 19.29.30133 for x64.

# Build instructions

## macOS/Linux:

```bash
cd path/to/this/repo
sh build.sh
```

You can then run the examples with

```bash
build/coreaudio_example             (for mac)
build/jack_example                  (for linux)
```

Note for macOS that if you want your program to be able to get input from the
microphone, you will need to run the binary from another process that already
has microphone permissions.

# Windows

Open `x64 Native Tools Command Prompt`

```cmd
cd path\to\this\repo
build
```

You can then run the examples with

```cmd
build\asio_example             (if you have an asio driver and hardware)
build\wasapi_example           (if you don't have asio)
```

Note that for the WASAPI example you need to enable exclusive mode for your
sound device (both input and output):

Sound Settings->Device properties->Additional device properties->Advanced->Allow applications...

# Notes

This is just example code, it is by no means robust. Error checking has been
omitted to make the source easier to read. If you want to add your own checks
it should be trivial. The code in the repo is intended to give you a starting
point for code that you might use in your own program.

## Code style

Yes it's a little weird. It's just what I've found easiest to read/type over the years.
Types are `snake_case`, both variables and functions are `PascalCase`.
