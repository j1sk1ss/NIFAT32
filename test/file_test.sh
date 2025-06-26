#!/bin/bash

BUILD_FLAGS=""

if [ "$1" == "--new-image" ]; then
    cd ../formatter || exit 1
    make || exit 1
    ./formatter -o nifat32.img -s nifat32 --v_size 64 || exit 1
    rm formatter || exit 1
    cd .. || exit 1
    mv formatter/nifat32.img test/ || exit 1
    cd test || exit 1
    shift
fi

if [ "$1" == "--debug" ]; then
    BUILD_FLAGS="-DERROR_LOGS -DWARNING_LOGS -DDEBUG_LOGS -g"
    shift
fi

if [ "$1" == "--log" ]; then
    BUILD_FLAGS="-DERROR_LOGS -DWARNING_LOGS -g"
    shift
fi

DEL_BIN=0
if [ "$1" == "--del-bin" ]; then
    DEL_BIN=1
    shift
fi

cd ..
gcc-14 $BUILD_FLAGS test/nifat32_many_files.c nifat32.c src/* std/* -o test/nifat32_many_files || exit 1

cd test || exit 1
./nifat32_many_files $1

if [ "$DEL_BIN" == 1 ]; then
    rm nifat32_many_files nifat32.img
fi
