// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GARNET_DRIVERS_BLUETOOTH_LIB_DATA_DOMAIN_H_
#define GARNET_DRIVERS_BLUETOOTH_LIB_DATA_DOMAIN_H_

#include <fbl/ref_counted.h>
#include <fbl/ref_ptr.h>
#include <lib/zx/socket.h>

#include "garnet/drivers/bluetooth/lib/hci/transport.h"
#include "garnet/drivers/bluetooth/lib/l2cap/l2cap.h"
#include "lib/fxl/macros.h"

namespace btlib {
namespace data {

// Represents the task domain that implements the host subsystem's data plane.
// This domain owns its own thread on which data-path tasks are dispatched.
// Protocols implemented here are:
//
//   a. L2CAP and SCO.
//   b. RFCOMM.
//   c. Data sockets that bridge out-of-process users to above protocols.
//
// Interactions between the data domain and other library threads is performed
// primarily via message passing.
class Domain : public fbl::RefCounted<Domain> {
 public:
  // Constructs an uninitialized data domain that can be used in production.
  // This spawns a thread on which data-domain tasks will be scheduled.
  static fbl::RefPtr<Domain> Create(fxl::RefPtr<hci::Transport> hci,
                                    std::string thread_name);

  // Constructs an instance using the given |dispatcher| instead of spawning a
  // thread. This is intended for unit tests.
  static fbl::RefPtr<Domain> CreateWithDispatcher(
      fxl::RefPtr<hci::Transport> hci, async_dispatcher_t* dispatcher);

  // These send an Initialize/ShutDown message to the data task runner. It is
  // safe for the caller to drop its Domain reference after ShutDown is called.
  //
  // Operations on an uninitialized or shut-down Domain have no effect.
  virtual void Initialize() = 0;
  virtual void ShutDown() = 0;

  // Registers an ACL connection with the L2CAP layer. L2CAP channels can be
  // opened on the logical link represented by |handle| after a call to this
  // method.
  //
  // |link_error_callback| will be used to notify when a channel signals a link
  // error. It will be posted onto |dispatcher|.
  //
  // Has no effect if this Domain is uninitialized or shut down.
  virtual void AddACLConnection(hci::ConnectionHandle handle,
                                hci::Connection::Role role,
                                l2cap::LinkErrorCallback link_error_callback,
                                async_dispatcher_t* dispatcher) = 0;

  // Registers an LE connection with the L2CAP layer. L2CAP channels can be
  // opened on the logical link represented by |handle| after a call to this
  // method.
  //
  // |conn_param_callback| will be used to notify the caller if new connection
  // parameters were accepted from the remote end of the link.
  //
  // |link_error_callback| will be used to notify when a channel signals a link
  // error.
  //
  // Upon successful registration of the link, |channel_callback| will be called
  // with the ATT and SMP fixed channels.
  //
  // Has no effect if this Domain is uninitialized or shut down.
  virtual void AddLEConnection(
      hci::ConnectionHandle handle, hci::Connection::Role role,
      l2cap::LinkErrorCallback link_error_callback,
      l2cap::LEFixedChannelsCallback channel_callback,
      l2cap::LEConnectionParameterUpdateCallback conn_param_callback,
      async_dispatcher_t* dispatcher) = 0;

  // Removes a previously registered connection. All corresponding Channels will
  // be closed and all incoming data packets on this link will be dropped.
  //
  // NOTE: It is recommended that a link entry be removed AFTER the controller
  // sends a HCI Disconnection Complete Event for the corresponding logical
  // link. This is to prevent incorrectly buffering data if the controller has
  // more packets to send after removing the link entry.
  //
  // Has no effect if this Domain is uninitialized or shut down.
  virtual void RemoveConnection(hci::ConnectionHandle handle) = 0;

  // Open an outbound dynamic channel against a peer's Protocol/Service
  // Multiplexing (PSM) code |psm| on a link identified by |handle|.
  //
  // |cb| will be called on |dispatcher| with the channel created to the remote,
  // or nullptr if the channel creation resulted in an error.
  //
  // Has no effect if this Domain is uninitialized or shut down.
  virtual void OpenL2capChannel(hci::ConnectionHandle handle, l2cap::PSM psm,
                                l2cap::ChannelCallback cb,
                                async_dispatcher_t* dispatcher) = 0;

  // Registers a handler for peer-initiated dynamic channel requests that have
  // the Protocol/Service Multiplexing (PSM) code |psm|.
  //
  // |cb| will be called on |dispatcher| with the channel created by each
  // inbound connection request received. Handlers must be unregistered before
  // they are replaced.
  //
  // Returns false if |psm| is invalid or already has a handler registered.
  //
  // Inbound connection requests with a PSM that has no registered handler will
  // be rejected.
  //
  // Has no effect if this Domain is uninitialized or shut down.
  //
  // TODO(xow): NET-1084 Pass in required channel configurations. Call signature
  //            will likely change.
  // TODO(xow): Dynamic PSMs may need their routing space (ACL or LE) identified
  virtual void RegisterService(l2cap::PSM psm, l2cap::ChannelCallback callback,
                               async_dispatcher_t* dispatcher) = 0;

  // Similar to RegisterService, but instead of providing a l2cap::Channel,
  // provides a zx::socket which can be used to communicate on the channel.
  // The underlying l2cap::Channel is activated; the socket provided will
  // receive any data sent to the channel and any data sent to the socket
  // will be sent as if sent by l2cap::Channel::Send.
  // |link_handle| disambiguates which remote device initiated the channel.
  //
  // TODO(armansito): Return the socket in a data structure that contains
  // additional meta-data about the connection, such as its link type and
  // channel configuration parameters (see NET-1084 and TODOs for
  // RegisterService above.
  using SocketCallback =
      fit::function<void(zx::socket, hci::ConnectionHandle link_handle)>;
  virtual void RegisterService(l2cap::PSM psm, SocketCallback socket_callback,
                               async_dispatcher_t* dispatcher) = 0;

  // Removes the handler for inbound channel requests for the previously-
  // registered service identified by |psm|. This only prevents new inbound
  // channels from being opened but does not close already-open channels.
  //
  // Has no effect if this Domain is uninitialized or shut down.
  virtual void UnregisterService(l2cap::PSM psm) = 0;

 protected:
  friend class fbl::RefPtr<Domain>;
  Domain() = default;
  virtual ~Domain() = default;

 private:
  FXL_DISALLOW_COPY_AND_ASSIGN(Domain);
};

}  // namespace data
}  // namespace btlib

#endif  // GARNET_DRIVERS_BLUETOOTH_LIB_DATA_DOMAIN_H_
