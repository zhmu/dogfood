#!/usr/bin/env python3

import os
import shutil
import struct
import subprocess

OUTPUT_DIR = 'data'
OUTPUT_IMAGE = 'ext2.img'
BLOCK_SIZE = 2048
IMAGE_SIZE = '2m'

if os.path.exists(OUTPUT_DIR):
    shutil.rmtree(OUTPUT_DIR)
if os.path.exists(OUTPUT_IMAGE):
    os.remove(OUTPUT_IMAGE)

os.makedirs(OUTPUT_DIR)
for p in [ 'dir1', 'dir2' ]:
    os.makedirs(os.sep.join([ OUTPUT_DIR, p ]))
os.symlink('does_not_exist', os.sep.join([ OUTPUT_DIR, 'symlink' ]))

with open(os.sep.join([ OUTPUT_DIR, 'file1.txt' ]), 'wt') as f:
    content = '''Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.

Curabitur pretium tincidunt lacus. Nulla gravida orci a odio. Nullam varius, turpis et commodo pharetra, est eros bibendum elit, nec luctus magna felis sollicitudin mauris. Integer in mauris eu nibh euismod gravida. Duis ac tellus et risus vulputate vehicula. Donec lobortis risus a elit. Etiam tempor. Ut ullamcorper, ligula eu tempor congue, eros est euismod turpis, id tincidunt sapien risus a quam. Maecenas fermentum consequat mi. Donec fermentum. Pellentesque malesuada nulla a mi. Duis sapien sem, aliquet nec, commodo eget, consequat quis, neque. Aliquam faucibus, elit ut dictum aliquet, felis nisl adipiscing sapien, sed malesuada diam lacus eget erat. Cras mollis scelerisque nunc. Nullam arcu. Aliquam consequat. Curabitur augue lorem, dapibus quis, laoreet et, pretium ac, nisi. Aenean magna nisl, mollis quis, molestie eu, feugiat in, orci. In hac habitasse platea dictumst.

'''
    f.write(content)

with open(os.sep.join([ OUTPUT_DIR, 'file2.bin' ]), 'wb') as f:
    for b in range(0, 32):
        # block
        for n in range(0, BLOCK_SIZE):
            f.write(struct.pack('B', b))

subprocess.run([ '/sbin/mkfs.ext2', '-b', str(BLOCK_SIZE), '-d', OUTPUT_DIR, OUTPUT_IMAGE, IMAGE_SIZE ])
