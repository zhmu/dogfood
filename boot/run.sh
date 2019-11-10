#!/bin/sh
#qemu-system-x86_64 -serial stdio -hda /home/rink/github/dogfood/boot/ext2.img -cdrom /home/rink/github/dogfood/boot/dogfood.iso -boot d $@
qemu-system-x86_64 -serial stdio -hda /tmp/ext2.img -cdrom /home/rink/github/dogfood/boot/dogfood.iso -boot d $@
