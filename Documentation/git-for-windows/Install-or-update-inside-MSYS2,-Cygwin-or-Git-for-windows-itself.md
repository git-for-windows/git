## It's different from [Install inside MSYS2 proper](https://github.com/git-for-windows/git/blob/HEAD/Documentation/git-for-windows/Install-inside-MSYS2-proper.md)

Instead of configuring and installing Git for Windows using `pacman`, this guide introduces another approach to properly install or update Git for Windows inside MSYS2, Cygwin or even Git for Windows itself. The only thing you need is a `bash` interpreter on Windows and a `curl`.

## How to

With the script `getgit` introduced by [git-for-windows/build-extra#261](https://github.com/git-for-windows/build-extra/pull/261), you can download and run the script or just run `curl https://raw.githubusercontent.com/git-for-windows/build-extra/HEAD/git-extra/getgit | bash`.

The script would gather information about the current system ( *Msys / Cygwin* ), bitness, version of an already existed Git for Windows, version of the latest released Git for Windows... and then forge a proper download URL targeting a proper version of the `PortableGit` installer.
With all the information prepared, the script would download the installer and unpack it to `/tmp`. After that, all the necessary files will be copied to a proper place inside the ( *Msys's / Cygwin's* ) file system without touching any existing **non-git-for-windows-exclusive** files.

Additionally, the `getgit` script will produce two scripts `ctxmenu.bat` and `rm-ctxmenu.bat` which could add and remove `Git GUI Here` and `Git Bash Here` context menu items. But note that, these two context menu items although seem alike with the ones of Git for Windows, they are actually **different**! They just provided similar outcomes and functionality yet still not the same things.

## Known issue
* On Cygwin, the final `Git Bash Here` won't have a nice prompt that has the current branch of a git repo folder shown. To fix this, you need to remove the `PS1=...` line in `/etc/bash.bashrc`.
* If some `Permission denied` errors raised, please run your `bash` as Administrator to make sure this script runs with write privilege granted. And if something similar happens against `ctxmenu.bat` script or `rm-ctxmenu.bat`, run it as Administrator as well.
