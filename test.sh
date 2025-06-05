make # Make test build
cd formatter && make && ./formatter -o nifat32.img -s nifat32
cd ..
mv formatter/nifat32.img .

./builds/nifat32_unix
