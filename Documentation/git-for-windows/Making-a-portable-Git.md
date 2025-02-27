As the portable Git is made by putting together files from the Git SDK, it is advisable to update those files first. To update the packages, call `pacman -Syu` ([caveats apply](https://github.com/git-for-windows/git/blob/HEAD/Documentation/git-for-windows/Package-management.md#updating-msys2-runtime-pacman-and-bash)). If you want to test Git changes or make an portable Git from your own Git fork, you need to call `cd /usr/src/git && make install` (for documentation changes, `make install-html` and *afterwards* `prefix=/mingw64 make -C contrib/subtree install-html` - without the second `make` you get a error message about a missing `git-subtree.html` file).

To package the portable Git, install the [SDK](https://gitforwindows.org/#download-sdk), run it and then issue the following commands:

```bash
cd /usr/src/build-extra
git fetch
git checkout main
./portable/release.sh <version>-test
```

where `<version>` is the Git version.
