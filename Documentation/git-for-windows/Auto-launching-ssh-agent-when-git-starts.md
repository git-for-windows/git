If you want your passphrase to be 'remembered' for a session (or configurable timeout period) you will need to setup an ssh-agent process to handle this key.

Recent versions of git for windows 2.x come with an ssh agent startup script and the installer also checks if an ssh agent is currently running and asks you to kill this process.

Run the ssh agent:

    start-ssh-agent.cmd

This should work both in a `cmd` and `bash` shell and can be included in `~/.profile` or `~/.bashrc`.

The [GitHub instructions](https://help.github.com/articles/working-with-ssh-key-passphrases/#auto-launching-ssh-agent-on-msysgit) are still valid but not needed anymore.

## Manually

To launch, put in `~/.profile` or `~/.bashrc`:

```bash
# ssh-agent auto-launch (0 = agent running with key; 1 = w/o key; 2 = not run.)
agent_run_state=$(ssh-add -l >| /dev/null 2>&1; echo $?)
if   [ $agent_run_state = 2 ]; then
  eval $(ssh-agent -s)
  ssh-add ~/.ssh/id_rsa
elif [ $agent_run_state = 1 ]; then
  ssh-add ~/.ssh/id_rsa
fi
```

To close on shell exit, put in `~/.bash_logout`:

```bash
ssh-agent -k
```