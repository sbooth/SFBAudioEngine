//
// SPDX-FileCopyrightText: 2010 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#pragma once

#import "BufferInput.hpp"

namespace sfb {

class FileContentsInput : public BufferInput {
  public:
    explicit FileContentsInput(CFURLRef _Nonnull url);
    ~FileContentsInput() noexcept = default;

    // This class is non-copyable.
    FileContentsInput(const FileContentsInput &) = delete;
    FileContentsInput(FileContentsInput &&) = delete;

    // This class is non-assignable.
    FileContentsInput &operator=(const FileContentsInput &) = delete;
    FileContentsInput &operator=(FileContentsInput &&) = delete;

  private:
    void _open() override;
    void _close() noexcept override;
    CFStringRef _Nonnull _copyDescription() const noexcept override;
};

} /* namespace sfb */
