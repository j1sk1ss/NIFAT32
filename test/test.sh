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

DEL_BIN=0
if [ "$1" == "--del-bin" ]; then
    DEL_BIN=1
    shift
fi

cd ..
gcc-14 $BUILD_FLAGS test/nifat32_test.c nifat32.c src/* std/* -o test/nifat32_test -g || exit 1

cd test || exit 1
./nifat32_test $1

if [ DEL_BIN == 1 ]; then
    rm nifat32_test
fi
