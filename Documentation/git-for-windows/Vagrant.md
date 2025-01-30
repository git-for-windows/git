# Introduction

[Vagrant](https://www.vagrantup.com/) is a modern way to define the requirements of a project. In the case of [Git for Windows](https://gitforwindows.org/), it allows us to set up a minimal virtual Linux machine that makes it easy to run the same revision of Git on Windows as well as on Linux without much effort.

**Note**: while Vagrant makes things easy, quite a bit of bandwidth is required for the setup (roughly a gigabyte will be downloaded in total).

# How to install

1. Download and install [VirtualBox](https://www.virtualbox.org)
2. Download and install [Vagrant](https://www.vagrantup.com/)
3. Install the [Git SDK](https://gitforwindows.org/#download-sdk)
4. Run `vagrant up` in the `/usr/src/git` directory
5. Run `vagrant ssh`

Note that the prompt shows that the current directory in the ssh session inside the virtual machine is `/vagrant/` and that the files in that directory are suspiciously identical to the `/usr/src/git/` directory in your Git SDK installation. This is not by accident. In fact, the `/vagrant/` directory inside the virtual machine *is* the `/usr/src/git` directory of the hosting Git SDK. Note: This implies that the file names are case-insensitive, still, even if running inside a Linux VM.

To compile and install Git, you will have to run `make clean` first because *Git* will have built *Windows* binaries in the same directory (when we will need *Linux* binaries inside the virtual machine started by Vagrant). After calling `make install` and `export PATH=$HOME/bin:$PATH` you will be able to run the Git version built from the source files in `/vagrant/git/`.

# Alternative to the Git SDK way

If you cannot download and install the Git SDK for some reason or other, you could also clone [the Git source code](https://github.com/git-for-windows/git) using [Git for Windows](https://gitforwindows.org/) instead, but make sure that Unix line endings are used: `git clone -c core.autocrlf=false https://github.com/git-for-windows/git vagrant-git`. If Git for Windows does not even work for you, you could also download the source code [as a `.zip`](https://github.com/git-for-windows/git/archive/HEAD.zip) and unpack it.

After that, continue with the `vagrant up` step above.

# Why?

Git was born on Linux. Over the years, it has become more and more platform-independent, but still support for Linux outshines support for every other platform, including Windows. Therefore it is preferable under certain circumstances to run Git inside Linux in a virtual machine, for example

* for finding out whether a bug is Windows-specific or not
* for performance (Linux' filesystem and memory management is better than Windows')
* for verifying that Git for Windows' source code compiles correctly with Linux, too
* etc

# Known problems

* The Linux version of Git was designed to run on Linux file systems. Vagrant exposes Git for Windows' source code directory as `/vagrant/` but of course it cannot make up for the Windows file system's lack of support for Unix-style file permissions nor case-sensitive file names. As a consequence, quite a few of the unit tests fail when run in-place.
* Git cannot be built with Perl's MakeMaker using Vagrant: it tries to create files with double colons in their name (e.g. `Git::I18N.3pm`) which is not allowed on Windows file systems.
* If the Git SDK was started inside a VirtualBox (e.g. to be able to test Git for Windows even on a Linux/MacOSX laptop), VirtualBox will not be able to start the virtual machine (with an error message *VERR_VMX_NO_VMX*). This is a [known problem with VirtualBox](https://www.virtualbox.org/ticket/4032).
* `git svn` is *very* slow. Actually, `git svn` is *not* very slow. But it is if you run it inside a directory under `/vagrant/`. The reason is that Linux' file system performance cannot come to full play when accessing `/vagrant/` -- which really is the host's directory managed by Windows. If you want to benefit from Linux' performance characteristics, you will have to perform the time-critical operations either in `/home/vagrant/` inside the virtual machine, or inside a `tmpfs`.

# Tips & Tricks

## Use `tmpfs`

Linux offers a plethora of file systems optimized for different use cases. One of them is [`tmpfs`](https://www.kernel.org/doc/Documentation/filesystems/tmpfs.txt), a RAM-based file system which keeps all files in virtual memory and is therefore very fast (but does not persist any of the data to disk).

Using such a file system can speed up Git operations quite substantially, in particular when performing disk-intensive operations, such as `git filter-branch`, `git gc` or `git svn`. It is very easy to create a `tmpfs`-backed file system:

```bash
mkdir -p $HOME/tmp
sudo mount -t tmpfs -o size=10G tmpfs $HOME/tmp
cd $HOME/tmp
```

You will want to clone projects into that `$HOME/tmp/` directory so that the I/O intensive operations benefit fully from using Vagrant.
