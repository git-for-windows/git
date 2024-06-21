## Using Command-Line Options

It is possible to install Git for Windows silently using the following flags when called from a command-line prompt:

```
Git-<version>-<bitness>.exe /VERYSILENT /NORESTART /NOCANCEL /SP- /CLOSEAPPLICATIONS /RESTARTAPPLICATIONS /COMPONENTS="icons,ext\reg\shellhere,assoc,assoc_sh"
```

## Using Installer Options From a File

You can also load install parameters from a file with `/LOADINF="filename"`, and you can record parameters to a file using `/SAVEINF="filename"`.

Example for saving selected options to a file during an interactive run started from the command-line:

```
Git-<version>-<bitness>.exe /SAVEINF=git_options.ini
```

Example reusing that previously created file to silently install Git in unattended mode:

```
Git-<version>-<bitness>.exe /VERYSILENT /NORESTART /NOCANCEL /LOADINF=git_options.ini
```

An example of a parameter file is:

```ini
[Setup]
Lang=default
Dir=C:\Program Files\Git
Group=Git
NoIcons=0
SetupType=default
Components=gitlfs,assoc,assoc_sh,windowsterminal
Tasks=
EditorOption=VIM
CustomEditorPath=
DefaultBranchOption=main
PathOption=Cmd
SSHOption=OpenSSH
TortoiseOption=false
CURLOption=WinSSL
CRLFOption=CRLFCommitAsIs
BashTerminalOption=MinTTY
GitPullBehaviorOption=Merge
UseCredentialManager=Enabled
PerformanceTweaksFSCache=Enabled
EnableSymlinks=Disabled
EnablePseudoConsoleSupport=Disabled
EnableFSMonitor=Disabled
```

## Installer Options

You can find all of the possible flags to use by calling the installer with the `/?` flag (for the options supported by [InnoSetup](http://www.jrsoftware.org/ishelp/index.php?topic=setupcmdline) out of the box), and by inspecting the [`install.iss` file](https://github.com/git-for-windows/build-extra/blob/HEAD/installer/install.iss) (for custom options added only to Git for Windows' installer).

### Custom Installer Options

A list of installer custom options as of Git for Windows v2.42.0.windows.2 is below:

| Key | &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Values&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; | Default | &nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;Remark&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp; |
| - | - | - | - |
| `EditorOption` | `Nano`, `VIM`, `Notepad++`, `VisualStudioCode`, `VisualStudioCodeInsiders`, `SublimeText`, `Atom`, `VSCodium`, `Notepad`, `Wordpad`, `CustomEditor` | `VIM` | |
| `CustomEditorPath` | | | Path and options for custom text editor (only in combination with `EditorOption=CustomEditor`) |
| `DefaultBranchOption` | | | Default branch name |
| `PathOption` | `BashOnly`, `Cmd`, `CmdTools` | `Cmd` | |
| `SSHOption` | `OpenSSH`, `ExternalOpenSSH`, `Plink` | `OpenSSH` | |
| `TortoiseOption` | `true`, `false` | `false` | Only in combination with `SSHOption=Plink` |
| `CurlOption` | `OpenSSL`, `WinSSL` | `OpenSSL` | |
| `CRLFOption` | `LFOnly`, `CRLFAlways`, `CRLFCommitAsIs` | `CRLFAlways` | |
| `BashTerminalOption` | `MinTTY`, `ConHost` | `MinTTY` | |
| `GitPullBehaviorOption` | `Merge`, `Rebase`, `FFOnly` | `Merge` | |
| `UseCredentialManager` | `Enabled`, `Disabled`, `Core` | `Enabled` | `Core` does exactly the same as `Enabled` and exists for historical reasons |
| `PerformanceTweaksFSCache` | `Enabled`, `Disabled` | `Enabled` | |
| `EnableSymlinks` | `Auto`, `Enabled`, `Disabled` | `Auto` | |
| `AddmandatoryASLRsecurityexceptions` | `Auto`, `Enabled`, `Disabled` | `Auto` | |
| `EnableBuiltinDifftool` | `Auto`, `Enabled`, `Disabled` | `Auto` | |
| `EnableBuiltinRebase` | `Auto`, `Enabled`, `Disabled` | `Auto` | |
| `EnableBuiltinStash` | `Auto`, `Enabled`, `Disabled` | `Auto` | |
| `EnableBuiltinInteractiveAdd` | `Auto`, `Enabled`, `Disabled` | `Auto` | |
| `EnablePseudoConsoleSupport` | `Auto`, `Enabled`, `Disabled` | `Auto` | |
| `EnableFSMonitor` | `Auto`, `Enabled`, `Disabled` | `Auto` | |