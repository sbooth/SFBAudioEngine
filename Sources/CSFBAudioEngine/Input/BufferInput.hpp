//
// SPDX-FileCopyrightText: 2010 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#pragma once

#import "InputSource.hpp"

namespace sfb {

class BufferInput : public InputSource {
  public:
    /// Buffer adoption behaviors.
    enum class BufferAdoption { copy, noCopy, noCopyAndFree };
    BufferInput(const void *_Nonnull buf, int64_t len, BufferAdoption behavior = BufferAdoption::copy);
    ~BufferInput() noexcept;

    // This class is non-copyable.
    BufferInput(const BufferInput &) = delete;
    BufferInput(BufferInput &&) = delete;

    // This class is non-assignable.
    BufferInput &operator=(const BufferInput &) = delete;
    BufferInput &operator=(BufferInput &&) = delete;

  protected:
    explicit BufferInput() noexcept = default;

    /// The data buffer.
    void *_Nonnull buf_{nullptr};
    /// Whether the buffer should be freed in the destructor.
    bool free_{false};
    /// The length of the buffer in bytes.
    int64_t len_{0};
    /// The current byte position in the buffer.
    int64_t pos_{0};

  private:
    void _open() override { pos_ = 0; }
    void _close() override {}
    bool _atEOF() const noexcept override { return len_ == pos_; }
    int64_t _position() const noexcept override { return pos_; }
    int64_t _length() const noexcept override { return len_; }
    bool _supportsSeeking() const noexcept override { return true; }
    void _seekToPosition(int64_t position) override { pos_ = position; }

    int64_t _read(void *_Nonnull buffer, int64_t count) override;
    CFStringRef _Nonnull _copyDescription() const noexcept override;
};

} /* namespace sfb */
