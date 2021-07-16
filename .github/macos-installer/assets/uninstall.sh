#!/bin/bash -e
if [ ! -r "/usr/local/git" ]; then
  echo "Git doesn't appear to be installed via this installer.  Aborting"
  exit 1
fi

if [ "$1" != "--yes" ]; then
  echo "This will uninstall git by removing /usr/local/git/, and symlinks"
  printf "Type 'yes' if you are sure you wish to continue: "
  read response
else
  response="yes"
fi

if [ "$response" == "yes" ]; then
  # remove all of the symlinks we've created
  pkgutil --files com.git.pkg | grep bin | while read f; do
    if [ -L /usr/local/$f ]; then
      sudo rm /usr/local/$f
    fi
  done

  # forget receipts.
  pkgutil --packages | grep com.git.pkg | xargs -I {} sudo pkgutil --forget {}
  echo "Uninstalled"

  # The guts all go here.
  sudo rm -rf /usr/local/git/
else
  echo "Aborted"
  exit 1
fi

exit 0
