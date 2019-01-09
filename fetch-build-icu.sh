#!/bin/sh

cd third-party
rm -rf icu
rm icu4c-63_1-src.tgz
wget http://download.icu-project.org/files/icu4c/63.1/icu4c-63_1-src.tgz
tar xzvf icu4c-63_1-src.tgz
cd ./icu/source
./runConfigureICU Linux --enable-static --disable-shared --disable-dyload
make CXXFLAGS="-O3 -std=c++17" -j5