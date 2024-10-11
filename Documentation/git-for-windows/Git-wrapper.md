One of the more confusing aspects of Git for Windows is that its main entry points (`<Git>\git-bash.exe`, `<Git>\cmd\git.exe`, `<Git>\bin\bash.exe`, where `<Git>` is the location into which Git was installed, typically `C:\Program Files\Git`) are _not_ the actual binaries implementing the functionality.

All of these executables are variants of the ["Git wrapper"](https://github.com/git-for-windows/MINGW-packages/blob/main/mingw-w64-git/git-wrapper.c). Its only purpose is to set a couple of environment variables and then spawn the _actual_ program. For example, `Git/bin/bash.exe` will set `MSYSTEM` and `PATH` and spawn `Git/usr/bin/bash.exe`.

# Why is this needed?

The actual programs, e.g. `bash.exe` or `git.exe`, live in `<Git>\usr\bin` and `<Git>\mingw64\bin` (or `<Git>\mingw32\bin` in 32-bit setups), and the issue is that either links dynamically to some `.dll` files (e.g. `msys-2.0.dll`). So if we were to add those directories to the `PATH` variable, 3rd-party software that links _to the same_ `.dll` files would quite possibly be broken (or Git's own programs). Even worse, some of the other `.dll` files present in those folders are in much more widespread use, e.g. OpenSSL. This has been the cause for quite a few tickets in the past, and the `<Git>\cmd` directory was added to help this, at first having Batch scripts for `git`, `git-gui` and `gitk`, and later those were turned into Visual Basic scripts, but eventually we just bit the bullet and turned them into real executables because we needed more control than scripts allowed us (e.g. to prevent ugly console windows from "flashing").

# What environment variables are overridden by the Git wrapper?

- **`PATH`**

   First of all, the `PATH` variable is modified, so that Git's own `bin` folders come first. This is needed e.g. to ensure that those Git commands that are still implemented as Unix shell scripts find the expected commands (`find.exe`, for example, which is a namesake of `C:\Windows\system32\find.exe`, but the latter has a very different purpose from the [POSIX `find`](https://pubs.opengroup.org/onlinepubs/009695399/utilities/find.html) expected by Git).

- **`HOME`**

  The next-important environment variable which the Git wrapper ensures exists is `HOME`, following Git's expectations that that variable exists and points to the current user's home directory. It is unfortunately not quite trivial to figure out what is the correct value for this (find out about some challenges e.g. in [this ticket](https://github.com/git-for-windows/git/issues/2709), demonstrating that `USERPROFILE` is not always the best answer).

- **`MSYSTEM`**

  To accommodate Git's expectations where it assumes e.g. a Unix shell to be present on the `PATH`, Git for Windows ships with a subset of [MSYS2](https://msys2.github.io/) (find out about more historical context [here](https://github.com/git-for-windows/git/blob/HEAD/Documentation/git-for-windows/Home.md#about)). MSYS2 can be run in two flavors, [MINGW and MSYS](https://github.com/git-for-windows/git/blob/HEAD/Documentation/git-for-windows/The-difference-between-MINGW-and-MSYS2.md) and the flavor Git for Windows needs is MINGW. This is specified via the `MSYSTEM` variable.

- **`PLINK_PROTOCOL`**

  While [Plink](https://the.earth.li/~sgtatham/putty/0.74/htmldoc/Chapter7.html#plink) does not enjoy first-class support in Git for Windows (the much-preferred solution is to use the included OpenSSH client), for historical reasons, Git for Windows offers support for it when an existing Plink configuration is detected. To avoid Plink from thinking that it should use the `telnet` protocol instead of the `ssh` protocol, we set `PLINK_PROTOCOL` (unless it is already defined).

For full information, read the source code of [the `setup_environment()` function of the Git wrapper](https://github.com/git-for-windows/MINGW-packages/blob/0a7407a39c3015cc7a3c296d8a0db38439c65eed/mingw-w64-git/git-wrapper.c#L116-L200).

# Avoiding the Git wrapper

Especially when running scripts that call Git tens of thousands of times, the start-up cost for the Git wrapper might be noticed, and users might want to side-step it. To address this desire, starting with https://github.com/git-for-windows/git/pull/2506 Git for Windows supports the use case the hard-coded absolute path to `<Git>\mingw64\bin\git.exe` is used to launch Git, without the need to add it to the `PATH`.