#!/bin/bash

cd ..
cd formatter
make
./formatter -o nifat32.img -s nifat32 
cd ..
mv formatter/nifat32.img test/
gcc-14 test/fat32_test.c nifat32.c src/* std/* -o test/fat32_test
cd test
