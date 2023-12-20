#/bin/bash

set -e

cd Musashi
make
cd ..

#gcc -o narrator narrator.c *.o softfloat/*.o
gcc -IMusashi -o translator translator.c Musashi/*.o Musashi/softfloat/*.o
