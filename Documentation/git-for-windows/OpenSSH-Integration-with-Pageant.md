For some users, it may be desirable to tie OpenSSH in to the PuTTY authentication agent (Pageant) using [ssh\-pageant](https://github.com/cuviper/ssh-pageant). This is a drop-in replacement for _ssh\-agent_, which simply builds a connection between OpenSSH and Pageant for key-based authentication. The tool makes it easy to leverage OpenSSH for remote repository access, which tends to be the most reliable choice within the specific context of Git for Windows (Git), without the need to run multiple agents which don't interoperate.

This functionality became available with the release of Git 2.8.2.

## Starting ssh\-pageant Manually from Git Bash

If you always use Git from within Git Bash, then the most straightforward approach is to have it launch _ssh\-pageant_ on your behalf. Simply create/edit your _$HOME/.bash_profile_ (or _$HOME/.profile_, if you prefer), and add the following.

    # ssh-pageant allows use of the PuTTY authentication agent (Pageant)
    SSH_PAGEANT="$(command -v ssh-pageant)"
    if [ -x "$SSH_PAGEANT" ]; then
       eval $("$SSH_PAGEANT" -qra "${SSH_AUTH_SOCK:-${TEMP:-/tmp}/.ssh-pageant-$USERNAME}")
    fi
    unset SSH_PAGEANT

The `-qra "${TEMP:-/tmp}/.ssh-pageant"` construct is equivalent to the `-q -r -a filename` options. In this context it means:

* **\-a filename** \-\- Bind to a specific socket file (creating it if necessary)
* **\-r** \-\- Allow reuse of an existing socket file (exit without error if an existing _ssh\-pageant/ssh\-agent_ process is using it)
* **\-q** \-\- Quiet mode (don't echo the PID, if starting a new ssh\-pageant process)

By specifying the socket name (defaulting to `$SSH_AUTH_SOCK`, if set) along with the reuse option, we ensure that only a single running copy of ssh\-pageant (per user) is required. Otherwise a separate incarnation would be launched every time Git Bash is invoked.

### Verify ssh\-pageant Functionality

Now start a new Git Bash session, or source the profile edited just above, and run the `ssh-add -l` command. If all is well, and Pageant is running (with one or more keys loaded), you should see something similar to the following.

    $ ssh-add -l
    4096 SHA256:XjN/glikgdBoBclg4EaN8sJ/ibrxTq7zVydpkUwANzk Heinz Doofenshmirtz (RSA)

## Starting ssh\-pageant Automatically at Logon

If you use Git from Git CMD, or directly from the Windows command prompt, then you'll probably want to ensure that _ssh\-pageant_ is launched automatically at logon time. The _start\-ssh\-pageant.cmd_ script is provided for this purpose, which resides in the **cmd** subdirectory of your Git installation.

### Setting the Environment

Unlike the Git Bash case above, this scenario **requires** the _SSH\_AUTH\_SOCK_ environment variable to be set before running the script... otherwise it will simply exit without performing any action. This is normally configured as a persistent **USER** variable, with the value specifying the desired socket file path in Unix/MSYS2 format.

NOTE: Since there can only be a single global variable of a given name, this approach may or may not cause conflicts if you have multiple environments which utilize the SSH_AUTH_SOCK setting. Running Git alongside of Cygwin, or MSYS2, for example. One way to address this is to use a fully-qualified Windows path for the socket instead of an environment-specific Unix/MSYS2 path.

#### Windows 7

Launch the Control Panel, and then select _System_ followed by _Advanced system settings_. Click on the _Environment Variables_ button, and finally _New..._ in the _User variables_ (**not** _System variables_) section. Enter **SSH_AUTH_SOCK** for _Variable name_ and **/tmp/.ssh-pageant-%USERNAME%** for _Variable value_, then click _OK_.

Now launch a new Git CMD or Windows command prompt (pre-existing sessions won't see the new variable), and enter the command `set SSH_AUTH_SOCK`. If all went according to plan, you should see something similar to the following.

    C:\Users\heinz.doofenshmirtz> set SSH_AUTH_SOCK
    SSH_AUTH_SOCK=/tmp/.ssh-pageant-heinz.doofenshmirtz

NOTE: the cross-environment-compatible (git for windows, msys2, and cygwin) equivalent would require a fully qualified windows path like `C:\Users\MYUSERNAME\AppData\Local\Temp\.ssh-pageant-MYUSERNAME`. The correct value can be determined by running a command like `cygpath --windows /tmp/.ssh-pageant-%USERNAME%` in a Git Bash window.

### Launch ssh\-pageant

At this point you should run _start\-ssh\-pageant.cmd_ manually, in order to verify that the agent starts successfully. Assuming that Git is installed into _C:\Program Files\Git_, this should look something like:

    C:\Program Files\Git\cmd> start-ssh-pageant
    Starting ssh-pageant...
    SSH_AUTH_SOCK='/tmp/.ssh-pageant-heinz.doofenshmirtz'
    SSH_PAGEANT_PID=11444

Assuming that the relevant keys have been loaded into Pageant, you should now be able to perform Git operations which rely upon them using OpenSSH without being prompted for the passphrase.

### Configure Automatic Startup

The most common approach is to create a shortcut pointing to _start\-ssh\-pageant.cmd_, and place it in your startup folder (_Start Menu_ / _Programs_ / _Startup_). Once in place, it should be launched automatically when you logon to Windows and be available to all Git processes.

#### Windows 7

1. Click the *Start* button, right click on *All Programs*, and select *Open*
2. Navigate into the *Programs* folder, followed by *Startup*
3. Right-click on an empty spot within *Startup*, and select **New / Shortcut**
4. Click *Browse*, navigate to the *cmd* folder underneath your Git installation, and select **start-ssh-pageant.cmd**. Click *OK*.
5. Click *Next*
6. Enter an alternate name for the shortcut, if desired, and click *Finish*

## Security Considerations

Since _ssh\-pageant_ (like _ssh\-agent_) is intended to bypass the requirement to repeatedly enter your private key password, it's imperative that its socket file be private in order to use it safely. In other words, you want to be **extremely** careful on multi-user systems to ensure that the **SSH_AUTH_SOCK** file -- and preferably the directory which includes it -- isn't accessible to anyone else. For a normal Git for Windows configuration this shouldn't be an issue, as _/tmp_ is normally mapped to a private location under your Windows user profile.