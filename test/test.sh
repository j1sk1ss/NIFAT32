#!/bin/bash

BUILD_FLAGS=""

if [ "$1" == "--new-image" ]; then
    cd ../formatter || exit 1
    make || exit 1
    ./formatter -o nifat32.img -s nifat32 || exit 1
    cd .. || exit 1
    mv formatter/nifat32.img test/ || exit 1
    cd test || exit 1
    shift
fi

if [ "$1" == "--debug" ]; then
    BUILD_FLAGS="-DERROR_LOGS -DWARNING_LOGS -DDEBUG_LOGS"
    shift
fi

cd ..
gcc-14 $BUILD_FLAGS test/fat32_test.c nifat32.c src/* std/* -o test/fat32_test || exit 1

cd test || exit 1
./fat32_test "$1"
rm fat32_test
