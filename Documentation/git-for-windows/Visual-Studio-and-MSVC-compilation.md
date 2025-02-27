This page curates tidbits regarding the use of Microsoft tools when editing or creating a personal version of Git.

You can comple Git using the MSVC compiler, or create a Visual Studio project for Git.

As Git upstream develops, the MSVC and VS projects can get left behind, so this page is to help folk keep up to date, keep testing, reporting breakages, and hopefully offering fixes or at least a bit of analysis using their favourite tool (you're here, so it must be..)

Current Work
------------

The aim is to be at least backward compatible to VS2008. The prior tools targeted that version, and MS provides upgrade/conversion facilities for newer versions. Back then parts of the tools were in a separate repo [MSYSGIT](https://github.com/msysgit/msysgit). Those parts have been transfered into the Git-for-Windows repository (or forks there-of) see https://github.com/git-for-windows/git/pull/256

You can now (#256) create a VS project file using the `msvc-build --vs` command in the contrib section. This will compile however there are a couple of link failures which need the right libraries to be automatically linked in the right places.

The main remaining issue is to include the auto post-build steps to install the sucessful compilation; and keep up with everything else..

Acronyms
--------

### MSVC (Micro Soft Visual studio Compiler)
MSVC is used to refer to Microsoft's compiler that lies beneath the VS IDE. Exactly what is compiled, and where it is targeted depends on the normal pre-processor #defines and #includes. MSVC already includes a number of pre-defined windows library functions.

### VSyyyy IDE (Visual Studio (some year) Integrated Development Environment
The full windows treatment of the graphics front end which allows the navigation of your source code within its own project.

### GIT_WINDOWS_NATIVE
This is the environment variable used to indicate that this is not a CYGWIN compilation, but that we are in Windows land.

### WIN32_LEAN_AND_MEAN
see https://blogs.msdn.microsoft.com/oldnewthing/20091130-00/?p=15863 This controls what additional libraries windows will import, particularly winsock which would otherwise be multiply defined.

### MSYS2
part of the (typically needed) operating stack for getting *nix / POSIX code working. see [The-difference-between-MINGW-and-MSYS2](https://github.com/git-for-windows/git/blob/HEAD/Documentation/git-for-windows/The-difference-between-MINGW-and-MSYS2.md)

### MinGW
part of the (typically needed) operating stack for getting *nix / POSIX code working. see [The-difference-between-MINGW-and-MSYS2](https://github.com/git-for-windows/git/blob/HEAD/Documentation/git-for-windows/The-difference-between-MINGW-and-MSYS2.md)
