#include "file.h"
#include "console.h"
#include "errno.h"
#include "process.h"
#include "fs.h"
#include "lib.h"

namespace file
{

File* Allocate(process::Process& proc)
{
    for(auto& file: proc.files) {
        if (file.f_refcount != 0) continue;
        file = File{};
        ++file.f_refcount;
        return &file;
    }
    return nullptr;
}

void Free(File& file)
{
    assert(file.f_refcount > 0);
    if (--file.f_refcount > 0) return;

    if (file.f_inode != nullptr)
        fs::iput(*file.f_inode);
}

void CloneTable(const process::Process& parent, process::Process& child)
{
    for(int n = 0; n < process::maxFiles; ++n) {
        const auto& parentFile = parent.files[n];
        if (parentFile.f_refcount == 0) continue;

        auto& childFile = child.files[n];
        childFile.f_refcount = 1;
        childFile.f_console = parentFile.f_console;
        childFile.f_offset = parentFile.f_offset;
        if (parentFile.f_inode != nullptr) {
            fs::iref(*parentFile.f_inode);
            childFile.f_inode = parentFile.f_inode;
        }
    }
}

int Write(File& file, const void* buf, int len)
{
    if (file.f_console)
        return console::Write(buf, len);

    return -EROFS;
}

int Read(File& file, void* buf, int len)
{
    if (file.f_console)
        return console::Read(buf, len);

    const auto count = fs::Read(*file.f_inode, buf, file.f_offset, len);
    file.f_offset += count;
    return count;
}

File* FindByIndex(process::Process& proc, int fd)
{
    if (fd < 0 || fd >= process::maxFiles) return nullptr;
    File& file = proc.files[fd];
    if (file.f_refcount == 0) return nullptr;
    return &file;
}

}
