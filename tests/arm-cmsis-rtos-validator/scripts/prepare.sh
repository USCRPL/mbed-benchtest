#! /bin/bash
set -euo pipefail
IFS=$'\n\t'

# The available Keil packs are listed at http://www.keil.com/dd2/pack/
# Archives are to be downloaded from:
# http://www.keil.com/pack/ARM.CMSIS-RTOS_Validation.x.x.x.pack

RELEASE_VERSION="1.0.0"

GITHUB_PROJECT="xpacks/arm-cmsis-rtos-validator"
NAME_PREFIX="ARM.CMSIS-RTOS_Validation"
ARCHIVE_NAME="${NAME_PREFIX}.${RELEASE_VERSION}.pack"
ARCHIVE_URL="http://www.keil.com/pack/${ARCHIVE_NAME}"

LOCAL_ARCHIVE_FILE="/tmp/xpacks/${ARCHIVE_NAME}"

echo "Cleaning previous files..."
for f in *
do
  if [ "${f}" == "scripts" ]
  then
    :
  else
    rm -rf "${f}"
  fi
done

if [ ! -f "${LOCAL_ARCHIVE_FILE}" ]
then
  mkdir -p $(dirname ${LOCAL_ARCHIVE_FILE})
  curl -o "${LOCAL_ARCHIVE_FILE}" -L "${ARCHIVE_URL}"
fi

echo "Unpacking '${ARCHIVE_NAME}'..."
unzip -q "${LOCAL_ARCHIVE_FILE}"

echo "Removing unnecessary files..."

rm -rf \
Documents \
Examples \


find . -name '*.exe' -exec rm \{} \;

echo "Creating README.md..."
cat <<EOF >README.md
# ${ARCHIVE_NAME}

This project, available from [GitHub](https://github.com/${GITHUB_PROJECT}),
includes the ${ARCHIVE_NAME} files.

## Version

* v${RELEASE_VERSION}

## Documentation

The latest CMSIS documentation is available from
[keil.com](http://www.keil.com/cmsis).

## Original files

The original files are available in the \`originals\` branch.

These files were extracted from \`${ARCHIVE_NAME}\`.

To save space, the following folders/files were removed:

* Documents
* Examples

EOF

echo "Done."
