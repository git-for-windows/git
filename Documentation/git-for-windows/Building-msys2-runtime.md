# Installing a build environment

1. Download and run the [Git for Windows SDK installer](https://gitforwindows.org/#download-sdk).

# Building msys2-runtime

2. Open the Git for Windows SDK MSys shell by double-clicking the `msys2_shell.cmd` script in the install folder (by default `C:\git-sdk-<arch>`).

3. From the MSys shell do: `sdk build msys2-runtime`

This last step may take a while, so please be patient!

# Rebuilding the msys2-runtime

When there are bugs in the msys2-runtime (e.g. problems with the POSIX-to-Windows path mangling), you need to rebuild the `msys-2.0.dll`, possibly frequently.

To rebuild the `msys-2.0.dll` -- assuming that you have built it already as described above -- you first need to start an *MSys* shell by double-clicking the `msys2_shell.cmd` script in your Git SDK's top-level directory. Then call `cd /usr/src/MSYS2-packages/msys2-runtime/src/msys2-runtime/winsup/cygwin`. Now you can modify the source code to your heart's content. It is actually a local clone of https://github.com/git-for-windows/msys2-runtime; you probably want to add your own fork as a remote.

To actually build the `msys-2.0.dll`, switch to `src/build-<arch>-pc-msys/<arch>-pc-msys/winsup/cygwin` and type `make`. This will generate an `msys0.dll` file in that directory. This is the new `msys-2.0.dll`, but you cannot simply copy it into `/usr/bin/` because it is in use by the current `mintty` as well as by the current Bash. To update, you need to close all programs using the msys2-runtime (including all of the terminal windows), then copy the `msys0.dll` file, replacing the existing `/usr/bin/msys-2.0.dll`, either using Explorer, `cmd.exe` or a separate Git SDK (for example, when debugging the 64-bit msys2-runtime, a 32-bit Git SDK comes in real handy). You will **want** to make a backup copy of the old `msys-2.0.dll`...

# Related: building/rebuilding Bash

The process to rebuild the `Bash` is very similar to the `msys2-runtime` one; You will just need to work in the `src/bash-<version>/` subdirectory of `/usr/src/MSYS2-packages/bash/` (both the sources and the generated `bash.exe` live there). Bash also requires all Bash instances to be exited before the executable is replaced by the new one. And like with the msys2-runtime, you will **want** to make a backup copy of the old `bash.exe`.