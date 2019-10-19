#pragma once

#include "types.h"

namespace fs { struct Inode; }
namespace process { struct Process; }

namespace file {

struct File
{
    int f_refcount = 0;
    bool f_console = false;
    fs::Inode* f_inode = nullptr;
    off_t f_offset = 0;
};

File* Allocate(process::Process& proc);
void Free(File&);
void CloneTable(const process::Process& parent, process::Process& child);
File* FindByIndex(process::Process& proc, int fd);

int Write(File& file, const void* buf, int len);
int Read(File& file, void* buf, int len);

}
