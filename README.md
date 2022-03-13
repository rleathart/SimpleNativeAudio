# SimpleNativeAudio
Example code for accessing audio hardware with ASIO, CoreAudio, and JACK

# Build instructions

## macOS/Linux:

```bash
chmod +x build.sh
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
