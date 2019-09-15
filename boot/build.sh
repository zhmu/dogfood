#!/bin/sh -e
# Note: assumes you did: mkdir build && cd build && cmake -GNinja ..
(cd ../build && ninja kernel_mb)
cp ../build/kernel/kernel.mb iso
grub-mkrescue -o dogfood.iso iso
