//
// SPDX-FileCopyrightText: 2010 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#pragma once

#import "InputSource.hpp"

namespace SFB {

class DataInput : public InputSource {
  public:
    explicit DataInput(CFDataRef _Nonnull data);
    ~DataInput() noexcept;

    // This class is non-copyable.
    DataInput(const DataInput &) = delete;
    DataInput(DataInput &&) = delete;

    // This class is non-assignable.
    DataInput &operator=(const DataInput &) = delete;
    DataInput &operator=(DataInput &&) = delete;

  private:
    void _open() noexcept override { pos_ = 0; }
    void _close() noexcept override {}
    bool _atEOF() const noexcept override { return CFDataGetLength(data_) == pos_; }
    int64_t _position() const noexcept override { return pos_; }
    int64_t _length() const noexcept override { return CFDataGetLength(data_); }
    bool _supportsSeeking() const noexcept override { return true; }
    void _seekToPosition(int64_t position) override { pos_ = position; }

    int64_t _read(void *_Nonnull buffer, int64_t count) override;
    CFStringRef _Nonnull _copyDescription() const noexcept override;

    CFDataRef _Nonnull data_{nullptr};
    CFIndex pos_{0};
};

} /* namespace SFB */
