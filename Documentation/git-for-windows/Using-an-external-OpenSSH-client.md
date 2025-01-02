## OpenSSH on Windows 10

Modern versions of Windows and Windows Server come with [OpenSSH](https://www.openssh.com/) installed, and available on the system PATH. By default, the OpenSSH binaries are located in `C:\WINDOWS\System32\OpenSSH\`.

The [`ssh-agent`](https://man.openbsd.org/ssh-agent.1) is also implemented as a [Windows service](https://docs.microsoft.com/en-us/windows/win32/services/services), allowing it to be run automatically, during boot or login, and so on.

> **NOTE:** By default the `ssh-agent` service is **disabled**, and if its use required — to manage your private keys, for instance — it will need to be explicitly enabled, and then started (or configured to start as necessary).

## Configuring Git For Windows to use an external client

> **NOTE:** This feature is still **_experimental_**.

As per [this discussion](https://github.com/git-for-windows/git/discussions/3451#discussioncomment-1424427) on the issue, during the installation of Git For Windows there is an option to utilise either the built-in [OpenSSH](https://www.openssh.com/) client, or an external client. Choosing to use an external client will skip inclusion of the bundled OpenSSH binaries in the Git For Windows installation.

Post-installation, or if using the portable version, to make Git For Windows utilise an external OpenSSH client, remove the bundled OpenSSH binaries. They are typically located in `/usr/bin/`, relative to the installation directory or root folder of the portable version. The bundled OpenSSH implementation is represented by the binaries listed below. On Windows they will have the `.exe` extension.

**NOTE:** The Windows implementation of OpenSSH does _not_ include replacements for all of these. At time of writing, it is missing those indicated below.

* `scp`
* `sftp`
* `ssh`
* `sshd` [not implemented in the built-in Windows OpenSSH]
* All binaries starting with `ssh-`:
    * `ssh-add`
    * `ssh-agent`
    * `ssh-copy-id` [not implemented in the built-in Windows OpenSSH]
    * `ssh-keygen`
    * `ssh-keyscan`
    * `ssh-pageant` [not implemented in the built-in Windows OpenSSH]
