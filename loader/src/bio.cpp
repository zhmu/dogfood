/*-
 * SPDX-License-Identifier: Zlib
 *
 * Copyright (c) 2009-2023 Rink Springer <rink@rink.nu>
 * For conditions of distribution and use, see LICENSE file
 */
extern "C" {
#include <efi.h>
#include <efilib.h>
extern EFI_HANDLE LibImageHandle;
}
#include "bio.h"
#include "lib.h"

namespace
{
    CHAR16*
    DevicePathToString(EFI_DEVICE_PATH* path)
    {
        static EFI_DEVICE_PATH_TO_TEXT_PROTOCOL* text_protocol = nullptr;
        if (!text_protocol) {
            auto status = BS->LocateProtocol(&gEfiDevicePathToTextProtocolGuid, NULL, reinterpret_cast<void**>(&text_protocol));
            if (EFI_ERROR(status)) return NULL;
        }
        return text_protocol->ConvertDevicePathToText(path, TRUE, TRUE);
    }
}

namespace bio {
    namespace
    {
        struct BlockDevice
        {
            CHAR16* id;
            EFI_BLOCK_IO* blockIo;
        };
        BlockDevice* block_devices;
        int num_block_devices = 0;

        constexpr inline auto NumberOfBios = 32;
        Buffer* buffer;
        size_t next_buffer_index = 0;

        Buffer& GetBuffer(Device dev, BlockNumber nr)
        {
            for(size_t n = 0; n < NumberOfBios; ++n) {
                auto& buf = buffer[n];
                if (buf.device == dev && buf.blockNr == nr) return buf;
            }
            auto& buf = buffer[next_buffer_index];
            next_buffer_index = (next_buffer_index + 1) % NumberOfBios;
            buf.device = -1;
            buf.blockNr = -1;
            return buf;
        }
    }

    void Initialize()
    {
        buffer = new Buffer[NumberOfBios];

        UINTN hsize = 0;
        BS->LocateHandle(ByProtocol, &gEfiBlockIoProtocolGuid, nullptr, &hsize, nullptr);
        const auto num_handles = hsize / sizeof(EFI_HANDLE);

        block_devices = new BlockDevice[num_handles];

        auto handles = new EFI_HANDLE[num_handles];
        BS->LocateHandle(ByProtocol, &gEfiBlockIoProtocolGuid, nullptr, &hsize, handles);

        for(UINTN n = 0; n < num_handles; ++n) {
            EFI_DEVICE_PATH* device_path;
            auto status = BS->OpenProtocol(handles[n], &gEfiDevicePathProtocolGuid, reinterpret_cast<void**>(&device_path), LibImageHandle, NULL, EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
            if (status != EFI_SUCCESS) continue;

            EFI_BLOCK_IO* blockIo;
            status = BS->OpenProtocol(handles[n], &gEfiBlockIoProtocolGuid, reinterpret_cast<void**>(&blockIo), LibImageHandle, NULL, EFI_OPEN_PROTOCOL_BY_HANDLE_PROTOCOL);
            if (status != EFI_SUCCESS) continue;
            // TODO Not yet, enable once we support partitions in Dogfood
            //if (!blockIo->Media->LogicalPartition) continue;

            auto id = DevicePathToString(device_path);
            //BS->FreePool(id);

            block_devices[num_block_devices] = BlockDevice{ id, blockIo };
            ++num_block_devices;
        }

        Print(reinterpret_cast<const CHAR16*>(L"found %d block device(s)\n"), num_block_devices);
        delete[] handles;
    }

    Buffer& bread(bio::Device dev, BlockNumber nr)
    {
        assert(dev < num_block_devices);
        auto& device = block_devices[dev];
        auto& buf = GetBuffer(dev, nr);
        if (buf.device == dev && buf.blockNr == nr) {
            return buf;
        }
        EFI_STATUS status = device.blockIo->ReadBlocks(device.blockIo, device.blockIo->Media->MediaId, nr, BlockSize, buf.data);
        if (status != EFI_SUCCESS) {
            Print(reinterpret_cast<const CHAR16*>(L"bread(): dev %d lba %d error %x\n"), dev, nr, status);
            panic("read error");
        }
        buf.device = dev;
        buf.blockNr = nr;
        return buf;
    }

    void bwrite(Buffer&)
    {
        panic("bwrite() is unsupported");
    }

    void brelse(Buffer&)
    {
    }

    Device GetNumberOfDevices()
    {
        return num_block_devices;
    }
}
