#include "gtest/gtest.h"
#include <sys/types.h>
#include "../kernel/page_allocator.h"
#include "../kernel/vm.h"

namespace {
    constexpr size_t memorySize = 512 * 1024;
    constexpr auto memorySizeInPages = memorySize / vm::PageSize;

    constexpr size_t extraMemorySize = 1024 * 1024;
    constexpr auto extraMemorySizeInPages = extraMemorySize / vm::PageSize;

    std::vector<void*> AllocateNumberOfPages(size_t amount)
    {
        std::vector<void*> pages;
        for(size_t n = 0; n < amount; ++n) {
            void* p = page_allocator::Allocate();
            EXPECT_NE(nullptr, p);
            pages.push_back(p);
        }
        return pages;
    }

    template<typename Container>
    void FreePages(const Container& container)
    {
        std::for_each(container.begin(), container.end(), [](auto p) {
            page_allocator::Free(p);
        });
    }

    template<typename Container>
    bool AreThereOnlyUniqueElementsInVector(const Container& container)
    {
        auto items{container};
        std::sort(items.begin(), items.end());
        auto last = std::unique(items.begin(), items.end());
        return last == items.end();
    }

    template<typename T> struct Range { T min_inclusive; T max_exclusive; };

    template<typename ElementContainer, typename RangeContainer>
    bool AreAllElementsWithinRange(const ElementContainer& elements, const RangeContainer& ranges)
    {
        return std::all_of(elements.begin(), elements.end(), [ranges](auto v) {
            return std::any_of(ranges.begin(), ranges.end(), [v](auto range) {
                return v >= range.min_inclusive && v < range.max_exclusive;
            });
        });
    }

    void AllocateAndRegisterMemoryRegion(std::unique_ptr<char[]>& memory, const size_t sizeInBytes)
    {
        memory = std::make_unique<char[]>(sizeInBytes);
        page_allocator::RegisterMemory(reinterpret_cast<uint64_t>(memory.get()), sizeInBytes / vm::PageSize);
    }

    struct PageAllocator : ::testing::Test
    {
        PageAllocator() {
            page_allocator::Initialize();
            AllocateAndRegisterMemoryRegion(memory, memorySize);
        }

        ~PageAllocator() {
            // Ensure we don't leak any usable pages
            page_allocator::Initialize();
        }

        void AddExtraMemory()
        {
            AllocateAndRegisterMemoryRegion(extraMemory, extraMemorySize);
        }

        std::unique_ptr<char[]> memory;
        std::unique_ptr<char[]> extraMemory;
    };
}

TEST(PageAllocatorTest, AreThereOnlyUniqueElementsInVector)
{
    EXPECT_TRUE(AreThereOnlyUniqueElementsInVector(std::array<int, 0>{ }));
    EXPECT_TRUE(AreThereOnlyUniqueElementsInVector(std::array{ 1 }));
    EXPECT_FALSE(AreThereOnlyUniqueElementsInVector(std::array{ 1, 1 }));
    EXPECT_TRUE(AreThereOnlyUniqueElementsInVector(std::array{ 9, 3, 4 }));
    EXPECT_FALSE(AreThereOnlyUniqueElementsInVector(std::array{ 9, 3, 4, 4, 9, 3 }));
}

TEST(PageAllocatorTest, AreAllElementsWithinRange_Single_Range)
{
    constexpr std::array range{ Range<int>{ 10, 15 } };
    EXPECT_TRUE(AreAllElementsWithinRange(std::array<int, 0>{ }, range));
    EXPECT_TRUE(AreAllElementsWithinRange(std::array{ 10 }, range));
    EXPECT_TRUE(AreAllElementsWithinRange(std::array{ 14 }, range));
    EXPECT_TRUE(AreAllElementsWithinRange(std::array{ 10, 11, 12, 13, 14 }, range));
    EXPECT_FALSE(AreAllElementsWithinRange(std::array{ 9 }, range));
    EXPECT_FALSE(AreAllElementsWithinRange(std::array{ 15 }, range));
    EXPECT_FALSE(AreAllElementsWithinRange(std::array{ 0, 10, 11, 12, 13, 14, 15 }, range));
}

TEST(PageAllocatorTest, AreAllElementsWithinRange_Multiple_Ranges)
{
    constexpr std::array ranges{ Range<int>{ 3, 7 }, Range<int>{ 17, 20 } };
    EXPECT_TRUE(AreAllElementsWithinRange(std::array<int, 0>{ }, ranges));
    EXPECT_TRUE(AreAllElementsWithinRange(std::array{ 3, 4, 19 }, ranges));
    EXPECT_TRUE(AreAllElementsWithinRange(std::array{ 3, 4, 5, 6, 17, 18, 19 }, ranges));
    EXPECT_FALSE(AreAllElementsWithinRange(std::array{ 13 }, ranges));
    EXPECT_FALSE(AreAllElementsWithinRange(std::array{ 3, 4, 5, 6, 7, 17, 18, 19 }, ranges));
    EXPECT_FALSE(AreAllElementsWithinRange(std::array{ 3, 4, 5, 6, 17, 18, 19, 20 }, ranges));
}

TEST_F(PageAllocator, Initialize)
{
    EXPECT_EQ(memorySizeInPages, page_allocator::GetNumberOfAvailablePages());
}

TEST_F(PageAllocator, Allocate_Page_From_Single_Region)
{
    void* p = page_allocator::Allocate();
    EXPECT_EQ(memorySizeInPages - 1, page_allocator::GetNumberOfAvailablePages());
}

TEST_F(PageAllocator, Allocate_And_Free_Page_From_Single_Region)
{
    void* p = page_allocator::Allocate();
    EXPECT_EQ(memorySizeInPages - 1, page_allocator::GetNumberOfAvailablePages());
    page_allocator::Free(p);
    EXPECT_EQ(memorySizeInPages, page_allocator::GetNumberOfAvailablePages());
}

TEST_F(PageAllocator, Allocate_All_Pages_From_Single_Region)
{
    auto pages = AllocateNumberOfPages(memorySizeInPages);
    EXPECT_EQ(0, page_allocator::GetNumberOfAvailablePages());
    EXPECT_EQ(nullptr, page_allocator::Allocate());
    EXPECT_TRUE(AreThereOnlyUniqueElementsInVector(pages));
    const std::vector<Range<void*>> ranges{ { memory.get(), memory.get() + memorySize } };
    EXPECT_TRUE(AreAllElementsWithinRange(pages, ranges));
}

TEST_F(PageAllocator, Allocate_And_Free_All_Pages_From_Single_Region)
{
    auto pages = AllocateNumberOfPages(memorySizeInPages);
    FreePages(pages);
    EXPECT_EQ(memorySizeInPages, page_allocator::GetNumberOfAvailablePages());
}

TEST_F(PageAllocator, Allocate_All_Pages_From_Multiple_Regions)
{
    AddExtraMemory();
    auto pages = AllocateNumberOfPages(memorySizeInPages + extraMemorySizeInPages);
    EXPECT_EQ(0, page_allocator::GetNumberOfAvailablePages());
    EXPECT_EQ(nullptr, page_allocator::Allocate());
    EXPECT_TRUE(AreThereOnlyUniqueElementsInVector(pages));
    const std::vector<Range<void*>> ranges{
        { memory.get(), memory.get() + memorySize },
        { extraMemory.get(), extraMemory.get() + extraMemorySize },
    };
    EXPECT_TRUE(AreAllElementsWithinRange(pages, ranges));
}

TEST_F(PageAllocator, Allocate_And_Free_All_Pages_From_Multiple_Regions)
{
    AddExtraMemory();
    auto pages = AllocateNumberOfPages(memorySizeInPages + extraMemorySizeInPages);

    FreePages(pages);
    EXPECT_EQ(memorySizeInPages + extraMemorySizeInPages, page_allocator::GetNumberOfAvailablePages());
}
