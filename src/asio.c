/*
 * This file provides an alternative implementation of the ASIO SDK that loads device
 * drivers directly instead of obtaining handles through COM.
 *
 * IMPORTANT(robin): THIS FILE WILL NOT COMPILE ON ITS OWN!!!
 * It is expected that you #include this source file into another source file that already
 * has the necessary WINAPI functions declared, by including Windows.h for example.
 * Alternatively you can just copy and paste the contents of this file into your own codebase.
 *
 * We use the following WINAPI functions:
 *
 *              winreg.h:
 *                RegOpenKeyExW
 *                RegEnumKeyExW
 *                RegQueryValueExW
 *                RegCloseKey
 *
 *              combaseapi.h
 *                CLSIDFromString
 *
 * We also assume the definition of the fixed size types
 *              u8, u16, u32, u64 // Unsigned integers
 *              s8, s16, s32, s64 // Signed integers
 *              f32, f64,         // float, double
 *              wchar             // 16-bit wide character
 *
 * NOTE(robin): The basic structure of initialising an ASIO driver on Windows is this:
 *
 * - The Windows registry stores a list of the currently installed ASIO drivers.
 * - This list is queried to obtain the CLSID (ClassID) and DLL path of each driver.
 * - The driver DLL contains a function called DllGetClassObject which is
 *   loaded at runtime using LoadLibrary/GetProcAddress.
 * - DllGetClassObject gives us access to the IClassFactory for the device driver
 *   which will give us a pointer to a struct housing the driver functions.
 *
 * NOTE(robin): In a little more detail:
 *
 * - Registry keys housing the name and CLSID of the installed ASIO drivers are
 *   located at HKEY_LOCAL_MACHINE\SOFTWARE\ASIO\. These keys can be enumerated
 *   and queried with the WINAPI functions RegOpenKeyEx, RegEnumKeyEx, and
 *   RegQueryValueEx.
 * - Once we have the CLSID string, we lookup the COM entry for the driver in
 *   HKEY_CLASSES_ROOT\CLSID\<CLSID string here>\ which will give us the DLL
 *   path through the InprocServer32 key.
 * - One would usually call CoCreateInstance at this stage to obtain the driver
 *   vtable. Instead, we bypass COM (at least a little bit) by directly calling
 *   DllGetClassObject to obtain the driver class factory.
 * - The class factory gives us a pointer to the driver vtable through
 *   IClassFactory::CreateInstance
 */

// NOTE(robin): Disable some warnings that we know we generate
#pragma warning(push)
#pragma warning(disable: 4133) // NOTE(robin): void** to PHKEY
#pragma warning(disable: 4057) // NOTE(robin): u32* to LPDWORD
#pragma warning(disable: 5045) // NOTE(robin): Spectre mitigation warning
#pragma warning(disable: 4062) // NOTE(robin): Unhandled enum cases

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wincompatible-pointer-types"
#pragma clang diagnostic ignored "-Wswitch"
#endif

// NOTE(robin): Tell the linker which WINAPI libraries we link to
#pragma comment(lib, "advapi32")
#pragma comment(lib, "ole32")

// NOTE(robin): ASIO SDK has 4 byte align
#pragma pack(push, 4)

// NOTE(robin): These are data structures and constants defined by the ASIO SDK
typedef enum
{
  ASIOErrorOK = 0l,
  ASIOErrorSuccess = 0x3f4847a0l,
  ASIOErrorNotPresent = -1000l,
  ASIOErrorHWMalfunction,
  ASIOErrorInvalidParameter,
  ASIOErrorInvalidMode,
  ASIOErrorSPNotAdvancing,
  ASIOErrorNoClock,
  ASIOErrorNoMemory,
} asio_error;

typedef struct
{
  f64 Speed;
  s64 TimeCodeInSamples;
  s32 Flags;
  char Future[64];
} asio_time_code;

typedef enum
{
  ASIOTimeInfoSystemTimeValid     = 0x1,
  ASIOTimeInfoSamplePositionValid = 0x2,
  ASIOTimeInfoSampleRateValid     = 0x4,
  ASIOTimeInfoSpeedValid          = 0x8,
  ASIOTimeInfoSampleRateChanged   = 0x10,
  ASIOTimeInfoClockSourceChanged  = 0x20,
} asio_time_info_flags;

typedef struct
{
  f64 Speed;
  s64 SystemTime;
  s64 SamplePosition;
  f64 SampleRate;
  asio_time_info_flags Flags;
  char Reserved[12];
} asio_time_info;

typedef struct
{
  s32 Reverved[4];
  asio_time_info Info;
  asio_time_code Code;
} asio_time;

typedef struct
{
  s32 Index;
  s32 AssociatedChannel;
  s32 AssociatedGroup;
  s32 IsCurrentSource;
  char Name[32];
} asio_clock_source;

typedef enum
{
  ASIOSelectorSupported = 1l,
  ASIOSelectorEngineVersion,
  ASIOSelectorResetRequest,
  ASIOSelectorBufferSizeChange,
  ASIOSelectorResyncRequest,
  ASIOSelectorLatenciesChanged,
  ASIOSelectorSupportsTimeInfo,
  ASIOSelectorSupportsTimeCode,
  ASIOSelectorMMCCommand,
  ASIOSelectorSupportsInputMonitor,
  ASIOSelectorSupportsInputGain,
  ASIOSelectorSupportsInputMeter,
  ASIOSelectorSupportsOutputGain,
  ASIOSelectorSupportsOutputMeter,
  ASIOSelectorOverload,

  ASIOSelectorCount,
} asio_message_selector;

typedef struct
{
  void (*BufferSwitch)(s32 BufferIndex, s32 DoDirectProcess);
  void (*SampleRateDidChange)(f64 SampleRate);
  s32 (*ASIOMessage)(asio_message_selector Selector, s32 Value, void* Message, f64* Opt);
  asio_time* (*BufferSwitchTimeInfo)(asio_time* Time, s32 BufferIndex, s32 DoDirectProcess);
} asio_callbacks;

typedef struct
{
  s32 IsInput;
  s32 ChannelIndex;
  void* Buffers[2]; // NOTE(robin): Doublebuffered
} asio_buffer_info;

typedef enum
{
  ASIOSampleTypeInt16MSB   = 0l,
  ASIOSampleTypeInt24MSB   = 1,
  ASIOSampleTypeInt32MSB   = 2,
  ASIOSampleTypeFloat32MSB = 3,
  ASIOSampleTypeFloat64MSB = 4,

  ASIOSampleTypeInt32MSB16 = 8,
  ASIOSampleTypeInt32MSB18 = 9,
  ASIOSampleTypeInt32MSB20 = 10,
  ASIOSampleTypeInt32MSB24 = 11,

  ASIOSampleTypeInt16LSB   = 16,
  ASIOSampleTypeInt24LSB   = 17,
  ASIOSampleTypeInt32LSB   = 18,
  ASIOSampleTypeFloat32LSB = 19,
  ASIOSampleTypeFloat64LSB = 20,

  ASIOSampleTypeInt32LSB16 = 24,
  ASIOSampleTypeInt32LSB18 = 25,
  ASIOSampleTypeInt32LSB20 = 26,
  ASIOSampleTypeInt32LSB24 = 27,

  ASIOSampleTypeDSDInt8LSB1 = 32,
  ASIOSampleTypeDSDInt8MSB1 = 33,
  ASIOSampleTypeDSDInt8NER8 = 40,

  ASIOSampleTypeCount,
} asio_sample_type;

typedef struct
{
  s32 ChannelIndex;
  s32 IsInput;
  s32 IsActive;
  s32 ChannelGroup;
  asio_sample_type SampleType;
  char Name[32];
} asio_channel_info;

#pragma pack(pop)

// NOTE(robin): This is the actual ASIO API
typedef struct asio_driver asio_driver;
struct asio_driver
{
  struct
  {
    // NOTE(robin): IUnknown
    s32 (__stdcall *QueryInterface)(asio_driver* This, void*, void**);
    u32 (__stdcall *AddRef)(asio_driver* This);
    u32 (__stdcall *Release)(asio_driver* This);

    // NOTE(robin): IASIO
    s32 (*Init)(asio_driver* This, void* SystemHandle);
    void (*GetDriverName)(asio_driver* This, char* Name);
    s32 (*GetDriverVersion)(asio_driver* This);
    void (*GetErrorMessage)(asio_driver* This, char* Message);
    asio_error (*Start)(asio_driver* This);
    asio_error (*Stop)(asio_driver* This);
    asio_error (*GetChannels)(asio_driver* This, s32* InputChannels, s32* OutputChannels);
    asio_error (*GetLatencies)(asio_driver* This, s32* InputLatency, s32* OutputLatency);
    asio_error (*GetBufferSize)(asio_driver* This, s32* MinSize, s32* MaxSize, s32* PreferredSize, s32* Granularity);
    asio_error (*CanSampleRate)(asio_driver* This, f64 SampleRate);
    asio_error (*GetSampleRate)(asio_driver* This, f64* SampleRate);
    asio_error (*SetSampleRate)(asio_driver* This, f64 SampleRate);
    asio_error (*GetClockSources)(asio_driver* This, asio_clock_source* Clocks, s32* SourceCount);
    asio_error (*SetClockSource)(asio_driver* This, s32 Reference);
    asio_error (*GetSamplePosition)(asio_driver* This, s64* SamplePosition, s64* TimeStamp);
    asio_error (*GetChannelInfo)(asio_driver* This, asio_channel_info* ChannelInfo);
    asio_error (*CreateBuffers)(asio_driver* This, asio_buffer_info* BufferInfos, s32 ChannelsCount, s32 BufferSize, asio_callbacks* Callbacks);
    asio_error (*DisposeBuffers)(asio_driver* This);
    asio_error (*ControlPanel)(asio_driver* This);
    asio_error (*Future)(asio_driver* This, s32 selector, void *opt);
    asio_error (*OutputReady)(asio_driver* This);
  } *VMT;
};

// NOTE(robin): Here we provide some convenience functions for interacting with the registry

void ASIOGetDriverCount(u32* DriverCount)
{
  void* ASIO_HKEY_LOCAL_MACHINE = (void*)(long)0x80000002;
  u32 ASIO_KEY_READ = (0x20000 | 0x1 | 0x8 | 0x10) & ~0x10000;

  // NOTE(robin): If we can't open this key, the user has not installed any ASIO drivers
  void* ASIOKey = 0;
  RegOpenKeyExW(ASIO_HKEY_LOCAL_MACHINE, L"SOFTWARE\\ASIO", 0, ASIO_KEY_READ, &ASIOKey);

  // NOTE(robin): Read the number of subkeys contained in HKEY_LOCAL_MACHINE\SOFTWARE\ASIO.
  // This is a no-op if the above call fails.
  *DriverCount = 0;
  RegQueryInfoKeyW(ASIOKey, 0, 0, 0, DriverCount, 0, 0, 0, 0, 0, 0, 0);

  RegCloseKey(ASIOKey);
}

// NOTE(robin): We do not do any dynamic memory allocation in this function to
// avoid imposing a memory model on the caller. The caller is responsible for
// preallocating buffers to receive the string data. For example, DriverNames must be
// an array of DriverCount wchar pointers which each point to an array of DriverNameMax
// wchars. That is, DriverNames = wchar[DriverCount][DriverNameMax]. Similarly for
// ClassIDStrings and DllPaths.

void ASIOGetDriverInfo(u32 DriverCount,
    wchar** ClassIDStrings, u32 ClassIDMax,
    wchar** DriverNames, u32 DriverNameMax,
    wchar** DllPaths, u32 DllPathMax)
{
  void* ASIO_HKEY_LOCAL_MACHINE = (void*)(long)0x80000002;
  void* ASIO_HKEY_CLASSES_ROOT = (void*)(long)0x80000000;
  u32 ASIO_KEY_READ = (0x20000 | 0x1 | 0x8 | 0x10) & ~0x10000;
  u32 ASIO_REG_SZ = 1;

  void* ASIOKey = 0;
  RegOpenKeyExW(ASIO_HKEY_LOCAL_MACHINE, L"SOFTWARE\\ASIO", 0, ASIO_KEY_READ, &ASIOKey);

  for (u32 DriverIndex = 0; DriverIndex < DriverCount; DriverIndex++)
  {
    u32 DataSize = 0;
    u32 DataType = 0;

    DataSize = DriverNameMax * sizeof(wchar);
    RegEnumKeyExW(ASIOKey, DriverIndex, DriverNames[DriverIndex], &DataSize, 0, 0, 0, 0);

    void* DriverKey = 0;
    RegOpenKeyExW(ASIOKey, DriverNames[DriverIndex], 0, ASIO_KEY_READ, &DriverKey);

    // NOTE(robin): Get the ClassID string for the driver from the registry:
    // HKEY_LOCAL_MACHINE\SOFTWARE\ASIO\<Driver name> -> CLSID

    DataSize = ClassIDMax * sizeof(wchar);
    DataType = ASIO_REG_SZ;
    RegQueryValueExW(DriverKey, L"CLSID", 0, &DataType, (u8*)ClassIDStrings[DriverIndex], &DataSize);
    ClassIDStrings[DriverIndex][DataSize] = 0; // NOTE(robin): Null terminate

    // NOTE(robin): Now that we have the ClassID strings, look for the driver location in the registry.
    // Driver locations are stored in HKEY_CLASSES_ROOT\CLSID\<ClassID string>\InprocServer32 -> (Default)

    void* DllKey = 0;
    RegOpenKeyExW(ASIO_HKEY_CLASSES_ROOT, L"CLSID", 0, ASIO_KEY_READ, &DllKey);
    RegOpenKeyExW(DllKey, ClassIDStrings[DriverIndex], 0, ASIO_KEY_READ, &DllKey);
    RegOpenKeyExW(DllKey, L"InprocServer32", 0, ASIO_KEY_READ, &DllKey);

    DataSize = DllPathMax * sizeof(wchar);
    DataType = ASIO_REG_SZ;
    RegQueryValueExW(DllKey, 0, 0, &DataType, (u8*)DllPaths[DriverIndex], &DataSize);
    DllPaths[DriverIndex][DataSize] = 0; // NOTE(robin): Null terminate

    RegCloseKey(DriverKey);
    RegCloseKey(DllKey);
  }
  RegCloseKey(ASIOKey);
}

// NOTE(robin): Gets a pointer to the driver from the driver DLL
void ASIOLoadDriver(asio_driver** Driver, wchar* ClassIDString, wchar* DllPath)
{
  void* ASIODll = LoadLibraryW(DllPath);

  typedef s32 dll_get_class_object(void*, void*, void**);
  dll_get_class_object* DllGetClassObject = (dll_get_class_object*)GetProcAddress(ASIODll, "DllGetClassObject");

  typedef struct
  {
    struct
    {
      // NOTE(robin): IUnknown
      s32 (__stdcall *QueryInterface)(void* This, void*, void**);
      u32 (__stdcall *AddRef)(void* This);
      u32 (__stdcall *Release)(void* This);

      // NOTE(robin): IClassFactory
      s32 (__stdcall *CreateInstance)(void* This, void*, void*, void**);
      s32 (__stdcall *LockServer)(void* This, s32);
    } *VMT;
  } ASIOIClassFactory;

  char ClassID[16] = {0};
  CLSIDFromString(ClassIDString, ClassID);
  char IClassFactoryUUID[16] = {'\x1', 0, 0, 0, 0, 0, 0, 0, '\xc0', 0, 0, 0, 0, 0, 0, '\x46'};

  ASIOIClassFactory* Factory;
  DllGetClassObject(&ClassID, (void*)&IClassFactoryUUID, (void**)&Factory);

  Factory->VMT->CreateInstance(Factory, 0, ClassID, (void**)Driver);
}

// NOTE(robin): Now we provide a conversion function from asio_sample_type to
// asio_sample_format which makes it easier to access the data describing the format
// in a programmatic way.

typedef struct
{
  u8 IsFloat;
  u8 BitAlign; // NOTE(robin): Zero if not applicable
  u8 BytesPerSample;
  u8 IsBigEndian;
  u32 DSD;
} asio_sample_format;

asio_sample_format ASIOGetSampleFormat(asio_sample_type SampleType)
{
  asio_sample_format Result = {0};

  // NOTE(robin): Set BytesPerSample {{{
  switch (SampleType)
  {
    case ASIOSampleTypeDSDInt8LSB1:
    case ASIOSampleTypeDSDInt8MSB1:
    case ASIOSampleTypeDSDInt8NER8:
    {
      Result.BytesPerSample = 1;
    } break;

    case ASIOSampleTypeInt16LSB:
    case ASIOSampleTypeInt16MSB:
    {
      Result.BytesPerSample = 2;
    } break;

    case ASIOSampleTypeInt24LSB:
    case ASIOSampleTypeInt24MSB:
    {
      Result.BytesPerSample = 3;
    } break;

    case ASIOSampleTypeFloat32LSB:
    case ASIOSampleTypeFloat32MSB:
    case ASIOSampleTypeInt32LSB:
    case ASIOSampleTypeInt32LSB16:
    case ASIOSampleTypeInt32LSB18:
    case ASIOSampleTypeInt32LSB20:
    case ASIOSampleTypeInt32LSB24:
    case ASIOSampleTypeInt32MSB:
    case ASIOSampleTypeInt32MSB16:
    case ASIOSampleTypeInt32MSB18:
    case ASIOSampleTypeInt32MSB20:
    case ASIOSampleTypeInt32MSB24:
    {
      Result.BytesPerSample = 4;
    } break;

    case ASIOSampleTypeFloat64LSB:
    case ASIOSampleTypeFloat64MSB:
    {
      Result.BytesPerSample = 8;
    } break;
  }
  // }}}

  // NOTE(robin): Set alignment {{{
  // NOTE(robin): Some ASIO devices will send data over the PCIE bus where it is more efficient to
  // pack samples into 32-bit packets.
  switch (SampleType)
  {
    case ASIOSampleTypeDSDInt8LSB1:
    case ASIOSampleTypeDSDInt8MSB1:
    {
      Result.BitAlign = 1;
    } break;

    case ASIOSampleTypeInt32LSB16:
    case ASIOSampleTypeInt32MSB16:
    {
      Result.BitAlign = 16;
    } break;

    case ASIOSampleTypeInt32LSB18:
    case ASIOSampleTypeInt32MSB18:
    {
      Result.BitAlign = 18;
    } break;

    case ASIOSampleTypeInt32LSB20:
    case ASIOSampleTypeInt32MSB20:
    {
      Result.BitAlign = 20;
    } break;

    case ASIOSampleTypeInt32LSB24:
    case ASIOSampleTypeInt32MSB24:
    {
      Result.BitAlign = 24;
    } break;
  }
  // }}}

  // NOTE(robin): Set IsBigEndian
  switch (SampleType)
  {
    case ASIOSampleTypeInt16MSB:
    case ASIOSampleTypeInt24MSB:
    case ASIOSampleTypeInt32MSB:
    case ASIOSampleTypeFloat32MSB:
    case ASIOSampleTypeFloat64MSB:
    case ASIOSampleTypeInt32MSB16:
    case ASIOSampleTypeInt32MSB18:
    case ASIOSampleTypeInt32MSB20:
    case ASIOSampleTypeInt32MSB24:
    case ASIOSampleTypeDSDInt8MSB1:
    {
      Result.IsBigEndian = 1;
    } break;
  }

  Result.IsFloat = SampleType == ASIOSampleTypeFloat32LSB ||
                   SampleType == ASIOSampleTypeFloat32MSB ||
                   SampleType == ASIOSampleTypeFloat64LSB ||
                   SampleType == ASIOSampleTypeFloat64MSB;

  Result.DSD = SampleType == ASIOSampleTypeDSDInt8LSB1 ||
               SampleType == ASIOSampleTypeDSDInt8MSB1 ||
               SampleType == ASIOSampleTypeDSDInt8NER8;

  return Result;
}

// NOTE(robin): You may not wish to use this, it would be much better to
// convert a block of samples in your format to a block of samples in the
// hardware's format and then memcpy into the hardware buffer.  This function
// is provided mostly for reference.
//
// ASIO drivers can expect data in a variety of formats, this function converts
// from 64-bit float format to the hardware's native format and then writes the
// converted sample to the hardware buffer.
asio_error ASIOWriteSampleToHardwareBuffer(void* HardwareBuffer, s32 FrameIndex, f64 Sample, asio_sample_format Format)
{
  // NOTE(robin): DSD formats and packed sample formats are not supported yet
  if (Format.DSD || Format.BitAlign || Format.BytesPerSample > 8)
    return ASIOErrorInvalidParameter;

  // NOTE(robin): We can have at most 8 bytes per sample
  u8 SampleBytes[8] = {0};
  u32 SampleByteCount = sizeof(SampleBytes) / sizeof(SampleBytes[0]);

  if (Format.IsFloat)
  {
    switch (Format.BytesPerSample)
    {
      case sizeof(f32):
      {
        *(f32*)SampleBytes = (f32)Sample;
      } break;

      case sizeof(f64):
      {
        *(f64*)SampleBytes = Sample;
      } break;

      default:
      {
        return ASIOErrorInvalidParameter;
      }
    }
  }
  else // NOTE(robin): Integer sample format
  {
    s32 SampleMaxValue = (1 << (Format.BytesPerSample * 8 - 1)) - 1;
    s32 IntegerSampleValue = (s32)(SampleMaxValue * Sample);

    u8* IntegerSampleBytes = (u8*)&IntegerSampleValue;
    for (u32 i = 0; i < Format.BytesPerSample; i++)
      SampleBytes[i] = IntegerSampleBytes[i];
  }

  // NOTE(robin): Swap the bytes in SampleBytes if we're big endian
  if (Format.IsBigEndian)
  {
    for (u64 i = 0; i < SampleByteCount / 2; i++)
    {
      u8 Temp = SampleBytes[i];
      SampleBytes[i] = SampleBytes[SampleByteCount - 1 - i];
      SampleBytes[SampleByteCount - 1 - i] = Temp;
    }
  }

  u8* HardwareByteBuffer = (u8*)HardwareBuffer + FrameIndex * Format.BytesPerSample;

  u32 Start = Format.IsBigEndian ? SampleByteCount - Format.BytesPerSample : 0;
  u32 End = Format.IsBigEndian ? SampleByteCount : Format.BytesPerSample;
  for (u32 i = Start; i < End; i++)
    HardwareByteBuffer[i] = SampleBytes[i];

  return ASIOErrorOK;
}

#pragma warning(pop)
#ifdef __clang__
#pragma clang diagnostic pop
#endif
