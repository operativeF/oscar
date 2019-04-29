#! /bin/bash
# First parameter is version number - (ex: 1.0.0)
# Second parametr is build or release status - (ex: beta or release)
#
rm -r tempDir
mkdir tempDir
cp build/oscar/OSCAR tempDir
cp -r build/oscar/Help tempDir
cp -r build/oscar/Html tempDir
cp -r build/oscar/Translations tempDir
cp OSCAR.png tempDir
cp OSCAR.desktop tempDir
#cp OSCAR-code/migration.sh tempDir
#
fpm --input-type dir --output-type deb  \
    --prefix /opt                   \
    --after-install ln_usrlocalbin.sh   \
    --before-remove rm_usrlocalbin.sh   \
    --name oscar --version $1 --iteration $2 \
    --category Other                \
    --maintainer oscar@nightowlsoftwre.ca   \
    --license GPL-v3                \
    --vendor nightowlsoftware.ca    \
    --description "Open Sourece CPAP Analysis Reporter" \
    --url https://sleepfiles.com/OSCAR  \
    --depends 'libqt5core5a > 5.7.0'    \
    --depends libqt5serialport5     \
    --depends libqt5xml5            \
    --depends libqt5network5        \
    --depends libqt5gui5            \
    --depends libqt5widgets5        \
    --depends libqt5opengl5         \
    --depends libqt5printsupport5   \
    --depends libglu1-mesa          \
    --depends libgl1                \
    --deb-no-default-config-files   \
    tempDir
