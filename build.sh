#/bin/bash

set -e

cd Musashi
make
cd ..

gcc -IMusashi -o translator translator.c Musashi/*.o Musashi/softfloat/*.o
gcc -IMusashi -o narrator narrator.c Musashi/*.o Musashi/softfloat/*.o

