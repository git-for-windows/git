#!/bin/bash

sign_directory () {
	(
	cd "$1"
	for f in *
	do
		macho=$(file --mime $f | grep mach)
		# Runtime sign dylibs and Mach-O binaries
		if [[ $f == *.dylib ]] || [ ! -z "$macho" ];
		then
			echo "Runtime Signing $f"
			codesign -s "$IDENTITY" $f --timestamp --force --options=runtime --entitlements $ENTITLEMENTS_FILE
		elif [ -d "$f" ];
		then
			echo "Signing files in subdirectory $f"
			sign_directory "$f"

		else
			echo "Signing $f"
			codesign -s "$IDENTITY" $f  --timestamp --force
		fi
	done
	)
}

for i in "$@"
do
case "$i" in
	--payload=*)
	SIGN_DIR="${i#*=}"
	shift # past argument=value
	;;
	--identity=*)
	IDENTITY="${i#*=}"
	shift # past argument=value
	;;
	--entitlements=*)
	ENTITLEMENTS_FILE="${i#*=}"
	shift # past argument=value
	;;
	*)
	die "unknown option '$i'"
	;;
esac
done

if [ -z "$SIGN_DIR" ]; then
    echo "error: missing directory argument"
    exit 1
elif [ -z "$IDENTITY" ]; then
    echo "error: missing signing identity argument"
    exit 1
elif [ -z "$ENTITLEMENTS_FILE" ]; then
    echo "error: missing entitlements file argument"
    exit 1
fi

echo "======== INPUTS ========"
echo "Directory: $SIGN_DIR"
echo "Signing identity: $IDENTITY"
echo "Entitlements: $ENTITLEMENTS_FILE"
echo "======== END INPUTS ========"

sign_directory "$SIGN_DIR"
