// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "garnet/bin/zxdb/client/system_impl.h"

#include "garnet/bin/zxdb/client/breakpoint_impl.h"
#include "garnet/bin/zxdb/client/job_context_impl.h"
#include "garnet/bin/zxdb/client/process_impl.h"
#include "garnet/bin/zxdb/client/remote_api.h"
#include "garnet/bin/zxdb/client/session.h"
#include "garnet/bin/zxdb/client/system_observer.h"
#include "garnet/bin/zxdb/client/target_impl.h"
#include "garnet/lib/debug_ipc/helper/message_loop.h"

namespace zxdb {

SystemImpl::SystemImpl(Session* session)
    : System(session), weak_factory_(this) {
  AddNewTarget(std::make_unique<TargetImpl>(this));
  AddNewJobContext(std::make_unique<JobContextImpl>(this));

  // Forward all messages from the symbol index to our observers. It's OK to
  // bind |this| because the symbol index is owned by |this|.
  symbols_.build_id_index().set_information_callback(
      [this](const std::string& msg) {
        for (auto& observer : observers())
          observer.OnSymbolIndexingInformation(msg);
      });
}

SystemImpl::~SystemImpl() {
  // Target destruction may depend on the symbol system. Ensure the targets
  // get cleaned up first.
  for (auto& target : targets_) {
    // It's better if process destruction notifications are sent before target
    // ones because the target owns the process. Because this class sends the
    // target notifications, force the process destruction before doing
    // anything.
    target->ImplicitlyDetach();
    for (auto& observer : observers())
      observer.WillDestroyTarget(target.get());
  }
  targets_.clear();
}

ProcessImpl* SystemImpl::ProcessImplFromKoid(uint64_t koid) const {
  for (const auto& target : targets_) {
    ProcessImpl* process = target->process();
    if (process && process->GetKoid() == koid)
      return process;
  }
  return nullptr;
}

void SystemImpl::NotifyDidCreateProcess(Process* process) {
  for (auto& observer : observers())
    observer.GlobalDidCreateProcess(process);
}

void SystemImpl::NotifyWillDestroyProcess(Process* process) {
  for (auto& observer : observers())
    observer.GlobalWillDestroyProcess(process);
}

std::vector<TargetImpl*> SystemImpl::GetTargetImpls() const {
  std::vector<TargetImpl*> result;
  for (const auto& t : targets_)
    result.push_back(t.get());
  return result;
}

SystemSymbols* SystemImpl::GetSymbols() { return &symbols_; }

std::vector<Target*> SystemImpl::GetTargets() const {
  std::vector<Target*> result;
  result.reserve(targets_.size());
  for (const auto& t : targets_)
    result.push_back(t.get());
  return result;
}

std::vector<JobContext*> SystemImpl::GetJobContexts() const {
  std::vector<JobContext*> result;
  result.reserve(job_contexts_.size());
  for (const auto& t : job_contexts_)
    result.push_back(t.get());
  return result;
}

std::vector<Breakpoint*> SystemImpl::GetBreakpoints() const {
  std::vector<Breakpoint*> result;
  result.reserve(breakpoints_.size());
  for (const auto& pair : breakpoints_) {
    if (!pair.second->is_internal())
      result.push_back(pair.second.get());
  }
  return result;
}

Process* SystemImpl::ProcessFromKoid(uint64_t koid) const {
  return ProcessImplFromKoid(koid);
}

void SystemImpl::GetProcessTree(ProcessTreeCallback callback) {
  session()->remote_api()->ProcessTree(debug_ipc::ProcessTreeRequest(),
                                       std::move(callback));
}

Target* SystemImpl::CreateNewTarget(Target* clone) {
  auto target = clone ? static_cast<TargetImpl*>(clone)->Clone(this)
                      : std::make_unique<TargetImpl>(this);
  Target* to_return = target.get();
  AddNewTarget(std::move(target));
  return to_return;
}

JobContext* SystemImpl::CreateNewJobContext(JobContext* clone) {
  auto job_context = clone ? static_cast<JobContextImpl*>(clone)->Clone(this)
                           : std::make_unique<JobContextImpl>(this);
  JobContext* to_return = job_context.get();
  AddNewJobContext(std::move(job_context));
  return to_return;
}

Breakpoint* SystemImpl::CreateNewBreakpoint() {
  auto owning = std::make_unique<BreakpointImpl>(session(), false);
  uint32_t id = owning->backend_id();
  Breakpoint* to_return = owning.get();

  breakpoints_[id] = std::move(owning);

  // Notify observers (may mutate breakpoint list).
  for (auto& observer : observers())
    observer.DidCreateBreakpoint(to_return);

  return to_return;
}

Breakpoint* SystemImpl::CreateNewInternalBreakpoint() {
  auto owning = std::make_unique<BreakpointImpl>(session(), true);
  uint32_t id = owning->backend_id();
  Breakpoint* to_return = owning.get();

  breakpoints_[id] = std::move(owning);
  return to_return;
}

void SystemImpl::DeleteBreakpoint(Breakpoint* breakpoint) {
  BreakpointImpl* impl = static_cast<BreakpointImpl*>(breakpoint);
  auto found = breakpoints_.find(impl->backend_id());
  if (found == breakpoints_.end()) {
    // Should always have found the breakpoint.
    FXL_NOTREACHED();
    return;
  }

  // Only notify observers for non-internal breakpoints.
  if (!found->second->is_internal()) {
    for (auto& observer : observers())
      observer.WillDestroyBreakpoint(breakpoint);
  }
  breakpoints_.erase(found);
}

void SystemImpl::Pause() {
  debug_ipc::PauseRequest request;
  request.process_koid = 0;  // 0 means all processes.
  request.thread_koid = 0;   // 0 means all threads.
  session()->remote_api()->Pause(
      request, std::function<void(const Err&, debug_ipc::PauseReply)>());
}

void SystemImpl::Continue() {
  debug_ipc::ResumeRequest request;
  request.process_koid = 0;  // 0 means all processes.
  request.how = debug_ipc::ResumeRequest::How::kContinue;
  session()->remote_api()->Resume(
      request, std::function<void(const Err&, debug_ipc::ResumeReply)>());
}

void SystemImpl::DidConnect() {
  // Force reload the symbol mappings after connection. This needs to be done
  // for every connection since a new image could have been compiled and
  // launched which will have a different build ID file.
  symbols_.build_id_index().ClearCache();
}

void SystemImpl::DidDisconnect() {
  for (auto& target : targets_)
    target->ImplicitlyDetach();
}

BreakpointImpl* SystemImpl::BreakpointImplForId(uint32_t id) {
  auto found = breakpoints_.find(id);
  if (found == breakpoints_.end())
    return nullptr;
  return found->second.get();
}

void SystemImpl::AddNewTarget(std::unique_ptr<TargetImpl> target) {
  Target* for_observers = target.get();

  targets_.push_back(std::move(target));
  for (auto& observer : observers())
    observer.DidCreateTarget(for_observers);
}

void SystemImpl::AddNewJobContext(std::unique_ptr<JobContextImpl> job_context) {
  JobContext* for_observers = job_context.get();

  job_contexts_.push_back(std::move(job_context));
  for (auto& observer : observers())
    observer.DidCreateJobContext(for_observers);
}

}  // namespace zxdb
