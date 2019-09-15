#!/bin/sh
qemu-system-x86_64 -serial stdio -cdrom dogfood.iso -boot d $@
