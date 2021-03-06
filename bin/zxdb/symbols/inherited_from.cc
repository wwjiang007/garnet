// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "garnet/bin/zxdb/symbols/inherited_from.h"

namespace zxdb {

InheritedFrom::InheritedFrom(LazySymbol from, uint64_t offset)
    : from_(std::move(from)), offset_(offset) {}
InheritedFrom::~InheritedFrom() = default;

const InheritedFrom* InheritedFrom::AsInheritedFrom() const { return this; }

}  // namespace zxdb
