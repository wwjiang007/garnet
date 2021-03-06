// Copyright 2015 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GARNET_BIN_UI_ROOT_PRESENTER_PRESENTATION1_H_
#define GARNET_BIN_UI_ROOT_PRESENTER_PRESENTATION1_H_

#include <map>
#include <memory>

#include <fuchsia/accessibility/cpp/fidl.h>
#include <fuchsia/math/cpp/fidl.h>
#include <fuchsia/ui/input/cpp/fidl.h>
#include <fuchsia/ui/policy/cpp/fidl.h>
#include <fuchsia/ui/viewsv1/cpp/fidl.h>
#include <lib/fit/function.h>
#include <zx/eventpair.h>

#include "garnet/bin/ui/presentation_mode/detector.h"
#include "garnet/bin/ui/root_presenter/display_rotater.h"
#include "garnet/bin/ui/root_presenter/display_size_switcher.h"
#include "garnet/bin/ui/root_presenter/display_usage_switcher.h"
#include "garnet/bin/ui/root_presenter/displays/display_metrics.h"
#include "garnet/bin/ui/root_presenter/displays/display_model.h"
#include "garnet/bin/ui/root_presenter/perspective_demo_mode.h"
#include "garnet/bin/ui/root_presenter/presentation.h"
#include "garnet/bin/ui/root_presenter/presentation_switcher.h"
#include "garnet/bin/ui/root_presenter/renderer_params.h"
#include "lib/component/cpp/startup_context.h"
#include "lib/fidl/cpp/binding.h"
#include "lib/fxl/macros.h"
#include "lib/fxl/memory/weak_ptr.h"
#include "lib/ui/input/device_state.h"
#include "lib/ui/input/input_device_impl.h"
#include "lib/ui/scenic/cpp/resources.h"
#if defined(countof)
// Workaround for compiler error due to Zircon defining countof() as a macro.
// Redefines countof() using GLM_COUNTOF(), which currently provides a more
// sophisticated implementation anyway.
#undef countof
#include <glm/glm.hpp>
#define countof(X) GLM_COUNTOF(X)
#else
// No workaround required.
#include <glm/glm.hpp>
#endif

namespace root_presenter {

// This class creates a view tree and sets up rendering of a new scene to
// display the graphical content of the view passed to |PresentScene()|.  It
// also wires up input dispatch and manages the mouse cursor.
//
// Handles viewsv1 and will be deprecated soon.
//
// The view tree consists of a root view which is implemented by this class
// and which has the presented (content) view as its child.
//
// The scene's node tree has the following structure:
// + Scene
//   + RootViewHost
//     + link: root_view_host_import_token
//       + RootView's view manager stub
//         + link: root_view_parent_export_token
//           + RootView
//             + link: content_view_host_import_token
//               + child: ContentViewHost
//           + link: Content view's actual content
//   + child: cursor 1
//   + child: cursor N
class Presentation1 : private ::fuchsia::ui::viewsv1::ViewTreeListener,
                      private ::fuchsia::ui::viewsv1::ViewListener,
                      private ::fuchsia::ui::viewsv1::ViewContainerListener,
                      public Presentation {
 public:
  Presentation1(::fuchsia::ui::viewsv1::ViewManager* view_manager,
                fuchsia::ui::scenic::Scenic* scenic, scenic::Session* session,
                scenic::ResourceId compositor_id,
                RendererParams renderer_params,
                int32_t display_startup_rotation_adjustment,
                component::StartupContext* startup_context);

  ~Presentation1() override;

  // Present the specified view.
  // Invokes the callback if an error occurs.
  // This method must be called at most once for the lifetime of the
  // presentation.
  void Present(zx::eventpair view_owner_token,
               fidl::InterfaceRequest<fuchsia::ui::policy::Presentation>
                   presentation_request,
               YieldCallback yield_callback,
               ShutdownCallback shutdown_callback);

  void OnReport(uint32_t device_id,
                fuchsia::ui::input::InputReport report) override;
  void OnDeviceAdded(mozart::InputDeviceImpl* input_device) override;
  void OnDeviceRemoved(uint32_t device_id) override;

  const scenic::Layer& layer() const override { return layer_; }

  const YieldCallback& yield_callback() override { return yield_callback_; };

  float display_rotation_desired() const override {
    return display_rotation_desired_;
  };

  void set_display_rotation_desired(float display_rotation) override {
    display_rotation_desired_ = display_rotation;
  }

  float display_rotation_current() const override {
    return display_rotation_current_;
  }

  int32_t display_startup_rotation_adjustment() const override {
    return display_startup_rotation_adjustment_;
  }

  const DisplayModel::DisplayInfo& actual_display_info() override {
    return display_model_actual_.display_info();
  }

  const DisplayMetrics& simulated_display_metrics() const override {
    return display_metrics_;
  };

  scenic::Camera* camera() override { return &camera_; }

  // |HACK_InputPath|
  void HACK_SetInputPath(bool use_legacy) override {
    HACK_legacy_input_path_ = use_legacy;
  }

  // |HACK_InputPath|
  void HACK_QueryInputPath(HACK_QueryInputPathCallback callback) override {
    callback(HACK_legacy_input_path_);
  }

 private:
  enum SessionPresentState {
    kNoPresentPending,
    kPresentPending,
    kPresentPendingAndSceneDirty
  };
  friend class DisplayRotater;
  friend class DisplayUsageSwitcher;
  friend class PerspectiveDemoMode;
  friend class DisplaySizeSwitcher;
  friend class PresentationSwitcher;

  // Sets |display_metrics_| and updates view_manager and Scenic.
  // Returns false if the updates were skipped (if display initialization hasn't
  // happened yet).
  bool ApplyDisplayModelChanges(bool print_log, bool present_changes) override;

  bool ApplyDisplayModelChangesHelper(bool print_log);

  // |ViewContainerListener|:
  void OnChildAttached(uint32_t child_key,
                       ::fuchsia::ui::viewsv1::ViewInfo child_view_info,
                       OnChildAttachedCallback callback) override;
  void OnChildUnavailable(uint32_t child_key,
                          OnChildUnavailableCallback callback) override;

  // |ViewListener|:
  void OnPropertiesChanged(::fuchsia::ui::viewsv1::ViewProperties properties,
                           OnPropertiesChangedCallback callback) override;

  // |Presentation|
  void EnableClipping(bool enabled) override;

  // |Presentation|
  void UseOrthographicView() override;

  // |Presentation|
  void UsePerspectiveView() override;

  // |Presentation|
  void SetRendererParams(
      ::fidl::VectorPtr<fuchsia::ui::gfx::RendererParam> params) override;

  // Used internally by Presenter. Allows overriding of renderer params.
  void OverrideRendererParams(RendererParams renderer_params,
                              bool present_changes = true) override;

  void InitializeDisplayModel(fuchsia::ui::gfx::DisplayInfo display_info);

  // |Presentation|
  void SetDisplayUsage(fuchsia::ui::policy::DisplayUsage usage) override;

  void SetDisplayUsageWithoutApplyingChanges(
      fuchsia::ui::policy::DisplayUsage usage_) override;

  // |Presentation|
  void SetDisplaySizeInMm(float width_in_mm, float height_in_mm) override;

  // |Presentation|
  void SetDisplayRotation(float display_rotation_degrees,
                          bool animate) override;

  // Returns false if the operation failed (e.g. the requested size is bigger
  // than the actual display size).
  bool SetDisplaySizeInMmWithoutApplyingChanges(float width_in_mm,
                                                float height_in_mm,
                                                bool print_errors) override;

  // |Presentation|
  void CaptureKeyboardEventHACK(
      fuchsia::ui::input::KeyboardEvent event_to_capture,
      fidl::InterfaceHandle<fuchsia::ui::policy::KeyboardCaptureListenerHACK>
          listener) override;

  // |Presentation|
  void CapturePointerEventsHACK(
      fidl::InterfaceHandle<fuchsia::ui::policy::PointerCaptureListenerHACK>
          listener) override;

  // |Presentation|
  void GetPresentationMode(GetPresentationModeCallback callback) override;

  // |Presentation|
  void SetPresentationModeListener(
      fidl::InterfaceHandle<fuchsia::ui::policy::PresentationModeListener>
          listener) override;

  void CreateViewTree(zx::eventpair view_owner_token,
                      fidl::InterfaceRequest<fuchsia::ui::policy::Presentation>
                          presentation_request,
                      fuchsia::ui::gfx::DisplayInfo display_info);

  // Returns true if the event was consumed and the scene is to be invalidated.
  bool GlobalHooksHandleEvent(const fuchsia::ui::input::InputEvent& event);

  void OnEvent(fuchsia::ui::input::InputEvent event);
  void OnSensorEvent(uint32_t device_id, fuchsia::ui::input::InputReport event);

  // Checks for whether to send an input event through regular dispatch or
  // accessibility input dispatch.
  void OnAccessibilityEvent(fuchsia::ui::input::InputEvent event);
  // Enable or disable accessibility support in this presentation.
  // Event handler for |a11y_toggle_.events().OnAccessibilityToggle|.
  void OnAccessibilityToggle(bool enabled);

  void PresentScene();
  void Shutdown();

  ::fuchsia::ui::viewsv1::ViewManager* const view_manager_;
  fuchsia::ui::scenic::Scenic* const scenic_;
  scenic::Session* const session_;
  scenic::ResourceId compositor_id_;

  scenic::Layer layer_;
  scenic::Renderer renderer_;
  // TODO(MZ-254): put camera before scene.
  scenic::Scene scene_;
  scenic::Camera camera_;
  scenic::AmbientLight ambient_light_;
  glm::vec3 light_direction_;
  scenic::DirectionalLight directional_light_;
  scenic::EntityNode root_view_host_node_;
  zx::eventpair root_view_host_import_token_;
  scenic::ImportNode root_view_parent_node_;
  zx::eventpair root_view_parent_export_token_;
  scenic::EntityNode content_view_host_node_;
  zx::eventpair content_view_host_import_token_;
  scenic::RoundedRectangle cursor_shape_;
  scenic::Material cursor_material_;

  SessionPresentState session_present_state_ = kNoPresentPending;

  bool display_model_initialized_ = false;

  DisplayModel display_model_actual_;
  DisplayModel display_model_simulated_;

  // Stores values that, if set, override any renderer params.
  bool presentation_clipping_enabled_ = true;
  RendererParams renderer_params_override_;

  // When |display_model_simulated_| or |display_rotation_desired_| changes:
  //   * |display_metrics_| must be recalculated.
  //   * |display_rotation_current_| must be updated.
  //   * Transforms on the scene must be updated.
  // This is done by calling ApplyDisplayModelChanges().
  DisplayMetrics display_metrics_;

  // Expressed in degrees.
  float display_rotation_desired_ = 0.f;
  float display_rotation_current_ = 0.f;

  // At startup, apply a rotation defined in 90 degree increments, just once.
  // Implies resizing of the presentation to adjust to rotated coordinates.
  // Valid values are ... -180, -90, 0, 90, 180, ...
  //
  // Used when the native display orientation is reported incorrectly.
  // TODO(SCN-857) - Make this less of a hack.
  int32_t display_startup_rotation_adjustment_;

  ::fuchsia::ui::viewsv1::ViewPtr root_view_;

  YieldCallback yield_callback_;
  ShutdownCallback shutdown_callback_;

  fuchsia::math::PointF mouse_coordinates_;

  fidl::Binding<fuchsia::ui::policy::Presentation> presentation_binding_;
  fidl::Binding<::fuchsia::ui::viewsv1::ViewTreeListener>
      tree_listener_binding_;
  fidl::Binding<::fuchsia::ui::viewsv1::ViewContainerListener>
      tree_container_listener_binding_;
  fidl::Binding<::fuchsia::ui::viewsv1::ViewContainerListener>
      view_container_listener_binding_;
  fidl::Binding<::fuchsia::ui::viewsv1::ViewListener> view_listener_binding_;

  ::fuchsia::ui::viewsv1::ViewTreePtr tree_;
  ::fuchsia::ui::viewsv1::ViewContainerPtr tree_container_;
  ::fuchsia::ui::viewsv1::ViewContainerPtr root_container_;
  fuchsia::ui::input::InputDispatcherPtr input_dispatcher_;

  // Rotates the display 180 degrees in response to events.
  DisplayRotater display_rotater_;

  // Toggles through different display usage values.
  DisplayUsageSwitcher display_usage_switcher_;

  PerspectiveDemoMode perspective_demo_mode_;

  // Toggles through different display sizes.
  DisplaySizeSwitcher display_size_switcher_;

  // Toggles through different presentations.
  PresentationSwitcher presentation_switcher_;

  struct CursorState {
    bool created;
    bool visible;
    fuchsia::math::PointF position;
    std::unique_ptr<scenic::ShapeNode> node;
  };

  std::map<uint32_t, CursorState> cursors_;
  std::map<uint32_t, std::pair<mozart::InputDeviceImpl*,
                               std::unique_ptr<mozart::DeviceState>>>
      device_states_by_id_;

  // A registry of listeners who want to be notified when their keyboard
  // event happens.
  struct KeyboardCaptureItem {
    fuchsia::ui::input::KeyboardEvent event;
    fuchsia::ui::policy::KeyboardCaptureListenerHACKPtr listener;
  };
  std::vector<KeyboardCaptureItem> captured_keybindings_;

  // A registry of listeners who want to be notified when pointer event happens.
  struct PointerCaptureItem {
    fuchsia::ui::policy::PointerCaptureListenerHACKPtr listener;
  };
  std::vector<PointerCaptureItem> captured_pointerbindings_;

  // Listener for changes in presentation mode.
  fuchsia::ui::policy::PresentationModeListenerPtr presentation_mode_listener_;
  // Presentation mode, based on last N measurements
  fuchsia::ui::policy::PresentationMode presentation_mode_;
  std::unique_ptr<presentation_mode::Detector> presentation_mode_detector_;

  // Hooks for accessibility input dispatch.
  // Used to reconnect |a11y_input_connection_| once the presentation receives
  // input.
  component::StartupContext* startup_context_;
  fuchsia::accessibility::ToggleBroadcasterPtr a11y_toggle_;
  // Flag to allow connecting to |a11y_input_connection_| and piping input to
  // it. We currently leave no way to set this to true, while a11y
  // infrastructure is still in development.
  bool accessibility_mode_ = false;
  fuchsia::accessibility::InputReceiverPtr a11y_input_connection_;
  // We store the view tree token to pass to |a11y_input_connection_| on
  // registration.
  fuchsia::ui::viewsv1::ViewTreeToken current_view_tree_;

  bool HACK_legacy_input_path_;

  fxl::WeakPtrFactory<Presentation1> weak_factory_;

  FXL_DISALLOW_COPY_AND_ASSIGN(Presentation1);
};

}  // namespace root_presenter

#endif  // GARNET_BIN_UI_ROOT_PRESENTER_PRESENTATION1_H_
