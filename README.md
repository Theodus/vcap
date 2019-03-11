# vcap
## Video device capture for V4L2 on Petalinux

### Build Instructions

#### Build a Native Executable
```shell
# make native
```
The resulting binary will be written to `./build/native/vcap`.

#### Build a Cross-compiled Executable

TODO

### Identifying Video Device and Configuration

List the available video devices
```shell
# v4l2-ctl --list-devices
```

Choose the flags for vcap from the set of supported options for your device (following examples using `/dev/video7`)
```shell
# v4l2-ctl -d /dev/video7 --list-formats-ex
```

### Dump Video to File

Invoke vcap using the options that you have chosen for your device, dumping the yuyv422 video to file
```
# ./vcap -d /dev/video7 -c 120 -r 640x480 -f 30 -o > out.yuv
```
Read the output of `./vcap -h` for more information on the options.

The shell script `playback.sh` may be used to play the resulting video using `ffplay`
```shell
# ./playback.sh out.yuv 30 640x480
```

### Output to HDMI

TODO
