gcc-14 unix_nifat32.c nifat32.c -Iinclude src/* std/* -DERROR_LOGS -DLOGGING_LOGS -DWARNING_LOGS -DDEBUG_LOGS -o builds/nifat32_unix

cd formatter && make && ./formatter --jc 2 -o nifat32.img -s nifat32
rm formatter
cd ..
mv formatter/nifat32.img .

# argv[1] - Path to nifat32 image
# argv[2] - Image size
# argv[3] - Sector size
# argv[4] - bs_count
# argv[5] - jc
./builds/nifat32_unix nifat32.img 64 512 5 2
