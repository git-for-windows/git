Everything you need to host a git server on Windows is included with Git for Windows.
In these instructions it is assumed that Git for Windows is installed at `%GIT_HOME%` and that you have `%GIT_HOME%\cmd` in your path on both client and server machines.

### Configure sshd
You need to create a key pair within `%GIT_HOME%\etc\ssh` for sshd to use as the server identity, e.g. `ssh_host_rsa_key` and `ssh_host_rsa_key.pub` files.

Here is a good description of how to create the key pair: http://www.geekride.com/how-to-generate-ssh-host-keys/

### Run sshd
A simple way to start sshd automatically when you log in is to put a .BAT script in
`%USERPROFILE%\AppData\Roaming\Microsoft\Windows\Start Menu\Programs\Startup`.

Mine contains:

    cd /d %GIT_HOME%
    %GIT_HOME%\usr\bin\sshd.exe

### Test sshd
Your ultimate goal is probably to be able to ssh into the server where you are running sshd, without having to enter your password (there are security considerations which probably require you to consult policies within your local organization). I suggest you setup your user account(s) with ssh public key authentication for this.

Start by attempting to ssh from your client to the server. Assuming this is the first time, you will be prompted with:

    The authenticity of host 'localhost (::1)' can't be established.
    ECDSA key fingerprint is SHA256:lZPvG6eLvsX6dRxey0ShBlYjQubifOX6yuk3atg7jQ0.
    Are you sure you want to continue connecting (yes/no)?

Type 'yes' and hit enter. At this moment `%USERPROFILE%\.ssh` directory will be created for you, if it didn't exist already. In that directory will be a `known_hosts` file identifying this server as one that you trust. You should be prompted for your password at this point. Enter it in, and you should now have a remote shell on the server (congratulations if this is your first remote shell on Windows).

Now exit out of that shell to return to your local machine.  In either a bash window or a cmd window, enter the `.ssh` directory found in your home directory.  Type `ssh-keygen` and you will be prompted like this:

    Generating public/private rsa key pair.
    Enter file in which to save the key (/Users/yourname/.ssh/id_rsa):
    Enter passphrase (empty for no passphrase):
    Enter same passphrase again:

Depending on how physically secure your client machine is and your local security policies, you may choose to use an empty passphrase.  The passphrase is used to gain access to your local key store, so if you use an empty passphrase anyone who has access to your client machine can impersonate you.

Two files will have been generated, `id_rsa` which contains your private key, and `id_rsa.pub` which contains your public key.  Protect your private key and never share it with anyone. Append the contents of the `id_rsa.pub` file generated on the client to your `~/.ssh/authorized_keys` on the server (create it if it doesn't exist).

At this time you should be able to ssh from the client to the server without entering a password (but if you chose a non-empty passphrase the client will prompt you for that).  When a non-empty passphrase is used, there is also the option of running ssh-agent to keep the passphrase in-memory for you, but I'll not cover that here.

If you configure multiple client machines for you can copy your `.ssh` directory from client to client, and thus you would only need one public key in `~/.ssh/authorized_keys` on the server (this is discouraged though). In any case, make sure you protect the contents of `.ssh` directory on each client.

### Adding projects
I create projects directly on the server in `%GIT_HOME%` with:

`git init --bare yourproject.git`

Then from a client you can clone that project and add content to it from there.

Note: Creating them there is probably not ideal when it comes time to upgrading Git version on the server, so you may want to consider this before starting out.

### Connect with git
From your client machine (which may or may not be the same machine as the server) checkout projects via `git clone` with an ssh-protocol URL: `git clone ssh://your.server/yourproject.git`

Or if your local username on the client is different than on the server: `git clone ssh://you@your.server/yourproject.git`

If you have chosen to use public key authentication, you should not have to enter a password.