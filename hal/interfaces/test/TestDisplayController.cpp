#include "hal/interfaces/test_doubles/DisplayControllerStub.hpp"
#include "gtest/gtest.h"
#include <array>
#include <cstdint>

namespace
{
    constexpr hal::DisplaySize panelSize{ 480, 272 };
    constexpr hal::DisplaySize layerSize{ 16, 8 };
    constexpr uint32_t layerStride = 32;

    class DisplayControllerTest
        : public testing::Test
    {
    public:
        void Start()
        {
            EXPECT_CALL(stub, Start(testing::_, testing::_));

            controller.Start(
                [this]()
                {
                    ++verticalBlanks;
                },
                [this]()
                {
                    ++underruns;
                });
        }

        hal::DisplayLayer Layer(infra::ByteRange memory)
        {
            return { { memory, layerSize, layerStride, hal::SurfaceFormat::rgb565 }, 10, 20, hal::BlendMode::pixelAlpha, 128 };
        }

        void Configure(std::size_t layer, infra::ByteRange memory)
        {
            EXPECT_CALL(stub, ConfigureLayer(layer, testing::_));
            controller.ConfigureLayer(layer, Layer(memory));
        }

        void Commit()
        {
            EXPECT_CALL(stub, Commit(testing::_));
            controller.Commit([this]()
                {
                    ++applied;
                });
        }

        testing::StrictMock<hal::DisplayControllerStub> stub{ panelSize, 2 };
        hal::DisplayController& controller{ stub };
        std::array<uint8_t, layerStride * 8> first{};
        std::array<uint8_t, layerStride * 8> second{};
        int verticalBlanks{ 0 };
        int underruns{ 0 };
        int applied{ 0 };
    };
}

TEST_F(DisplayControllerTest, size_and_layer_count_are_reported)
{
    EXPECT_CALL(stub, Size());
    EXPECT_CALL(stub, NumberOfLayers());

    EXPECT_EQ(panelSize, controller.Size());
    EXPECT_EQ(std::size_t(2), controller.NumberOfLayers());
}

TEST_F(DisplayControllerTest, vertical_blank_is_not_reported_before_start)
{
    stub.VerticalBlank();

    EXPECT_EQ(0, verticalBlanks);
}

TEST_F(DisplayControllerTest, every_vertical_blank_is_reported_while_started)
{
    Start();

    stub.VerticalBlank();
    stub.VerticalBlank();

    EXPECT_EQ(2, verticalBlanks);
    EXPECT_TRUE(stub.Started());
}

TEST_F(DisplayControllerTest, vertical_blank_is_not_reported_after_stop)
{
    Start();
    EXPECT_CALL(stub, Stop());
    controller.Stop();

    stub.VerticalBlank();
    stub.Underrun();

    EXPECT_EQ(0, verticalBlanks);
    EXPECT_EQ(0, underruns);
    EXPECT_FALSE(stub.Started());
}

TEST_F(DisplayControllerTest, underrun_is_reported_while_started)
{
    Start();

    stub.Underrun();

    EXPECT_EQ(1, underruns);
    EXPECT_EQ(0, verticalBlanks);
}

TEST_F(DisplayControllerTest, a_restart_reports_to_the_new_callbacks)
{
    Start();
    EXPECT_CALL(stub, Stop());
    controller.Stop();
    int otherBlanks = 0;
    EXPECT_CALL(stub, Start(testing::_, testing::_));
    controller.Start(
        [&otherBlanks]()
        {
            ++otherBlanks;
        },
        []() {});

    stub.VerticalBlank();

    EXPECT_EQ(1, otherBlanks);
    EXPECT_EQ(0, verticalBlanks);
}

TEST_F(DisplayControllerTest, a_configured_layer_is_staged_until_the_commit_completes)
{
    Configure(0, first);

    ASSERT_TRUE(stub.StagedLayer(0));
    EXPECT_EQ(10, stub.StagedLayer(0)->x);
    EXPECT_EQ(20, stub.StagedLayer(0)->y);
    EXPECT_EQ(hal::BlendMode::pixelAlpha, stub.StagedLayer(0)->blendMode);
    EXPECT_EQ(128, stub.StagedLayer(0)->alpha);
    EXPECT_FALSE(stub.AppliedLayer(0));
}

TEST_F(DisplayControllerTest, commit_applies_the_staged_layers_together_and_reports_once)
{
    Configure(0, first);
    Configure(1, second);
    Commit();

    EXPECT_EQ(0, applied);
    EXPECT_TRUE(stub.CommitPending());
    EXPECT_FALSE(stub.AppliedLayer(0));
    EXPECT_FALSE(stub.AppliedLayer(1));

    stub.CompleteCommit();

    EXPECT_EQ(1, applied);
    EXPECT_FALSE(stub.CommitPending());
    ASSERT_TRUE(stub.AppliedLayer(0));
    ASSERT_TRUE(stub.AppliedLayer(1));
    EXPECT_EQ(first.data(), stub.AppliedLayer(0)->framebuffer.memory.begin());
    EXPECT_EQ(second.data(), stub.AppliedLayer(1)->framebuffer.memory.begin());
}

TEST_F(DisplayControllerTest, changes_after_a_commit_wait_for_the_next_commit)
{
    Configure(0, first);
    Commit();
    stub.CompleteCommit();

    EXPECT_CALL(stub, SetFramebuffer(0, testing::_));
    controller.SetFramebuffer(0, second);

    EXPECT_EQ(second.data(), stub.StagedLayer(0)->framebuffer.memory.begin());
    EXPECT_EQ(first.data(), stub.AppliedLayer(0)->framebuffer.memory.begin());

    Commit();
    stub.CompleteCommit();

    EXPECT_EQ(second.data(), stub.AppliedLayer(0)->framebuffer.memory.begin());
}

TEST_F(DisplayControllerTest, a_disabled_layer_is_removed_by_the_next_commit)
{
    Configure(0, first);
    Commit();
    stub.CompleteCommit();

    EXPECT_CALL(stub, DisableLayer(0));
    controller.DisableLayer(0);

    EXPECT_TRUE(stub.AppliedLayer(0));

    Commit();
    stub.CompleteCommit();

    EXPECT_FALSE(stub.AppliedLayer(0));
}

TEST_F(DisplayControllerTest, the_next_commit_can_be_issued_from_the_completion)
{
    Configure(0, first);
    EXPECT_CALL(stub, Commit(testing::_)).Times(2);
    controller.Commit([this]()
        {
            ++applied;
            controller.Commit([this]()
                {
                    applied += 10;
                });
        });

    stub.CompleteCommit();
    EXPECT_EQ(1, applied);
    EXPECT_TRUE(stub.CommitPending());

    stub.CompleteCommit();
    EXPECT_EQ(11, applied);
}

TEST_F(DisplayControllerTest, a_second_commit_while_one_is_in_flight_asserts)
{
    Commit();
    EXPECT_CALL(stub, Commit(testing::_)).Times(testing::AtLeast(0));

    EXPECT_DEATH(controller.Commit([]() {}), "");
}

TEST_F(DisplayControllerTest, the_palette_takes_effect_without_a_commit)
{
    std::array<hal::Argb8888, 2> palette{ 0xff000000, 0xffffffff };
    EXPECT_CALL(stub, SetPalette(1, testing::_));

    controller.SetPalette(1, palette);

    EXPECT_EQ(palette.data(), stub.Palette(1).begin());
    EXPECT_EQ(std::size_t(2), stub.Palette(1).size());
}

TEST_F(DisplayControllerTest, a_layer_outside_the_controller_asserts)
{
    EXPECT_CALL(stub, DisableLayer(2)).Times(testing::AtLeast(0));

    EXPECT_DEATH(controller.DisableLayer(2), "");
}

TEST_F(DisplayControllerTest, a_frame_buffer_smaller_than_its_surface_asserts)
{
    EXPECT_CALL(stub, ConfigureLayer(0, testing::_)).Times(testing::AtLeast(0));

    EXPECT_DEATH(controller.ConfigureLayer(0, Layer(infra::Head(infra::ByteRange(first), 8))), "");
}
