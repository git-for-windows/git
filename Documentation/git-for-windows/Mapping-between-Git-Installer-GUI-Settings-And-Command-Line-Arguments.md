The following table maps Git GUI Installation settings to corresponding Git Command Line Installation arguments:

| Screenshot | Available Arguments<br/>(in sequence) | Example Arguments |
| - | - | - |
| ![Git 01](https://github.com/git-for-windows/git/assets/9283914/ca0ae5e8-77ed-4a46-ac4b-b8b2f29e1504) | <br/>`Components=`<br/><br/><ul><li>`icons`</li><li>`ext\reg\shellhere`</li><li>`gitlfs`</li><li>`assoc`</li><li>`assoc_sh`</li><li>`windowsterminal`</li><li>`scalar`</li></ul><br/> | `Components=gitlfs,scalar` |
| ![Git 02](https://github.com/git-for-windows/git/assets/9283914/44547320-012f-4a14-9c20-657a67a45813) | <br/>`EditorOption=`<br/><br/><ul><li>`Nano`</li><li>`VIM`</li><li>`Notepad++`</li><li>`VisualStudioCode`</li><li>`VisualStudioCodeInsiders`</li><li>`SublimeText`</li><li>`Atom`</li><li>`VSCodium`</li><li>`Notepad`</li><li>`Wordpad`</li><li>`CustomEditor`</li></ul><br/> | `EditorOption=VisualStudioCode` |
| ![Git 03](https://github.com/git-for-windows/git/assets/9283914/748ecec9-d460-4bb2-bcb7-63cf9f37c882) | <br/>`DefaultBranchOption=`<br/><br/>*{branch name}*<br/> | `DefaultBranchOption=main` |
| ![Git 04](https://github.com/git-for-windows/git/assets/9283914/7296a36f-416d-4ab6-9913-c83cfa6f2a06) | <br/>`PathOption=`<br/><br/><ul><li>`BashOnly`</li><li>`Cmd`</li><li>`CmdTools`</li></ul><br/> | `PathOption=Cmd` |
| ![Git 05](https://github.com/git-for-windows/git/assets/9283914/ed274bde-ab0e-49d5-820e-3bd6fae6f873) | <br/>`SSHOption=`<br/><br/><ul><li>`OpenSSH`</li><li>`ExternalOpenSSH`</li><li>`Plink`</li></ul><br/> | `SSHOption=ExternalOpenSSH` |
| ![Git 06](https://github.com/git-for-windows/git/assets/9283914/0b5711f9-d1ad-4c64-aae6-0629a0bda235) | <br/>`CurlOption=`<br/><br/><ul><li>`OpenSSL`</li><li>`WinSSL`</li></ul><br/> | `CurlOption=WinSSL` |
| ![Git 07](https://github.com/git-for-windows/git/assets/9283914/d8d5b885-1c26-439b-9dab-ea855f0387ca) | <br/>`CRLFOption=`<br/><br/><ul><li>`CRLFAlways`</li><li>`LFOnly`</li><li>`CRLFCommitAsIs`</li></ul><br/> | `CRLFOption=CRLFAlways` |
| ![Git 08](https://github.com/git-for-windows/git/assets/9283914/bb676934-eafb-4316-bbfd-877e6eb2b659) | <br/>`BashTerminalOption=`<br/><br/><ul><li>`MinTTY`</li><li>`ConHost`</li></ul><br/> | `BashTerminalOption=ConHost` |
| ![Git 09](https://github.com/git-for-windows/git/assets/9283914/b802d077-650a-4f41-a87f-23e2ff412c66) | <br/>`GitPullBehaviorOption=`<br/><br/><ul><li>`Merge`</li><li>`Rebase`</li><li>`FFOnly`</li></ul><br/> | `GitPullBehaviorOption=Merge` |
| ![Git 10](https://github.com/git-for-windows/git/assets/9283914/c556c3e1-f9bc-4462-b65e-3d87ff9aac1b) | <br/>`UseCredentialManager=`<br/><br/><ul><li>`Enabled`</li><li>`Disabled`</li><li>`Core`</li></ul><br/> | `UseCredentialManager=Enabled` |
| ![Git 11](https://github.com/git-for-windows/git/assets/9283914/79864759-a5fa-499b-9a6b-94443b6a9253) | <br/>`PerformanceTweaksFSCache=`<br/><br/><ul><li>`Enabled`</li><li>`Disabled`</li></ul><br/><br/>`EnableSymlinks=`<br/><br/><ul><li>`Auto`</li><li>`Enabled`</li><li>`Disabled`</li></ul><br/> | `PerformanceTweaksFSCache=Enabled`<br/><br/>`EnableSymlinks=Disabled` |
| ![Git 12](https://github.com/git-for-windows/git/assets/9283914/7bcdf2cb-2b6e-4a50-8f52-8b22e7df2a3c) | <br/>`EnablePseudoConsoleSupport=`<br/><br/><ul><li>`Auto`</li><li>`Enabled`</li><li>`Disabled`</li></ul><br/><br/>`EnableFSMonitor=`<br/><br/><ul><li>`Auto`</li><li>`Enabled`</li><li>`Disabled`</li></ul><br/> | `EnablePseudoConsoleSupport=Disabled`<br/><br/>`EnableFSMonitor=Enabled` |

<br/>

For a comprehensive list of Git for Windows Installer command line arguments [see here](https://github.com/git-for-windows/git/blob/HEAD/Documentation/git-for-windows/Silent-or-Unattended-Installation.md).