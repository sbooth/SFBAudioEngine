//
// SPDX-FileCopyrightText: 2010 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#pragma once

#import "BufferInput.hpp"

namespace SFB {

class MemoryMappedFileInput : public BufferInput {
  public:
    explicit MemoryMappedFileInput(CFURLRef _Nonnull url);
    ~MemoryMappedFileInput() noexcept;

    // This class is non-copyable.
    MemoryMappedFileInput(const MemoryMappedFileInput &) = delete;
    MemoryMappedFileInput(MemoryMappedFileInput &&) = delete;

    // This class is non-assignable.
    MemoryMappedFileInput &operator=(const MemoryMappedFileInput &) = delete;
    MemoryMappedFileInput &operator=(MemoryMappedFileInput &&) = delete;

  private:
    void _open() override;
    void _close() override;
    CFStringRef _Nonnull _copyDescription() const noexcept override;
};

} /* namespace SFB */
