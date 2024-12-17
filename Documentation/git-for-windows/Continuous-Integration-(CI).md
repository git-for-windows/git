# _Attention !_ #
_As of 2016-09-08 the CI services mentioned below are no longer active. Dscho is currently working on CI based on Visual Studio Team Services. No ETA yet, though..._

***

## Foreword

We have a [public Jenkins CI server](https://dscho.cloudapp.net/) running on Windows Server 2012 R2 in [Microsoft Azure](http://azure.microsoft.com/). Anonymous users are able to see the job configurations, but not to edit them.

## Git SDK installer builds

The job that builds snapshots of the latest SDK installer is [here](https://dscho.cloudapp.net/job/gfw-msys2-build-sdk-installer/). Use this installer *only* if you depend on the latest fixes in the installer itself. Otherwise please wait for the [latest release](https://github.com/git-for-windows/build-extra/releases/latest).

If you just want to update your existing SDK installation, you can do so by running `pacman -Syu` from Git Bash.

## Git user installer builds

The job that builds snapshots of the latest user is [here](https://dscho.cloudapp.net/job/gfw-msys2-build-user-installer/). Use this installer *only* if you need to verify that any bugs are fixed. Do *not* use the snapshot for production use but please wait for the [latest release](https://github.com/git-for-windows/git/releases/latest) instead.
