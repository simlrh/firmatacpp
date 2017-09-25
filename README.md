#firmatacpp [![Donate](https://nourish.je/assets/images/donate.svg)](http://ko-fi.com/A250KJT)

A C++ firmata client library. Currently implements the basic protocol and I2C extension, version 2.5.

If you are having problems with messages like this:

```
CMake Error at vendor/serial/CMakeLists.txt:30 (add_library):
  Cannot find source file:

    src/serial.cc
```

then the magic commands are: `git submodule init && git submodule update`

Packages required to build on Ubuntu are: bluetooth bluez libbluetooth-dev libboost-all-dev
