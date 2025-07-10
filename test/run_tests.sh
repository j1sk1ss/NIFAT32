#!/bin/bash

BUILD_FLAGS=""
DEL_BIN=1

# -------------------------------
# Flags
# -------------------------------
if [ "$1" == "--new-image" ]; then
    cd ../formatter || exit 1
    make || exit 1
    ./formatter -o nifat32.img -s nifat32 --v_size 64 || exit 1
    rm formatter || exit 1
    cd .. || exit 1
    mv formatter/nifat32.img . || exit 1
    cd test || exit 1
    shift
fi

if [ "$1" == "--debug" ]; then
    BUILD_FLAGS="-DERROR_LOGS -DWARNING_LOGS -DDEBUG_LOGS -DINFO_LOGS -DLOGGING_LOGS -g"
    shift
fi

if [ "$1" == "--ndel-bin" ]; then
    DEL_BIN=0
    shift
fi

cd ..

# -------------------------------
# ompilation and startup
# -------------------------------
for TEST_FILE in test/test_*.c; do
    BIN_NAME=$(basename "$TEST_FILE" .c)

    echo "Compilation $BIN_NAME..."
    gcc-14 $BUILD_FLAGS "$TEST_FILE" test/nifat32_test.h nifat32.c src/* std/* -o "test/$BIN_NAME" || {
        echo "error $BIN_NAME"
        continue
    }

    echo "Running $BIN_NAME..."
    ./test/"$BIN_NAME" "$@"
    RET=$?
    if [ $RET -eq 0 ]; then
        echo "$BIN_NAME: Test passed!"
    else
        echo "$BIN_NAME: Test failed with code $RET"
    fi

    if [ "$DEL_BIN" == 1 ]; then
        rm "test/$BIN_NAME"
    fi

    echo "-----------------------------------------"
done
