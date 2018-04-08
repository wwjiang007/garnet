// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GARNET_EXAMPLES_UI_HELLO_SPACES_APP_H_
#define GARNET_EXAMPLES_UI_HELLO_SPACES_APP_H_

#include <fuchsia/cpp/ui.h>

#include "lib/app/cpp/application_context.h"
#include "lib/fsl/tasks/message_loop.h"
#include "lib/ui/scenic/client/resources.h"
#include "lib/ui/scenic/client/session.h"

namespace hello_spaces {

enum class AppType : unsigned char { Controller, Guest };

class App {
 public:
  explicit App(AppType type);

  const char* AppIdentifier() const;

 private:
  // Called asynchronously when the session dies.
  void ReleaseSessionResources();

  // Updates and presents the scene.  Called first by Init().  Each invocation
  // schedules another call to Update() when the result of the previous
  // presentation is asynchronously received.
  void Update(uint64_t next_presentation_time);

  // Creates all of the scene resources and sets up the scene graph.
  void CreateScene(float display_width, float display_height);

  AppType type_;
  std::unique_ptr<component::ApplicationContext> application_context_;
  fsl::MessageLoop* loop_;

  ui::ScenicPtr scenic_;
  std::unique_ptr<scenic_lib::Session> session_;
  std::unique_ptr<scenic_lib::DisplayCompositor> compositor_;
  std::unique_ptr<scenic_lib::Camera> camera_;
  std::unique_ptr<ui::SpaceProvider> space_provider_;

  FXL_DISALLOW_COPY_AND_ASSIGN(App);
};

}  // namespace hello_spaces

#endif  // GARNET_EXAMPLES_UI_HELLO_SPACES_APP_H_
