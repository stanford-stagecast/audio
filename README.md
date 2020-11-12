# audio
Stagecast Audio group repo

[![Compiles](https://github.com/stanford-stagecast/audio/workflows/Compile/badge.svg?event=push)](https://github.com/stanford-stagecast/audio/actions)

# Build directions

To build you need to have:
```
g++ >= 8.0
gcc >= 8.0
libasound2-dev
libdbus1-dev
libx264-dev
```
Note: If you g++-8/gc-8 properly installed and it still doesn't work,
make sure to check out this article: https://linuxconfig.org/how-to-switch-between-multiple-gcc-and-g-compiler-versions-on-ubuntu-20-04-lts-focal-fossa
which tells you how to set your default g++/gcc compilers to version 8.

Once this is done, execute the following commands:
```
$ mkdir build
$ cd build/
$ cmake ..
$ make
```

# Contents of this Repo:
-src/stats: Internet Statistics Measuring Project
