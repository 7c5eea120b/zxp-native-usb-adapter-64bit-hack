# Patch for ZebraNativeUsbAdapter_64.dll (memory allocator workaround)

Zebra Link-OS Multiplatform SDK for Desktop Java - Card (Build: 2.12.3782)
comes with the `ZebraNativeUsbAdapter_64.dll`, although there is an implementation
bug that would cause the SDK to crash with the following error when running
under 64 bit Java.

## Crash
```
#
# A fatal error has been detected by the Java Runtime Environment:
#
#  EXCEPTION_ACCESS_VIOLATION (0xc0000005) at pc=0x00007ffedbdb25be, pid=11668, tid=0x0000000000002504
#
# JRE version: Java(TM) SE Runtime Environment (8.0_381) (build 1.8.0_381-b09)
# Java VM: Java HotSpot(TM) 64-Bit Server VM (25.381-b09 mixed mode windows-amd64 compressed oops)
# Problematic frame:
# C  [ZebraNativeUsbAdapter_64.dll+0x25be]
#
# Failed to write core dump. Minidumps are not enabled by default on client versions of Windows
#
# If you would like to submit a bug report, please visit:
#   http://bugreport.java.com/bugreport/crash.jsp
# The crash happened outside the Java Virtual Machine in native code.
# See problematic frame for where to report the bug.
#
```

## Patch

1. Download release: [Patch_ZebraNativeUsbAdapter_64_20231119R03.zip](https://github.com/7c5eea120b/zxp-native-usb-adapter-64bit-hack/releases/download/build-20231119R03/Patch_ZebraNativeUsbAdapter_64_20231119R03.zip)
2. Replace your original `ZebraNativeUsbAdapter_64.dll` with the two DLL files that are contained in the archive.

### Technical details

Whenever Java SDK asks to open the connection with the printer, the original DLL would allocate
a heap object representing the printer connection and return a pointer back to Java SDK. However, there is
an implementation bug that would cause the DLL to cast the pointer to `uint32_t` before returning it.
Similarly, the exported functions that allow subsequent printer interaction would also shrink the pointer right
in their prologue.

The patch features a special proxy DLL called `MQALLOC.dll` that re-implements the C++ dynamic
allocator (`operator new` and `operator delete`). The additional DLL will ensure that heap objects are always
allocated on low virtual addresses (pointer values significantly below 2^32). Thus, the pointer values will
not get damaged, even if they would be inappropriately casted to `uint32_t` anywhere.

### Known limitations

* The hack would cause a crash if there are more than 468 open printer handles at once
  (this shouldn't ever happen in practice, unless there is a bug somewhere else).

### Debugging

Set `DEBUG_MQALLOC=1` environment variable and launch Java with your application.
Debug printouts from the allocator will be written on standard error stream.
