// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <fuchsia/media/cpp/fidl.h>

#include <lib/gtest/real_loop_fixture.h>

#include "garnet/bin/media/audio_core/test/audio_core_tests_shared.h"
#include "lib/component/cpp/environment_services_helper.h"

namespace media {
namespace audio {
namespace test {

//
// AudioRendererSync tests
//

// Base class for tests of the synchronous AudioRendererSync interface.
// We expect the async and sync interfaces to track each other exactly -- any
// behavior otherwise is a bug in core FIDL. These tests were only created to
// better understand how errors manifest themselves when using sync interfaces.
//
// In short, further testing of the sync interfaces (over and above any testing
// done on the async interfaces) should not be needed.
class AudioRendererSyncTest : public gtest::RealLoopFixture {
 protected:
  void SetUp() override {
    ::gtest::RealLoopFixture::SetUp();

    environment_services_ = component::GetEnvironmentServices();
    environment_services_->ConnectToService(audio_.NewRequest());
    ASSERT_TRUE(audio_);

    ASSERT_EQ(ZX_OK, audio_->CreateAudioRenderer(audio_renderer_.NewRequest()));
    ASSERT_TRUE(audio_renderer_);
  }

  std::shared_ptr<component::Services> environment_services_;
  fuchsia::media::AudioSyncPtr audio_;
  fuchsia::media::AudioRendererSyncPtr audio_renderer_;
};

class AudioRendererSyncTest_Negative : public AudioRendererSyncTest {};

// Basic validation of GetMinLeadTime() for the synchronous AudioRenderer.
// In subsequent synchronous-interface test(s), receiving a valid return value
// from this call is our only way of verifying that the connection survived.
TEST_F(AudioRendererSyncTest, GetMinLeadTime) {
  int64_t min_lead_time = -1;
  ASSERT_EQ(ZX_OK, audio_renderer_->GetMinLeadTime(&min_lead_time))
      << kConnectionErr;
  EXPECT_GE(min_lead_time, 0);
}

// Before renderers are operational, multiple SetPcmStreamTypes should succeed.
// We test twice because of previous bug, where the first succeeded but any
// subsequent call (before Play) would cause a FIDL channel disconnect.
// GetMinLeadTime is our way of verifying whether the connection survived.
TEST_F(AudioRendererSyncTest, SetPcmFormat) {
  fuchsia::media::AudioStreamType format;
  format.sample_format = fuchsia::media::AudioSampleFormat::FLOAT;
  format.channels = 2;
  format.frames_per_second = 48000;
  EXPECT_EQ(ZX_OK, audio_renderer_->SetPcmStreamType(std::move(format)));

  int64_t min_lead_time = -1;
  ASSERT_EQ(ZX_OK, audio_renderer_->GetMinLeadTime(&min_lead_time))
      << kConnectionErr;
  EXPECT_GE(min_lead_time, 0);

  fuchsia::media::AudioStreamType format2;
  format2.sample_format = fuchsia::media::AudioSampleFormat::SIGNED_16;
  format2.channels = 1;
  format2.frames_per_second = 44100;
  EXPECT_EQ(ZX_OK, audio_renderer_->SetPcmStreamType(std::move(format2)));

  min_lead_time = -1;
  EXPECT_EQ(ZX_OK, audio_renderer_->GetMinLeadTime(&min_lead_time));
  EXPECT_GE(min_lead_time, 0);
}

// Before setting format, PlayNoReply should cause a Disconnect.
// GetMinLeadTime is our way of verifying whether the connection survived.
TEST_F(AudioRendererSyncTest_Negative, PlayNoReplyWithoutFormat) {
  EXPECT_EQ(ZX_OK, audio_renderer_->PlayNoReply(fuchsia::media::NO_TIMESTAMP,
                                                fuchsia::media::NO_TIMESTAMP));

  int64_t min_lead_time = -1;
  EXPECT_EQ(ZX_ERR_PEER_CLOSED,
            audio_renderer_->GetMinLeadTime(&min_lead_time));
  // Although the connection has disconnected, the proxy should still exist.
  EXPECT_TRUE(audio_renderer_);
}

// Before setting format, PauseNoReply should cause a Disconnect.
// GetMinLeadTime is our way of verifying whether the connection survived.
TEST_F(AudioRendererSyncTest_Negative, PauseNoReplyWithoutFormat) {
  EXPECT_EQ(ZX_OK, audio_renderer_->PauseNoReply());

  int64_t min_lead_time = -1;
  EXPECT_EQ(ZX_ERR_PEER_CLOSED,
            audio_renderer_->GetMinLeadTime(&min_lead_time));
  // Although the connection has disconnected, the proxy should still exist.
  EXPECT_TRUE(audio_renderer_);
}

}  // namespace test
}  // namespace audio
}  // namespace media
