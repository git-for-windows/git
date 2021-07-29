#!/bin/bash

for i in "$@"
do
case "$i" in
	--package=*)
	PACKAGE="${i#*=}"
	shift # past argument=value
	;;
	--keychain-profile=*)
	KEYCHAIN_PROFILE="${i#*=}"
	shift # past argument=value
	;;
	*)
	die "unknown option '$i'"
	;;
esac
done

if [ -z "$PACKAGE" ]; then
    echo "error: missing package argument"
    exit 1
elif [ -z "$KEYCHAIN_PROFILE" ]; then
    echo "error: missing keychain profile argument"
    exit 1
fi

# Exit as soon as any line fails
set -e

# Send the notarization request
xcrun notarytool submit -v "$PACKAGE" -p "$KEYCHAIN_PROFILE" --wait

# Staple the notarization ticket (to allow offline installation)
xcrun stapler staple -v "$PACKAGE"
