// Copyright 2018 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <inttypes.h>
#include <memory>
#include <string>

#include "garnet/bin/zxdb/client/err.h"
#include "garnet/public/lib/fxl/macros.h"

namespace llvm {
class MCContext;
class MCDisassembler;
class MCInstPrinter;
}  // namespace llvm

namespace zxdb {

class ArchInfo;
class MemoryDump;
class OutputBuffer;

// Disassembles a block of data.
class Disassembler {
 public:
  struct Options {
    // Writes addresses to the output stream.
    bool emit_addresses = false;

    // Writes the raw bytes to the output stream.
    bool emit_bytes = false;

    // Controls the behavior for undecodable instructions. When false,
    // DisassembleOne() will report no data consumed and the empty string will
    // be returned. When true, it will emit a "data" mnemonic and advance to
    // the next instruction boundary.
    //
    // DisassembleMany will always should undecodable instructions (otherwise
    // it won't advance).
    bool emit_undecodable = true;
  };

  Disassembler();
  ~Disassembler();

  // The ArchInfo pointer must outlive this class. Since typically this will
  // come from the Session object which can destroy the LLVM context when the
  // agent is disconnected, you will not want to store Disassembler objects.
  Err Init(const ArchInfo* arch);

  // Disassembles one machine instruction, appending the string (with a
  // newline) to the output buffer. The number of bytes consumed will be
  // returned.
  //
  // Be sure the input buffer always has enough data for any instruction.
  size_t DisassembleOne(const uint8_t* data,
                        size_t data_len,
                        uint64_t address,
                        const Options& options,
                        OutputBuffer* out) const;

  // Disassembles the block, either until there is no more data, or
  // |max_instructions| have been decoded. If max_instructions is 0 it will
  // always decode the whole block.
  //
  // The number of instructions decoded will be put into *instruction_count.
  size_t DisassembleMany(const uint8_t* data,
                         size_t data_len,
                         uint64_t start_address,
                         const Options& options,
                         size_t max_instructions,
                         OutputBuffer* out,
                         size_t* instruction_count) const;

  // Like DisassembleMany() but uses a MemoryDump object. The dump will start
  // at the beginning of the memory dump. This function understands the
  // addresses of the memory dump, and also invalid ranges (which will be
  // marked in the disassemly).
  //
  // An unmapped range will be counted as one instruction. The memory
  // addresses for unmapped ranges will always be shown even if disabled in the
  // options.
  size_t DisassembleDump(const MemoryDump& dump,
                         const Options& options,
                         size_t max_instructions,
                         OutputBuffer* out,
                         size_t* instruction_count) const;

 private:
  const ArchInfo* arch_ = nullptr;

  std::unique_ptr<llvm::MCContext> context_;
  std::unique_ptr<llvm::MCDisassembler> disasm_;
  std::unique_ptr<llvm::MCInstPrinter> printer_;

  FXL_DISALLOW_COPY_AND_ASSIGN(Disassembler);
};

}  // namespace zxdb
