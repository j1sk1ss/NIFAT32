gcc-14 unix_nifat32.c nifat32.c src/* std/* -DWARNING_LOGS -DDEBUG_LOGS -o builds/nifat32_unix
cd formatter && make && ./formatter --jc 1 -o nifat32.img -s nifat32
rm formatter
cd ..
mv formatter/nifat32.img .

./builds/nifat32_unix
