# SimpleNativeAudio
Ever wanted to write a simple program in C that plays some audio in real-time?
This repository will give you some example code that should help you to get up
and running!

Example code for accessing audio hardware in C with ASIO, WASAPI, CoreAudio,
JACK and ALSA.

All example code can be found in the `src` directory.

# Build instructions

The source code is compiled using simple shell scripts. See below for
instructions on your platform.

## macOS/Linux:

```bash
cd path/to/this/repo         (e.g. cd /Users/username/Downloads/SimpleNativeAudio)
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

## Windows

Open `x64 Native Tools Command Prompt`

```cmd
cd path\to\this\repo         (e.g. cd \Users\username\Downloads\SimpleNativeAudio)
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

For the ASIO example, you will need to have an ASIO driver installed on your
system. If you have an audio interface then it's likely that you already do, if
not you will need to check the manufacturer's website for how to obtain the
driver for your interface.

If you **don't** have an audio interface, then you can try a driver like
[ASIO4ALL](https://www.asio4all.org). Note that the ASIO4ALL driver can be a bit
finicky and is not really supported. If you want to use ASIO properly, then
**you need actual hardware that supports it**. 

# Notes

Tested with Clang 12.0.0 (macOS/Linux) and MSVC 19.29.30133 for x64 (Windows).

This is just example code, it is by no means robust. Error checking has been
omitted to make the source easier to read. If you want to add your own checks
it should be trivial. The code in the repo is intended to give you a starting
point for code that you might use in your own program.

## ASIO4ALL

If you're attempting to use ASIO4ALL, then you will need to tell ASIO4ALL which
device you want to use. When you run the `asio_example` program, a small green
play button will appear in your system tray. You'll need to click this to open
the configuration interface for ASIO4ALL. Unfortunately you can only do this
while the program is running so you will need to **modify the source code** in
`src/asio_example.c` to change the `Sleep(3000)` call at the bottom to something
that gives you more time to change the ASIO4ALL settings. Something like
`Sleep(20000)` will give you 20 seconds. You may need to tell ASIO4ALL which
device to use a few times before it works.

## Code style

Yes it's a little weird. It's just what I've found easiest to read/type over the years.
Types are `snake_case`, both variables and functions are `PascalCase`.
