Page for things that pop up while fiddling with MSYS2.

# PS1 script

I had to replace `PS1="$PS1"'\n'` with `PS1="$PS1"$'\n'` within the `profile` file to get the git prompt working. See also [this msys2 ticket page](http://sourceforge.net/p/msys2/tickets/17/)

# PS1 Script (continuation)

If you have this error (or something alike) with a prompt like  this one: `\e[0;36m[\e[1;36m\t\e[0m\e[0;36m]\e[0m \e[0;33m\w\e[0m`**`$(__git_ps1 ' \e[0;31m(\e[1;31m%s\e[0m\e[0;31m)\e[0m')`**`\n$`, then you will have to replace the `$(...)` syntax by backtick (`).

This PS1 will do the following error:
```
[00:48:02] /
$ echo $PS1
\e[0;36m[\e[1;36m\t\e[0m\e[0;36m]\e[0m \e[0;33m\w\e[0m$(__git_ps1 ' \e[0;31m(\e[1;31m%s\e[0m\e[0;31m)\e[0m')\n$
bash: command substitution: line 1: syntax error near unexpected token `)'
bash: command substitution: line 1: `__git_ps1 ' (%s)')'
```

While this one won't:

```
$ export PS1="\\e[0;36m[\\e[1;36m\\t\\e[0m\\e[0;36m]\\e[0m \\e[0;33m\\w\\e[0m\`__git_ps1 ' \\e[0;31m(\\e[1;31m%s\\e[0m\\e[0;31m)\\e[0m'\`\\n\$ "
```

See [this stackoverflow question](http://stackoverflow.com/questions/21517281/ps1-command-substitution).

# Debugging the environment conversion to UTF-8

When trying to debug environment issues, it can be very helpful to print out the environment as it comes into Git. Unfortunately, `mingw_startup()` – which converts the environment to UTF-8 – is run so early that the `sprintf()` family of functions does not work: the locale has not been initialized yet (because this would require the environment, using the `LC_ALL` environment variable!). Therefore, something like this is required:
```c
#include <strsafe.h>
...
{
  char buffer[1024];
  StringCbPrintf(buffer, sizeof(buffer), "env %d is '%s'\n", i, environ[i]);
  write(2, buffer, strlen(buffer));
}
```

# Changing the colors in the Terminal

See https://github.com/mavnn/mintty-colors-solarized

# Building the git documentation
- pacman -S asciidoc xmlto
- start a msys shell
- make doc

In a mingw32 shell asciidoc fails with
```
../git/Documentation$ asciidoc git-annotate.txt
asciidoc: FAILED: configuration file asciidoc.conf missing
```

# Openssh

Complains about world writeable private key files, although that makes no sense on windows.
Can be fixed with

```
chmod 600 ~/.ssh/*
```

# VIM

VIM might complain about trailing character in the `_vimrc` file, such as:

```
line    3:
E488: Trailing characters: number^M
line    4:
E488: Trailing characters: nowrap^M
line    5:
```

This means the `_vimrc` have CRLF end line style instead of LF. You can either use a graphical tool such as Notepad++ or jEdit, or simply use dos2unix:

```
dos2unix $HOME/_vimrc
```
