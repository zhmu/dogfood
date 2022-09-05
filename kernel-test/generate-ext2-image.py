#!/usr/bin/env python3

import os
import shutil
import struct
import subprocess

OUTPUT_DIR = 'data'
OUTPUT_IMAGE = 'ext2.img'
OUTPUT_SOURCE = 'ext2-image.cpp'
BLOCK_SIZE = 2048
IMAGE_SIZE = '2m'

def generate_updates(buf):
    updates = [ ]
    cur_update = None
    for n, c in enumerate(buf):
        if cur_update is not None:
            if cur_update['byte'] == c:
                cur_update['end'] = n
                continue

            updates.append(cur_update)
            cur_update = None

        if c == 0:
            continue
        cur_update = { 'start': n, 'end': n, 'byte': c }

    if cur_update is not None:
        updates.append(cur_update)
    return updates

def apply_updates(buf, updates):
    for u in updates:
        for m in range(u['start'], u['end'] + 1):
            buf[m] = u['byte']

def test_updates():
    buffer = bytearray([ 0x00, 0x00, 0x12, 0x12, 0x34, 0x00, 0x00, 0x00, 0x11])
    updates = generate_updates(buffer)

    b = bytearray(len(buffer))
    apply_updates(b, updates)

    if buffer != b:
        raise Exception('test_updates: failed')

def generate_update_code(updates):
    s = [ ]
    for u in updates:
        if u['start'] == u['end']:
            s.append('    buffer[0x%x] = 0x%02x;' % (u['start'], u['byte']))
        else:
            s.append('    std::fill(buffer.begin() + 0x%x, buffer.begin() + 0x%x, 0x%02x);' % (u['start'], u['end'] + 1, u['byte']))
    return s

test_updates()

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

with open(OUTPUT_IMAGE, 'rb') as f:
    buf = f.read()
    updates = generate_updates(buf)

    test = bytearray(len(buf))
    apply_updates(test, updates)
    if bytearray(buf) != test:
        raise Exception('update error')

    lines = []
    lines.append('#include <vector>')
    lines.append('#include <cstdint>')
    lines.append('')
    lines.append('std::vector<uint8_t> GenerateImage()')
    lines.append('{')
    lines.append('    std::vector<uint8_t> buffer;')
    lines.append('    buffer.resize(0x%x);' % len(buf))
    lines += generate_update_code(updates)
    lines.append('    return buffer;')
    lines.append('}')
    lines.append('')

with open(OUTPUT_SOURCE, 'wt') as f:
    f.write('\n'.join(lines))
