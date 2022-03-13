# SimpleNativeAudio
Example code for accessing audio hardware with ASIO, CoreAudio, and JACK.

Tested with Clang 12.0.0 (macOS/Linux) and MSVC 19.29.30133 for x64.

# Build instructions

## macOS/Linux:

```bash
./build.sh
```

# Windows

Open `x64 Native Tools Command Prompt`

```cmd
cd path\to\this\repo
build
```

If you want to build with Clang (this is unsupported):

```cmd
clang src\asio_example.c
```

# Notes

This is just example code, it is by no means robust. Error checking has been
omitted to make the source easier to read. If you want to add your own checks
it should be trivial. The code in the repo is intended to give you a starting
point for code that you might use in your own program.

## Code style

Yes it's a little weird. It's just what I've found easiest to read/type over the years.
Types are `snake_case`, both variables and functions are `PascalCase`.
