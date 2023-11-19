# Patch for ZebraNativeUsbAdapter_64.dll (memory allocator workaround)
## Crash

Zebra Link-OS Multiplatform SDK for Desktop Java - Card (Build: 2.12.3782)
comes with the `ZebraNativeUsbAdapter_64.dll`, although there is an implementation
bug that would cause the SDK to crash with the following error when running
under 64 bit Java.

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

**Applying a patch:**
1. **Download release: [Patch_ZebraNativeUsbAdapter_64_20231119R04.zip](https://github.com/7c5eea120b/zxp-native-usb-adapter-64bit-hack/releases/download/build-20231119R04/Patch_ZebraNativeUsbAdapter_64_20231119R04.zip)**
2. **Replace your original `ZebraNativeUsbAdapter_64.dll` with the two DLL files that are contained in the archive.**

That's it, now it should work under the recent versions of 64 bit Java JRE.

See "Known bugs and limitations" section below if you are still having issues.

## Technical details

Whenever Java SDK asks to open the connection with the printer, the original DLL would allocate
a heap object representing the printer connection and return a pointer back to Java SDK. However, there is
an implementation bug that would cause the DLL to cast the pointer to `uint32_t` before returning it.
Similarly, the exported functions that allow subsequent printer interaction would also shrink the pointer right
in their prologue.

The patch features a special proxy DLL called `MQALLOC.dll` that re-implements the C++ dynamic
allocator (`operator new` and `operator delete`). The additional DLL will ensure that heap objects are always
allocated on low virtual addresses (pointer values significantly below 2^32). Thus, the pointer values will
not get damaged, even if they would be inappropriately casted to `uint32_t` anywhere.

The implementation of the allocator is extremely rudimental but sufficient for this particular case.
The entire original DLL would only allocate one type of objects, all with the same and known size.

### Known bugs and limitations

* This patch's implementation will throw an exception once there are more than 468 open printer handles
  at once (this shouldn't ever happen in practice, unless there is a bug somewhere else).
* Sometimes Windows fails to resolve the patched DLL's dependencies and you might get `UnsatisfiedLinkError: Can't find dependent libraries`.
  If you are getting this error, you can work it around by placing the following code at the very beginning of your Java application:
  ```java
  if (!System.getProperty("sun.arch.data.model").equals("32")) {
      while (true) {
          try {
              System.loadLibrary("ZebraNativeUsbAdapter_64");
              break;
          } catch (UnsatisfiedLinkError e) {
              try {
                  Thread.sleep(250);
              } catch (InterruptedException ex) {
                  throw new RuntimeException(ex);
              }
          }
      }
  }
  ```
  The DLL is eventually going to load after a few attempts. I have no idea why this is randomly happening (race condition in Windows DLL loader?).

### Debugging

Set `DEBUG_MQALLOC=1` environment variable and launch Java with your application.
Debug printouts from the allocator will be written on standard error stream.

### Building locally

Only if you want to build this patch from scratch:

1. Get the original `ZebraNativeUsbAdapter_64.dll` from the SDK (mod time: 2016-11-08T22:33:33; SHA256: `034bd1293128507120005ebb6a5ba510b614932292e648e15a77677c09c63f1e`).
2. Open the binary in hex editor and search for the only occurrence of `MSVCR90` string. Replace it with `MQALLOC` and save.
3. Build the source code from this repository with MSVC, that would generate additional DLL called `MQALLOC.dll`.
4. Use the patched `ZebraNativeUsbAdapter_64.dll` that you've got from step (2) together with `MQALLOC.dll` (in the same directory) instead of the original DLL.

## Disclaimer

Information and binary releases are provided here only to increase the compatibility.
