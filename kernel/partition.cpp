#include "partition.h"
#include "bio.h"
#include "lib.h"
#include <array>
#include <memory>
#include <optional>
#include <span>

namespace partition
{
    namespace constants
    {
        inline constexpr uint32_t Crc32ReversedPolynomial = 0xedb88320;
        inline constexpr std::array<uint8_t, 8> Signature = {
           'E', 'F', 'I', ' ', 'P', 'A', 'R', 'T'
        };
    }

    namespace
    {

        struct PartitionHeader {
            uint8_t signature[8];
            uint32_t revision;
            uint32_t header_size;
            uint32_t header_crc32;
            uint32_t reserved;
            uint64_t my_lba;
            uint64_t alternate_lba;
            uint64_t first_usable_lba;
            uint64_t last_usble_lba;
            uint8_t disk_guid[16];
            uint64_t partition_entry_lba;
            uint32_t number_of_partition_entries;
            uint32_t sizeof_partition_entry;
            uint32_t partition_entry_array_crc32;
        } __attribute__((packed));
        static_assert(sizeof(PartitionHeader) == 92);

        struct PartitionEntry {
            uint8_t partition_type_guid[16];
            uint8_t unique_partition_guid[16];
            uint64_t starting_lba;
            uint64_t ending_lba;
            uint64_t attributes;
            uint8_t partition_name[72];

            bool operator==(const PartitionEntry&) const = default;
        };
        static_assert(sizeof(PartitionEntry) == 128);

        // CRC32 implementation, based on Hacker's Delight, Figure 14-6
        uint32_t crc32(std::span<const uint8_t> input)
        {
            uint32_t crc = 0xffff'ffff;
            for(const auto byte: input) {
                crc = crc ^ byte;
                for(int j = 7; j >= 0; --j) {
                   const auto mask = -(crc & 1);
                   crc = (crc >> 1) ^ (constants::Crc32ReversedPolynomial & mask);
                }
            }
            return ~crc;
        }

        void PrintGUID(const uint8_t* ptr)
        {
            for(int n = 0; n < 16; ++n) {
                Print(print::Hex{ptr[n]}, " ");
            }
        }

        std::optional<PartitionHeader> ReadGptPartitionHeader(int device, bio::BlockNumber block_nr)
        {
            PartitionHeader ph;
            {
                auto buf = bio::ReadBlock(device, block_nr);
                assert(buf);
                memcpy(&ph, buf->data, sizeof(PartitionHeader));
            }

            if (memcmp(reinterpret_cast<const char*>(ph.signature), reinterpret_cast<const char*>(constants::Signature.data()), constants::Signature.size()) != 0) {
                Print("Invalid GPT signature on device ", device, ", ignoring\n");
                return {};
            }
            const auto headerCrc32 = ph.header_crc32;
            ph.header_crc32 = 0;
            uint32_t myHeaderCrc32 = crc32({ reinterpret_cast<const uint8_t*>(&ph), reinterpret_cast<const uint8_t*>(&ph + 1) });
            if (headerCrc32 != myHeaderCrc32) {
                Print("GPT: checksum error on device ", device, ", ignoring\n");
                return {};
            }
            return ph;
        }

        std::optional<std::unique_ptr<uint8_t[]>> ReadGptPartitions(int device, const PartitionHeader& ph)
        {
            const auto total_bytes = ph.number_of_partition_entries * ph.sizeof_partition_entry;
            auto partitions = std::make_unique<uint8_t[]>(total_bytes);

            uint8_t* partition_ptr = &partitions[0];
            auto bytes_left = total_bytes;
            for (bio::BlockNumber blockNr = ph.partition_entry_lba; bytes_left > 0; ++blockNr) {
                const auto chunk_length = std::min(bytes_left, bio::BlockSize);
                auto buf = bio::ReadBlock(device, blockNr);
                assert(buf);
                memcpy(partition_ptr, buf->data, chunk_length);

                partition_ptr += chunk_length;
                bytes_left -= chunk_length;
            }

            const auto myPartitionCrc32 = crc32({ reinterpret_cast<uint8_t*>(&partitions[0]), reinterpret_cast<uint8_t*>(&partitions[total_bytes]) });
            if (ph.partition_entry_array_crc32 != myPartitionCrc32) {
                Print("GPT: checksum errro on partitions of device ", device, " ignoring\n");
                return {};
            }
            return partitions;
        }
    }

    constexpr inline bio::BlockNumber PrimaryGptLba = 1;

    void Initialize()
    {
        int device = 0;

        const auto ph = ReadGptPartitionHeader(device, PrimaryGptLba);
        if (!ph) return;
        if (ph->sizeof_partition_entry != sizeof(PartitionEntry)) {
            Print("GPT: partition size mismatch on device ", device, " ignoring\n");
            return;
        }
        auto partitions = ReadGptPartitions(device, *ph);
        if (!partitions) return;

        auto entry = reinterpret_cast<PartitionEntry*>(partitions->get());

        int m = 1;
        for(unsigned int n = 0; n < ph->number_of_partition_entries; ++n) {
            if (entry[n] == PartitionEntry{}) continue;

            Print("entry ", n, " starting_lba ", entry[n].starting_lba, " ending_lba ", entry[n].ending_lba, " type ");
            PrintGUID(entry[n].partition_type_guid);
            Print("\n");

            bio::RegisterDevice(device + m, entry[n].starting_lba);
            ++m;
        }

        Print("\n");
    }
}
