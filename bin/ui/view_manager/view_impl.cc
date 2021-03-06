// Copyright 2015 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "garnet/bin/ui/view_manager/view_impl.h"

#include "garnet/bin/ui/view_manager/view_registry.h"
#include "garnet/bin/ui/view_manager/view_state.h"

namespace view_manager {

ViewImpl::ViewImpl(ViewRegistry* registry, ViewState* state)
    : registry_(registry), state_(state) {}

ViewImpl::~ViewImpl() {}

void ViewImpl::GetServiceProvider(
    fidl::InterfaceRequest<fuchsia::sys::ServiceProvider>
        service_provider_request) {
  service_provider_bindings_.AddBinding(this,
                                        std::move(service_provider_request));
}

void ViewImpl::OfferServiceProvider(
    fidl::InterfaceHandle<fuchsia::sys::ServiceProvider> service_provider,
    fidl::VectorPtr<fidl::StringPtr> service_names) {
  state_->SetServiceProvider(std::move(service_provider),
                             std::move(service_names));
}

void ViewImpl::GetContainer(
    fidl::InterfaceRequest<::fuchsia::ui::viewsv1::ViewContainer>
        view_container_request) {
  container_bindings_.AddBinding(this, std::move(view_container_request));
}

void ViewImpl::SetListener(
    fidl::InterfaceHandle<::fuchsia::ui::viewsv1::ViewContainerListener>
        listener) {
  state_->set_view_container_listener(listener.Bind());
}

void ViewImpl::AddChild(
    uint32_t child_key,
    fidl::InterfaceHandle<::fuchsia::ui::viewsv1token::ViewOwner>
        child_view_owner,
    zx::eventpair host_import_token) {
  // "Cast" the ViewOwner channel endpoint to an eventpair endpoint.  Should
  // work for the time being while this interface is being deprecated.
  // TODO(SCN-1018): Remove this along with the interface.
  AddChild2(child_key, zx::eventpair(child_view_owner.TakeChannel().release()),
            std::move(host_import_token));
}

void ViewImpl::AddChild2(uint32_t child_key, zx::eventpair view_holder_token,
                         zx::eventpair host_import_token) {
  registry_->AddChild(state_, child_key, std::move(view_holder_token),
                      std::move(host_import_token));
}

void ViewImpl::RemoveChild(
    uint32_t child_key,
    fidl::InterfaceRequest<::fuchsia::ui::viewsv1token::ViewOwner>
        transferred_view_owner_request) {
  // "Cast" the ViewOwner channel endpoint to an eventpair endpoint.  Should
  // work for the time being while this interface is being deprecated.
  // TODO(SCN-1018): Remove this along with the interface.
  RemoveChild2(
      child_key,
      zx::eventpair(transferred_view_owner_request.TakeChannel().release()));
}

void ViewImpl::RemoveChild2(uint32_t child_key,
                            zx::eventpair transferred_view_holder_token) {
  registry_->RemoveChild(state_, child_key,
                         std::move(transferred_view_holder_token));
}

void ViewImpl::SetChildProperties(
    uint32_t child_key,
    ::fuchsia::ui::viewsv1::ViewPropertiesPtr child_view_properties) {
  registry_->SetChildProperties(state_, child_key,
                                std::move(child_view_properties));
}

void ViewImpl::SendSizeChangeHintHACK(uint32_t child_key,
                                      float width_change_factor,
                                      float height_change_factor) {
  registry_->SendSizeChangeHintHACK(state_, child_key, width_change_factor,
                                    height_change_factor);
};

void ViewImpl::RequestFocus(uint32_t child_key) {
  registry_->RequestFocus(state_, child_key);
}

void ViewImpl::RequestSnapshotHACK(uint32_t child_key,
                                   RequestSnapshotHACKCallback callback) {
  registry_->RequestSnapshotHACK(state_, child_key, std::move(callback));
}

void ViewImpl::ConnectToService(fidl::StringPtr service_name,
                                zx::channel client_handle) {
  registry_->ConnectToViewService(state_, service_name,
                                  std::move(client_handle));
}

}  // namespace view_manager
