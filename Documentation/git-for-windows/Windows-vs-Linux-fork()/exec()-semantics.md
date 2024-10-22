# TL;DR

Traditionally, you needed to make 2 separate calls to create new process in Linux: [`fork()`](http://man7.org/linux/man-pages/man2/fork.2.html) and then [`exec()`](http://man7.org/linux/man-pages/man3/exec.3.html). Windows has a different way, using a single call ([`CreateProcess()`](https://msdn.microsoft.com/en-us/library/windows/desktop/ms682425(v=vs.85).aspx)), and doesn't support `fork()`. Software originally written for Linux has to use an emulated (way slower and somewhat imperfect) `fork()` to run on Windows. If this software creates a lot of processes during its normal workload, it will then run noticeably slower on Windows than on Linux.

Note that there's a newer, single call method on Linux ([`posix_spawn()`](http://man7.org/linux/man-pages/man3/posix_spawn.3.html)) that should have better performance when ported to Windows.

## Before start

All information about Linux is a brief retelling of Robert Love's book: [Linux Kernel Development](https://doc.lagout.org/operating%20system%20/linux/Linux%20Kernel%20Development%2C%203rd%20Edition.pdf).
I definitely recommend you to read this book by yourself, especially if this article wasn't enough for you. I will oversimplify, otherwise I'd need to put the whole book here.

`fork()` on Windows is emulated by *MSYS*, while there is no `fork()` in *MINGW*. That is partially what makes [MSYS much slower than MINGW](https://github.com/git-for-windows/git/blob/HEAD/Documentation/git-for-windows/The-difference-between-MINGW-and-MSYS2.md).

# Copy-on-write

COW is a general mechanism that allows avoiding copying while we do not need it. It's easier to explain this awesome mechanism with an example from life. Imagine you have a great paper book, and your friends want to read it with you. Moreover, all of you want to make notes while reading. The naive solution is to make a full copy of the book for each reader, and then read each copy and make notes. But COW offers you to read the same book, and make the notes in your own notebook: you can copy little pieces if you want, but you don't need to make a full copy. So, every reader owns the notebook, but you have only 1 book for all: it is enough. It allows to save time dramatically.

# Linux internals: fork(), exec()

`fork()` creates new process as an almost identical copy of an existing one. `exec()` loads another executable into the new process and starts to execute it instead of the copied executable. In the end, the original process continues as before and there's a new, different process running.

Since this scenario is so common on Linux, the system is optimized to execute `fork()` very quickly. In theory, it makes a copy of the process, but there is almost no actual copying yet thanks to COW. Calling `exec()` then throws out all this would-be-copied stuff and no time is wasted.

# Windows internals: why fork() does not work as native

Windows doesn't officially have a good substitute of neither `fork()` nor `exec()`, so some smart people made a replacement that works pretty well, but can't be optimized well because it doesn't work at the system level. The replacement works like this: By calling `fork()`, the original process creates a process from the same executable, but it's like brand new at this point, as if it had no connection to the original one. Then the new process realizes it is supposed to be a fork and copies the entire address accessible memory space of the original process. It tries as best as it can to open the original file descriptors and sockets. It reinstates the current working directory (even if it does not exist anymore) and more. It spends a whole lot of time making sure the fork is as perfect as it can be [and may even fail completely when it can't due to DLL preloading](https://github.com/git-for-windows/git/blob/HEAD/Documentation/git-for-windows/32-bit-issues.md). Then the new process calls `exec()`, throws out all this and finally creates a new process with the correct executable.
