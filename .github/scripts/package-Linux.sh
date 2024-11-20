#!/bin/bash

rm -rf Plugins/Release
rm -f Plugins/Tests
cp -r Plugins plugdata
cp README.md plugdata/README.md
cp LICENSE plugdata/LICENSE.txt

# Create tar.gz with unique name for each distro
tar -czvf plugdata-$1.tar.gz plugdata

.github/scripts/generate-upload-info.sh
