#include "bootloader/ApplicationCheck.hpp"
#include "bootloader/test/TestImages.hpp"
#include <algorithm>
#include <gmock/gmock.h>
#include <vector>

namespace
{
    using namespace bootloader;
    using testing::Return;
    using testing::StrictMock;

    constexpr uint32_t slotSize = 8 * 1024;

    class TargetCheckMock
        : public TargetCheck
    {
    public:
        MOCK_METHOD(bool, Accepts, (const ApplicationHeader& header), (const, override));
    };

    MATCHER_P(NamesTarget, target, "")
    {
        return TargetOf<TestTarget>(arg) == target;
    }

    std::vector<uint8_t> Slot(const std::vector<uint8_t>& image)
    {
        return bootloader::Slot(image, slotSize);
    }

    constexpr TargetLayout SmallSlotLayout()
    {
        return TargetLayout{ testLayout.ramBegin, testLayout.ramEnd, 0x00008000, 0x00008000 + slotSize };
    }

    const ExactTargetCheck<TestTarget> thisTargetCheck{ thisTarget };
}

TEST(TestImageStart, an_image_for_this_target_fits)
{
    EXPECT_TRUE(ImageStartFitsTarget(infra::MakeRange(GoodImage()), testLayout, thisTargetCheck));
}

TEST(TestImageStart, a_stack_pointer_outside_this_chips_ram_is_refused)
{
    EXPECT_FALSE(ImageStartFitsTarget(infra::MakeRange(Image(thisTarget, 0x20080000, 0x00008441)), testLayout, thisTargetCheck));
}

TEST(TestImageStart, a_reset_handler_outside_this_boards_slot_is_refused)
{
    EXPECT_FALSE(ImageStartFitsTarget(infra::MakeRange(Image(thisTarget, 0x20040000, 0x08008441)), testLayout, thisTargetCheck));
}

TEST(TestImageStart, a_reset_handler_without_the_thumb_bit_is_refused)
{
    EXPECT_FALSE(ImageStartFitsTarget(infra::MakeRange(Image(thisTarget, 0x20040000, 0x00008440)), testLayout, thisTargetCheck));
}

TEST(TestImageStart, the_target_check_decides_on_the_header_of_the_image)
{
    StrictMock<TargetCheckMock> target;
    EXPECT_CALL(target, Accepts(NamesTarget(otherTarget))).WillOnce(Return(true));

    EXPECT_TRUE(ImageStartFitsTarget(infra::MakeRange(Image(otherTarget, 0x20040000, 0x00008441)), testLayout, target));
}

TEST(TestImageStart, an_image_the_target_check_refuses_is_refused)
{
    StrictMock<TargetCheckMock> target;
    EXPECT_CALL(target, Accepts(NamesTarget(thisTarget))).WillOnce(Return(false));

    EXPECT_FALSE(ImageStartFitsTarget(infra::MakeRange(GoodImage()), testLayout, target));
}

TEST(TestImageStart, an_image_without_an_application_header_is_refused_before_the_target_check)
{
    StrictMock<TargetCheckMock> target;
    auto image = GoodImage();
    image[applicationHeaderOffset] ^= 0xff;

    EXPECT_FALSE(ImageStartFitsTarget(infra::MakeRange(image), testLayout, target));
}

TEST(TestImageStart, an_image_with_another_header_version_is_refused_before_the_target_check)
{
    StrictMock<TargetCheckMock> target;
    auto image = GoodImage();
    image[applicationHeaderOffset + 4] ^= 0x02;

    EXPECT_FALSE(ImageStartFitsTarget(infra::MakeRange(image), testLayout, target));
}

TEST(TestImageStart, an_image_too_short_to_hold_the_header_is_refused)
{
    auto image = GoodImage();
    image.resize(applicationHeaderOffset + sizeof(ApplicationHeader) - 1);

    EXPECT_FALSE(ImageStartFitsTarget(infra::MakeRange(image), testLayout, thisTargetCheck));
}

TEST(TestApplicationHeader, the_target_fills_the_start_of_the_target_area_and_the_rest_reads_erased)
{
    const auto header = MakeApplicationHeader(thisTarget, 1, 7, 0);

    EXPECT_EQ(thisTarget, TargetOf<TestTarget>(header));
    EXPECT_TRUE(std::all_of(header.target.begin() + sizeof(TestTarget), header.target.end(), [](uint8_t byte)
        {
            return byte == 0xff;
        }));
}

TEST(TestApplicationHeader, the_header_carries_the_firmware_version)
{
    const auto header = MakeApplicationHeader(thisTarget, 1, 7, 3);

    EXPECT_EQ(ApplicationHeader::validMagic, header.magic);
    EXPECT_EQ(ApplicationHeader::currentVersion, header.headerVersion);
    EXPECT_EQ(1, header.versionMajor);
    EXPECT_EQ(7, header.versionMinor);
    EXPECT_EQ(3, header.versionPatch);
}

TEST(TestExactTargetCheck, accepts_the_same_target)
{
    EXPECT_TRUE(thisTargetCheck.Accepts(MakeApplicationHeader(thisTarget, 1, 7, 0)));
}

TEST(TestExactTargetCheck, refuses_another_board)
{
    EXPECT_FALSE(thisTargetCheck.Accepts(MakeApplicationHeader(TestTarget{ 3, thisTarget.family, thisTarget.device }, 1, 7, 0)));
}

TEST(TestExactTargetCheck, refuses_another_family)
{
    EXPECT_FALSE(thisTargetCheck.Accepts(MakeApplicationHeader(TestTarget{ thisTarget.board, 2, thisTarget.device }, 1, 7, 0)));
}

TEST(TestExactTargetCheck, refuses_another_device)
{
    EXPECT_FALSE(thisTargetCheck.Accepts(MakeApplicationHeader(TestTarget{ thisTarget.board, thisTarget.family, 0x05 }, 1, 7, 0)));
}

TEST(TestStartable, an_intact_application_for_this_target_is_startable)
{
    const auto slot = Slot(GoodImage());

    EXPECT_TRUE(ApplicationIsStartable(infra::MakeRange(slot), SmallSlotLayout(), thisTargetCheck));
}

TEST(TestStartable, a_changed_byte_in_the_image_is_not_startable)
{
    auto slot = Slot(GoodImage());
    slot[2000] ^= 0x01;

    EXPECT_FALSE(ApplicationIsStartable(infra::MakeRange(slot), SmallSlotLayout(), thisTargetCheck));
}

TEST(TestStartable, an_erased_descriptor_is_not_startable)
{
    auto slot = Slot(GoodImage());
    std::fill(slot.end() - imageDescriptorSize, slot.end(), 0xff);

    EXPECT_FALSE(ApplicationIsStartable(infra::MakeRange(slot), SmallSlotLayout(), thisTargetCheck));
}

TEST(TestStartable, a_descriptor_size_beyond_the_slot_is_not_startable)
{
    auto slot = Slot(GoodImage());
    Put32(slot, slot.size() - imageDescriptorSize + 4, slotSize);

    EXPECT_FALSE(ApplicationIsStartable(infra::MakeRange(slot), SmallSlotLayout(), thisTargetCheck));
}

TEST(TestStartable, an_intact_image_for_another_target_is_not_startable)
{
    const auto slot = Slot(Image(otherTarget, 0x20040000, 0x00008441));

    EXPECT_FALSE(ApplicationIsStartable(infra::MakeRange(slot), SmallSlotLayout(), thisTargetCheck));
}

TEST(TestStartable, the_descriptor_covers_exactly_the_image_size)
{
    const auto image = GoodImage();
    const auto descriptor = MakeImageDescriptor(infra::MakeRange(image));

    EXPECT_EQ(ImageDescriptor::validMagic, descriptor.magic);
    EXPECT_EQ(image.size(), descriptor.imageSize);
}
