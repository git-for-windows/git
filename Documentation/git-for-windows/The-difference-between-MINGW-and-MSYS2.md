# tl;dr

MINGW refers to executables that are compiled using the MINGW GCC Compiler and target the Win32 API. MSYS2 refers to executables that are compiled by MSYS2 GCC Compiler and make use of a POSIX emulation layer.

# Background

First of all, it needs to be noted that many parts of Git are not written in portable C; Git relies on a POSIX shell and Perl to be available instead.

## Git for Windows 1.x (MINGW and MSys)

To support the scripts, Git for Windows has to ship a minimal POSIX emulation layer with Bash and Perl thrown in, and when the Git for Windows effort started in August 2007, it used MSys, a stripped down version of Cygwin. Consequently, the original name of the project was "msysGit" (which, sadly, caused a *lot* of confusion because few Windows users know about MSys, and even less care).

To compile the C code of Git for Windows, MSys was used, too: it sports two versions of the GNU C Compiler: one that links implicitly to the POSIX emulation layer, and another one that targets the plain Win32 API (with a few convenience functions thrown in). For performance reasons (the POSIX emulation layer puts a serious dent into execution speed), Git for Windows' executables are built using the latter, and therefore they are really just Win32 programs. To discern executables requiring the POSIX emulation layer from the ones that do not, the latter are called MinGW (Minimal GNU for Windows) when the former are called MSys executables.

This reliance on MSys incurred challenges, too, though: some of our changes to the MSys runtime -- necessary to support Git for Windows better -- were not accepted upstream, so we had to maintain our own fork. Also, the MSys runtime was not developed further to support e.g. UTF-8 or 64-bit, and apart from lacking a package management system
until much later (when `mingw-get` was introduced), many packages provided by the MSys/MinGW project lag behind the respective source code versions, in particular Bash and OpenSSL. For a while, the Git for Windows project tried to remedy the situation by trying to build newer versions of those packages, but the situation quickly became untenable,
especially with problems like the Heartbleed bug requiring swift action that has nothing to do with developing Git for Windows further.

## Git for Windows 2.x (MINGW and MSYS2)

Happily, in the meantime [the MSYS2 project](https://msys2.github.io/) emerged, and was chosen to be the base of the Git for Windows 2.x. Just like MSys, MSYS2 is a stripped down version of [Cygwin](https://cygwin.com), but it is actively kept up-to-date with Cygwin's newest revisions. Thereby it already supports Unicode internally, and it also offers the 64-bit support that we yearned for since the beginning of the Git for Windows project.

MSYS2 also ported the Pacman package management system from Arch Linux and uses it heavily. This brings the same convenience to which Linux users are used (from `yum` or `apt-get`), and to which MacOSX users are used (from Homebrew or MacPorts), or BSD users (from the Ports system), to MSYS2: a simple `pacman -Syu` will update all installed packages to the newest versions currently available.

MSYS2 is also *very* active, typically providing package updates multiple times per week.

It still required a two-month effort to bring everything to a state where Git's test suite passes, many more months until the first official Git for Windows 2.x was released, and a couple of patches still await their submission to the respective upstream projects. Yet without MSYS2, the modernization of Git for Windows would simply not have happened.

# Difference between MSYS2 and MINGW: going into details

In order to support Git fully, Git for Windows needs a way to execute the shell scripts that are *still* a core part of Git.
To do so, we use a stripped-down MSYS2 (which in turn is a modified version of Cygwin - read more [here](https://www.msys2.org/wiki/How-does-MSYS2-differ-from-Cygwin/) and [here](https://github.com/msys2/msys2/wiki/How-does-MSYS2-differ-from-Cygwin))).

Now, to understand the packages' names better, we need to understand how MSYS2 packages are compared to MINGW packages.

MSYS2's runtime is `<SDK>\usr\bin\msys-2.0.dll`. It is an implicit dependency of all MSYS2 executables (most of which live in <SDK>\usr\bin). This runtime provides emulation for all POSIX functionality enjoyed e.g. by Linux software. This means that it is *a lot* easier to port Linux projects to MSYS2 than it is to straight Windows. So when porting Bash or OpenSSH, it is much, much easier to port them to an MSYS2 package.

So why don't we just make all packages MSYS2 ones? The POSIX emulation is *slow*. Like, really, really noticeably slow.
So it is much preferred to port Linux software to pure Win32 calls, without going the POSIX emulation route. Of course, this is a ton more work *per project*. That is the reason why this is not done for all packages. You may want to read [this article](https://github.com/git-for-windows/git/blob/HEAD/Documentation/git-for-windows/Windows-vs-Linux-fork%28%29/exec%28%29-semantics.md) for getting an acquaintance of just a part of underlying problems.

Normally, we use Visual C++ compiler on Windows. When building Windows software using GCC, it is necessary to have all the support headers and libraries. They are not provided by Microsoft, but by a separate project called MINGW ("Minimal GNU on Windows"). MSYS2 uses these MINGW libraries (and MINGW GCC compiler) to build pure Win32 packages, and those are called MINGW packages.

That is the difference between `<SDK>\usr\src\MSYS2-packages` and `<SDK>\usr\src\MINGW-packages`. To discern those packages from one another, the MINGW packages all start with the prefix `mingw-w64-`.


# It was too simple
To make things even more complicated, remember that you can run 32-bit executables from a 64-bit process and vice versa (provided that you are on a 64-bit Windows). This is only *partially* supported by MSYS2: while you can mix and match 32-bit/64-bit MINGW processes, the same is *not* true of MSYS2 processes, as they are limited by the "bit-ness" of the MSYS2 runtime.

A 64-bit MSYS2 process always uses the 64-bit MSYS2 runtime, and this runtime needs to be shared between all child MSYS2 processes, to share information required to emulate e.g. POSIX signal handling. That means that you can install only 32-bit MSYS2 packages into a 32-bit MSYS2 setup.

So when you try to upgrade Bash in a 32-bit Git for Windows SDK (which is a slightly modified version of MSYS2, specifically crafted to support Git for Windows' development), it is clear that `pacman -Sy bash` will only upgrade a 32-bit Bash, not a 64-bit one.

However, if you want to install a 32-bit *MINGW* package, you have to be able to discern that from a 64-bit MINGW package.

For example, you could install MINGW cURL in both 32-bit and 64-bit versions. To enable that, there is a further convention that the MINGW packages have not only the prefix `mingw-w64-` but also the infix `i686-` for 32-bit, and `x86_64-` for 64-bit packages. The 32-bit MINGW cURL is offered, therefore, in the `mingw-w64-i686-curl` package, the 64-bit one in `mingw-w64-x86_64-curl`.

To allow for side-by-side installation, the 32-bit MINGW files are installed into the `<SDK>\mingw32` root, the 64-bit ones into `<SDK>\mingw64`. Hence, the 64-bit curl.exe lives in `<SDK>\mingw64\bin\curl.exe`. Git for Windows wants to be as fast as possible, so git.exe is compiled as a MINGW executable.

Therefore, it lives in `<SDK>\mingw64\bin\git.exe` (or in `<SDK>\mingw32\bin\git.exe`), and is bundled in the `mingw-w64-x86_64-git` (or `mingw-w64-i686-git`) package. (That's not the complete truth for documentation and testing packages, but let's concentrate on the main story.)

Pacman (package manager used by MSYS2) has this feature where it regularly adds its own modifications on top of the original source of packages. In Git's case, it adds e.g. the git-wrapper in its various forms, and more. The git-wrapper is pretty specific to the official Git for Windows that is based on MSYS2, though (while we have a task to support both MSys and MSYS2 packages). So it cannot live in git-for-windows/git. It lives in MINGW-packages, in the subdirectory specific to the mingw-w64-git package.
