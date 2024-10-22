# **Please note that this scenario is not officially supported by Git for Windows**

Git for Windows being based on MSYS2, it's possible to install the `mingw-w64-git` package into an existing MSYS2 installation.  That means that if you are already using MSYS2 on your computer, you can use Git for Windows without running the full installer or using the portable version.  There are some caveats for going this way, details below.  Reading the whole guide before starting is recommended.

This guide assumes you are comfortable using the command line and you are willing to completely re-install your MSYS2 if something goes wrong.  You can contact @Elieux for advice in the `git-for-windows/git` Gitter room or through the contact channels for MSYS2.  Please don't file issues with Git for Windows installed this way until you verify them in an official Git for Windows distribution.


#### The steps

Here are the steps to install the 64-bit version of Git for Windows to be run in an MSYS2 terminal (`msys2_shell.cmd`):

 1. Edit `/etc/pacman.conf` and add the Git for Windows package repositories above any others (i.e. just before `[mingw32]` on line #71 as of this writing):

        [git-for-windows]
        Server = https://wingit.blob.core.windows.net/x86-64

        [git-for-windows-mingw32]
        Server = https://wingit.blob.core.windows.net/i686

    (The above is correct.  The second, MINGW-only repository is for the other architecture.)

    This step can be done with the following `sed` command (make sure to do proper backup before trying it):

        sed -i '/^\[mingw32\]/{ s|^|[git-for-windows]\nServer = https://wingit.blob.core.windows.net/x86-64\n\n[git-for-windows-mingw32]\nServer = https://wingit.blob.core.windows.net/i686\n\n|; }' /etc/pacman.conf

    To avoid the future signature related issues, run the following commands first

        rm -r /etc/pacman.d/gnupg/
        pacman-key --init
        pacman-key --populate msys2

 2. Authorize the signing key with:

        curl -L https://raw.githubusercontent.com/git-for-windows/build-extra/HEAD/git-for-windows-keyring/git-for-windows.gpg |
        pacman-key --add - &&
        pacman-key --lsign-key E8325679DFFF09668AD8D7B67115A57376871B1C &&
        pacman-key --lsign-key 3B6D86A1BA7701CD0F23AED888138B9E1A9F3986

 3. Then synchronize with new repositories with

        pacman -Syyuu

    This installs a new `msys2-runtime` and therefore will ask you to terminate all MSYS2 processes.  Save what you need from other open MSYS2 shells and programs, exit them and confirm the Pacman prompt. 
 Double-check Task Manager and kill `pacman.exe` if it's still running after the window is closed.  Start a new MSYS2 terminal.

 4. Then synchronize *again* to install the rest:

        pacman -Suu

    It might happen that some packages are downgraded, this is expected.

 5. And finally install the packages containing Git, its documentation and some extra things:

        pacman -S mingw-w64-x86_64-{git,git-doc-html,git-doc-man} mingw-w64-x86_64-git-extra

Now you can close the current shell and open a MINGW64 shell (`msys2_shell.cmd -mingw64`) to check that everything went well.  Run `git --version` and it should output something like `git version 2.31.0.windows.1` (or newer).


#### Enhancements, modifications, troubleshooting

If you want to run Git from outside the shells, add `C:\msys64\cmd` to your `PATH`, e.g. by using *Edit environment variables for your account* from the Start menu.  It has to come before any other entry pointing inside your MSYS2 installation. If you have installed your MSYS2 into a different directory, correct the path accordingly.

To integrate with the Windows Credential Manager, install the package `mingw-w64-x86_64-git-credential-manager`.

To be able to view the git man pages when invoking help with `git help X` or `git X --help` (in addition to `man git-X`), add the line `export MSYS2_ENV_CONV_EXCL=MANPATH` to your shell configuration, and set the man pages as default help format with `git config --global help.format man` (or append `-m` to the git help invocation).

If you encounter error "*error: wrong number of arguments, should be from 1 to 2*" with `git add -p`, set `add.interactive.useBuiltin` to `true` in Git's configuration.

If you encounter DLL errors (*The code execution cannot proceed because libsomething.dll was not found.*), this is most likely an incompatibility between the DLL versions from Git for Windows and upstream MSYS2.  Usually this is caused by cURL, GnuTLS and OpenSSL.  Replacing the packages with the ones from upstream seems to work best, but no guarantees for what that actually does with Git: `pacman -S mingw64/mingw-w64-x86_64-curl mingw64/mingw-w64-x86_64-gnutls mingw64/mingw-w64-x86_64-openssl`

Git for Windows carries an `msys2-runtime` different from upstream MSYS2, see [issue #284](https://github.com/git-for-windows/git/issues/284) for more details.  You can run into some rare issues with programs other than Git due to this.  It's possible to keep the stock MSYS2 runtime by moving the `msys` repository above the `git-for-windows` repository in `/etc/pacman.conf`.  There are known issues with Git in that case, although not common.

The `mingw-w64-x86_64-git-extra` package modifies the MSYS2 installation heavily (sometimes in ways that are tedious to undo).  The package can be skipped if you want to keep your MSYS2 pristine.  As of this writing, the package:

- brings the `git-for-windows-keyring` package as its dependency, but you can install it manually
- adds the `sdk` command intended for developing Git for Windows
- installs some update helpers (`update-via-pacman`, `git update-git-for-windows`) but you can still use the regular `pacman -Syu`
- installs various other helpers: `blocked-file-util`, `create-shortcut`, `git-askyesno`, `git-credential-helper-selector`, `proxy-lookup`, `WhoUses`, `astextplain`, `notepad`, `vi`, `wordpad`
- changes configuration for Pacman, Bash/Sh, Vim, Nano, Readline, OpenSSH and Git itself (`/etc/gitconfig`, `/etc/gitattributes`)
- changes the MSYS2-to-Windows integration settings in `/etc/nsswitch.conf` and `/etc/fstab` and inherits outside `PATH` by default
- may create a `.bash_profile` in your home directory
- may delete, modify or replace `/msys2.ico`, `/usr/bin/start`, `/mingw/*-w64-mingw32/include/pthread_unistd.h`, `/ssl`
- deletes, modifies and replaces some files in `/mingw??/bin/` and `/mingw??/libexec/git-core/`
- changes the terminal window title, changes the shell prompt
- enables a GUI SSH password prompt
- automatically wraps some interactive non-msys programs using `winpty`: Node, Python, PHP, PostgreSQL
- adds support for ARM64
