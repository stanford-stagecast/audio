# audio
Stagecast Audio group repo

[![Compiles](https://github.com/stanford-stagecast/audio/workflows/Compile/badge.svg?event=push)](https://github.com/stanford-stagecast/audio/actions)

# Build directions

To build you need to have:
```
g++ >= 9.0
libasound2-dev
libdbus-1-dev
libopus-dev
libsndfile1-dev
libavformat-dev
libssl-dev
libcrypto++-dev
librubberband-dev
libsamplerate-dev
libfftw3-dev
libv4l-dev
libjpeg-turbo8-dev
libswscale-dev
libx264-dev
libjsoncpp-dev
```
Note: If you have installed g++-9 and it still doesn't work,
make sure to check out this article: https://linuxconfig.org/how-to-switch-between-multiple-gcc-and-g-compiler-versions-on-ubuntu-20-04-lts-focal-fossa
which tells you how to set your default g++/gcc compilers to version 9.

Once this is done, execute the following commands:
```
$ mkdir build
$ cd build/
$ cmake ..
$ make
```
