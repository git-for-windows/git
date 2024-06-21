## Which versions of Windows are supported?

Git for Windows currently works with all recent, commercially released x86 32 bit and 64 bit versions of Windows, starting with Windows 7 SP1.

***
NOTE: Git for Windows version [2.37.1](https://github.com/git-for-windows/git/releases/tag/v2.37.1.windows.1) was the last version supporting Windows Vista and Server 2008.
***

***
NOTE: Git for Windows version [2.10.0](https://github.com/git-for-windows/git/releases/tag/v2.10.0.windows.1) was the last version supporting Windows XP and Server 2003.
***

Windows XP and Windows Server 2003 are long past their end of life, hence support for these versions was dropped from Git for Windows 2.10.1 and later. This limitation is inherited by Git's use of MSYS2, which in turn inherited it from [Cygwin](https://cygwin.com/ml/cygwin/2015-08/msg00446.html).

More information here: [Git for Windows' prerequisites](https://gitforwindows.org/requirements.html).

## Is there an MSI for installation via Group Policy?

There is currently no MSI package; there is an exe installer and a portable package. You are welcome to contribute a Pull Request that packages a new installer.

~There is an existing Pull Request in development and looking for testers, please try out https://github.com/robmen/gitsetup/issues/1 and give feedback.~ It's closed by now. Future development is happening [here](https://github.com/git-for-windows/build-extra/tree/HEAD/msi/).

In the meantime you could try:

 * using the portable package
 * deploying the exe installer with System Center Configuration Manager
 * deploying the portable package via a script
 * deploying the <a href="https://chocolatey.org/packages/git">Chocolatey package</a> via Puppet (or something similar)

## Should I upgrade to a newer Git for Windows version?

In general, yes: it is a good idea to stay up-to-date.

If you have a version older than 2.45.1, it is *highly* advisable to upgrade. A couple of Git versions came with important fixes to security-relevant vulnerabilities: 2.45.1, 2.40.1, 2.39.2, 2.39.1, 2.38.1, 2.37.1, 2.36.0, 2.35.3, 2.35.3, 2.30.2, 2.30.0(2), 2.29.2(3), 2.29.2(2), 2.26.1, 2.24.1(2), 2.17.1(2), 2.14.1, 2.7.4, 2.7.0, 2.6.1, 2.5.2, 1.9.5-preview20150319, and 1.9.5-preview20141217.

## How do I update *Git for Windows* upon new releases?

This depends on how you installed *Git for Windows*. The bundle installation requires you to download and install the new version. Any customizations you made will be kept as long as you did so in the appropriate configuration folders.

*Git for Windows* comes with a tool to check for updates and offer to install them. Whether or not you enabled auto-updates during installation, you can manually run `git update-git-for-windows`. (For help, run `git update-git-for-windows -h`.)

For advanced users working with the *Git for Windows* SDK `pacman` is available as a package manager. See [Package management](Package-management.md)

## How do I check the release hash and release notes

For hashes see https://github.com/git-for-windows/git/releases

For (G4W) release notes see the [build-extra repo](https://github.com/git-for-windows/build-extra/blob/HEAD/ReleaseNotes.md), or the top level of your installation (e.g. [C:\Program Files\Git\ReleaseNotes.html](https://www.example.com/Program%20Files/Git/ReleaseNotes.html) - via manual browsing).

For extracting 7z/zip archives see [page](https://github.com/git-for-windows/git/blob/HEAD/Documentation/git-for-windows/Zip-Archives---extracting-the-released-archives.md).

## Are there 'Nightly' builds of the latest and greatest

The Git for Windows builds are not quite that frequent, but there are Snapshot builds listed at https://wingit.blob.core.windows.net/files/index.html

These often 'fix' (or attempt fixes) recent issues before a new formal release. Check their commit notes and links to issues to see if your problem is included.

## What is the release cadence of Git for Windows?

Official Git for Windows versions mainly follow Git's release cycle: every 12 weeks or so, a new major Git version is released (see details here: https://tinyurl.com/gitCal). Typically, Git for Windows will follow suit within a day.

Other than that, Git for Windows follows the *newest* Git version's maintenance releases (read: after Git for Windows v2.15.0 was released, no new Git for Windows v2.14.x version would be released). Indicators for imminent maintenance releases of Git are:

- The Git maintainer sometimes talks about this in the preamble of the ["What's cooking in git.git" mails](https://public-inbox.org/git/?q=s%3A%22what%27s+cooking%22+AND+f%3Agitster%40pobox.com).
- The [`maint` in git.git](https://github.com/git/git/commits/maint) accumulates critical patches.

Finally, Git for Windows is sometimes released in "out-of-band" versions, when critical fixes specific to Git for Windows necessitate it. These out-of-band versions are indicated by appending a `(2)` to the latest release (or `(3)`, `(4)`, etc). Examples for such out-of-band versions include: [Git for Windows v2.15.1(2)](https://github.com/git-for-windows/git/releases/tag/v2.15.1.windows.2) and [Git for Windows v2.16.1(4)](https://github.com/git-for-windows/git/releases/tag/v2.16.1.windows.4).

## What is the relationship between *Git for Windows* and *msysGit*?

*Git for Windows* used to be developed using the development environment called "msysGit", but roughly coinciding with Git 2.1, msysGit was superseded by a new development environment: the [Git for Windows SDK](https://github.com/git-for-windows/build-extra/releases). See [here](https://github.com/git-for-windows/git/blob/HEAD/Documentation/git-for-windows/Updating-your-SDK.md) to get a copy.

## Some native console programs don't work when run from Git Bash. How to fix it?

*Git for Windows* defaults to using [mintty](http://mintty.github.io/) terminal. Compared to default Windows console host, it provides normal multi-line cut&paste, working resizing, defaults to unicode font and avoids some bugs in the default console host. However it does not present itself as console to native applications (those *not* built with MSys or Cygwin), so in these applications:

 * Non-ASCII output may be corrupted due to mismatch in character sets (MSYS2 and Cygwin use UTF-8 while Windows will fall back to the legacy DOS codepages in this case).
 * Interactive and full-screen applications won't work at all.

There are several methods for working around these problems:

 * Run programs that have problems using the [`winpty`](https://github.com/rprichard/winpty) utility. This allows you to keep using the nicer mintty terminal, but can become unwieldy if you need the workaround for many programs.
 * Modify the shortcut for Git Bash to run `bash` directly without `mintty` so it uses the default console host and configure it for "Quick Edit", reasonable size and scroll-back and suitable unicode font. You'll still have to live with the other quirks of console host.
 * Install and use [ConEmu](https://conemu.github.io/).

## I get errors trying to check out files with long path names.
Windows file paths are by default limited to 260 characters. Some repositories may have committed files which contain paths longer than the limit. By default, *Git for Windows* does not support long paths, and will print errors when trying to perform any operation on a long file name. Set the configuration property `core.longpaths` to true to allow certain Git operations to properly handle these files. See [this wiki page](https://github.com/git-for-windows/git/blob/HEAD/Documentation/git-for-windows/Git-cannot-create-a-file-or-directory-with-a-long-path.md) for more information.

## The installed files are duplicated and look massive - Why?
All the apparent copies are simply hard links - see Issue  1997 [Use symbolic links for libexec](https://github.com/git-for-windows/git/issues/1997) for more details.

## Excel file modifications not always noticed.
Excel (and some other apps) do not update the *modified time* of its files which is used by Git to quickly detect changes. Rather Excel used the *change time* field. Git will notice the modifications if a `git status` is performed. See [issue 1000](https://github.com/git-for-windows/git/issues/1000#issuecomment-301611003) if you need more background.

## How do I access a repository hosted on a Microsoft Team Foundation Server inside a Windows domain?

The Microsoft Team Foundation Server is capable of hosting git repositories. If the server is a member of a windows domain, and your user account is in that domain, you can use domain authentication to identify yourself to the server, and can thus access git repositories without having to enter any credentials. For this to work, the server has to be configured to use domain authentication, _not_ NTLM authentication. The repository URL can be obtained from the TFS web interface, it may look like http://server.example.com:8080/tfs/TWA/TeamDev/_git/reponame.

In order to instruct git to use domain authentication, prefix the server name with `:@`, like so: `http://:@server.example.com:8080/tfs/TWA/TeamDev/_git/reponame`. This is a special case of the usual `username:password@server` syntax, where both the `username` and `password` fields are empty. This causes git to look up and use your domain credentials.

See also: [Clone an existing Git repo - Azure Repos](https://docs.microsoft.com/en-us/azure/devops/repos/git/clone)

## I get "Permission denied (publickey)." when using git pull or git clone

### DSA keys
Some DSA keys are not considered secure anymore by OpenSSH 7. Adding "PubkeyAcceptedKeyTypes ssh-dss" to ~/.ssh/config helps.

### Running without Git Bash
If you choose to use OpenSSH that comes with git during installation (default option) and wish to work from Windows' Command Prompt (cmd) or PowerShell instead of Git Bash you should make sure:

1. You're calling the correct executables.  
Windows comes with a different OpenSSH distribution and by default you might be calling it while git commands use its own.
Set the environment PATH to use the git installation ssh commands first, those are usually at `C:\Program Files\Git\usr\bin`.  
In CMD: `set PATH=C:\Program Files\Git\usr\bin;%PATH%`  
In PowerShell: `$Env:PATH = "C:\Program Files\Git\usr\bin;$Env:PATH"`  
This will make sure, for example, that you add the private keys to the correct _ssh-agent service_ when calling `ssh-add`.
Otherwise you might be asked to introduce the passphrase on each git command call that connects to a remote repository.

2. Git's ssh is able to connect to the ssh-agent.  
In order to connect to the _ssh-agent service_, git's ssh needs to know _how_. This is accomplished by two environment variables called `SSH_AGENT_PID` and `SSH_AUTH_SOCK`.
Luckily, there is a convenient script `start-ssh-agent.cmd` included with the git installation you can call to set this up. The values are dynamic and only valid after calling the script. Forgetting to set them will fail silently, so be sure to call the script each time you open a new command prompt.

## Switch locale for git-bash launcher (of Git for Windows SDK)

Some developers want to start git-bash (of Git for Windows SDK) with a different language.
To achieve this in windows following command could be placed inside the launcher:
`C:\Windows\System32\cmd.exe /c "set LANG=en_GB && start C:\git-sdk-64\git-bash.exe && exit"`
replace `en_GB` with your preferred locale.

## I have errors while building the solution in Visual studio
(i.e. `fatal error C1083: Cannot open include file: 'openssl/ssl.h': No such file or directory`)

Try to open the console and invoke `git\compat\vcbuild\vcpkg_install.bat`. Make sure that it has completed successfully, then clean and rebuild the solution.

Ensure also that you are using proper Build Tools (v140). It's going by default in VS 2015. If you are using VS 2017, you need to install them manually. Do not upgrade the project to v141 - it can't be done automatically.

## Licenses

Reproduced from [Git for Windows' release notes](https://github.com/git-for-windows/build-extra/blob/HEAD/ReleaseNotes.md#licenses):

Git is an Open Source project covered by the GNU General Public License version 2 (some parts of it are under different licenses, compatible with the GPLv2). It was originally written by Linus Torvalds with help of a group of hackers around the net.

Git for Windows also contains Embedded CAcert Root Certificates. For more information please go to https://www.cacert.org/policy/RootDistributionLicense.php.

This package contains software from a number of other projects including Bash, zlib, curl, tcl/tk, perl, MSYS2 and a number of libraries and utilities from the GNU project, licensed under the GNU General Public License. Likewise, it contains Perl which is dual licensed under the GNU General Public License and the Artistic License.
