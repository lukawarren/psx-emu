#!/bin/bash

FPS=60

cd build
if ninja; then
    rm *.mp4 *.ppm
    ./psx-emu

    rm -f /tmp/psx_*.mp4
    ffmpeg -y -i /tmp/psx_frame_%04d.ppm -c:v libx264 -pix_fmt yuv420p -filter:v fps=$FPS /tmp/psx.mp4
    rm -f /tmp/psx_*.ppm

    xdg-open /tmp/psx.mp4
fi