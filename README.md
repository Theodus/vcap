# vcap
## Video device capture for V4L2 on Petalinux

### Build Instructions

#### Build a Native Executable
```console
# make native
```
The resulting binary will be written to `./build/native/vcap`.

#### Build a Cross-compiled Executable

Specify the cross-compiler toolchain prefix and the path of required libraries such as V4L2 (following example using prefix of libs bundled with xsdk from installed zc702 base TRD)
```console
# VENDOR_LIB_PATH=/home/theodus/Documents/rdf0286-zc702-zvik-base-trd-2015-4/software/xsdk/lib \
    INCLUDES=-I/home/theodus/Documents/rdf0286-zc702-zvik-base-trd-2015-4/software/xsdk/include \
    CROSS_TOOLCHAIN_PREFIX=/opt/Xilinx/SDK/2015.4/gnu/arm/lin \
    make cross
```
The resulting binary will be written to `./build/cross/vcap`.

### Identifying Video Device and Configuration

List the available video devices
```console
# v4l2-ctl --list-devices
```

Choose the flags for vcap from the set of supported options for your device (following examples using `/dev/video7`)
```console
# v4l2-ctl -d /dev/video7 --list-formats-ex
```

### Dump Video to File

Invoke vcap using the options that you have chosen for your device, dumping the yuyv422 video to file
```
# ./vcap -d /dev/video7 -c 120 -r 640x480 -f 30 -o > out.yuv
```
Read the output of `./vcap -h` for more information on the options.

The shell script `playback.sh` may be used to play the resulting video using `ffplay`
```console
# ./playback.sh out.yuv 30 640x480
```

### Output to HDMI

TODO
