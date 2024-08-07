A common source for exploits has been to cause buffer overflows. When executables are loaded to the same address range every time they are launched, such buffer overflows can rely on encountering the same scenario every time, which makes it easier to exploit them.

To remedy that, [Address Space Layout Randomization (ASLR)](https://msrc-blog.microsoft.com/2013/12/11/software-defense-mitigating-common-exploitation-techniques/) has been invented. Essentially, it causes executables and dynamic libraries to be loaded into somewhat random memory locations, making the common buffer overflows much harder to exploit. There is even an option to require all programs to use ASLR, whether the executables indicate that they want that or not: [Mandatory ASLR](https://msrc-blog.microsoft.com/2017/11/21/clarifying-the-behavior-of-mandatory-aslr/).

As per [an MSYS2 post](https://www.msys2.org/news/#2021-01-31-aslr-enabled-by-default), MSYS2 enabled ASLR, and so should Git for Windows. So why doesn't Git for Windows work under Mandatory ASLR?

Well, that MSYS2 post left out something crucial. To understand that, we need to add a bit of background first. Historically, most Open Source software depends on POSIX semantics that are not available on Windows, e.g. the often-mentioned `fork()` syscall (there were some subsystems in Windows that supported that syscall, but programs linking to the regular Win32 API cannot use that). Some software has been ported to pure Win32 calls (e.g. using `CreateProcessW()`), but the easier approach is to use a POSIX emulation layer like [Cygwin](https://cygwin.com/) (of which [the MSYS2 runtime](https://github.com/msys2/msys2-runtime) is a close derivative). That "ease of porting" comes at the cost of performance, and of the inability to use ASLR.

MSYS2's packages that depend on that POSIX emulation layer are called "MSYS packages", the other packages are called "MINGW packages". [That MSYS2 post](https://www.msys2.org/news/#2021-01-31-aslr-enabled-by-default) did not make it clear, but it talked only about the MINGW packages. It is a bit clearer in [the ticket](https://github.com/msys2/MINGW-packages/issues/6674) that lead to the work that was described in that post (note that it is a ticket in the `msys2/MINGW-packages` repository).

Git for Windows bundles some MINGW programs (such as `git.exe`) and some MSYS programs (such as `bash.exe`). The MINGW programs already use ASLR, the MSYS programs do not. See for yourself:

PS C:\> dumpbin /headers 'C:\Program Files\Git\mingw64\bin\libcurl-4.dll' | grep -i dynamic
                   Dynamic base
PS C:\> dumpbin /headers 'C:\Program Files\Git\mingw64\bin\git.exe' | grep -i dynamic
                   Dynamic base
PS C:\> dumpbin /headers 'C:\Program Files\Git\usr\bin\bash.exe' | grep -i dynamic
PS C:\> dumpbin /headers 'C:\Program Files\Git\usr\bin\msys-2.0.dll' | grep -i dynamic

The "Dynamic base" indicates that ASLR is enabled. The absence indicates that it is disabled. By the way, that is no different from regular MSYS2's situation:

PS C:\> dumpbin /headers 'C:\msys64\usr\bin\msys-2.0.dll' | grep -i dynamic
PS C:\> dumpbin /headers 'C:\msys64\usr\bin\bash.exe' | grep -i dynamic

There have been many posts to Cygwin asking for ASLR support (and let's make this utterly clear: if Cygwin cannot enable ASLR, neither can the MSYS2 runtime, it's either both or none), e.g. [here](https://cygwin.cygwin.narkive.com/Zrqc0stg/aslr-breaks#), [here](https://x.cygwin.com/ml/cygwin/current/244156.html) and [here](https://www.mail-archive.com/cygwin%40cygwin.com/msg165776.html). Noone has found any answer to the fundamental design problem yet: How could `fork()` be emulated without requiring an identical address for the Cygwin heap in all parent/child processes?