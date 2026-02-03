//
// SPDX-FileCopyrightText: 2021 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#pragma once

#include <AudioToolbox/ExtendedAudioFile.h>

#include <utility>

namespace audio_toolbox {

/// A bare-bones ExtAudioFile wrapper modeled after std::unique_ptr.
class ExtAudioFileWrapper final {
  public:
    /// Creates an empty extended audio file wrapper.
    ExtAudioFileWrapper() noexcept = default;

    // This class is non-copyable
    ExtAudioFileWrapper(const ExtAudioFileWrapper &) = delete;

    // This class is non-assignable
    ExtAudioFileWrapper &operator=(const ExtAudioFileWrapper &) = delete;

    /// Move constructor.
    ExtAudioFileWrapper(ExtAudioFileWrapper &&other) noexcept;

    /// Move assignment operator.
    ExtAudioFileWrapper &operator=(ExtAudioFileWrapper &&other) noexcept;

    /// Calls ExtAudioFileDispose on the managed ExtAudioFile object.
    ~ExtAudioFileWrapper() noexcept;

    /// Creates an extended audio file wrapper managing an existing ExtAudioFile object.
    explicit ExtAudioFileWrapper(ExtAudioFileRef _Nullable extAudioFile) noexcept;

    /// Returns true if the managed ExtAudioFile object is not null.
    [[nodiscard]] explicit operator bool() const noexcept;

    /// Returns the managed ExtAudioFile object.
    [[nodiscard]] operator ExtAudioFileRef _Nullable() const noexcept;

    /// Returns the managed ExtAudioFile object.
    [[nodiscard]] ExtAudioFileRef _Nullable get() const noexcept;

    /// Replaces the managed ExtAudioFile object with another ExtAudioFile object.
    /// @note The object assumes responsibility for disposing of the passed ExtAudioFile object using
    /// ExtAudioFileDispose.
    void reset(ExtAudioFileRef _Nullable extAudioFile = nullptr) noexcept;

    /// Swaps the managed ExtAudioFile object with the managed ExtAudioFile object from another extended audio file
    /// wrapper.
    void swap(ExtAudioFileWrapper &other) noexcept;

    /// Releases ownership of the managed ExtAudioFile object and returns it.
    /// @note The caller assumes responsibility for disposing of the returned ExtAudioFile object using
    /// ExtAudioFileDispose.
    [[nodiscard]] ExtAudioFileRef _Nullable release() noexcept;

  private:
    /// The managed ExtAudioFile object.
    ExtAudioFileRef _Nullable extAudioFile_{nullptr};
};

// MARK: - Implementation -

inline ExtAudioFileWrapper::ExtAudioFileWrapper(ExtAudioFileWrapper &&other) noexcept
    : extAudioFile_{other.release()} {}

inline ExtAudioFileWrapper &ExtAudioFileWrapper::operator=(ExtAudioFileWrapper &&other) noexcept {
    reset(other.release());
    return *this;
}

inline ExtAudioFileWrapper::~ExtAudioFileWrapper() noexcept { reset(); }

inline ExtAudioFileWrapper::ExtAudioFileWrapper(ExtAudioFileRef _Nullable extAudioFile) noexcept
    : extAudioFile_{extAudioFile} {}

inline ExtAudioFileWrapper::operator bool() const noexcept { return extAudioFile_ != nullptr; }

inline ExtAudioFileWrapper::operator ExtAudioFileRef _Nullable() const noexcept { return extAudioFile_; }

inline ExtAudioFileRef _Nullable ExtAudioFileWrapper::get() const noexcept { return extAudioFile_; }

inline void ExtAudioFileWrapper::reset(ExtAudioFileRef _Nullable extAudioFile) noexcept {
    if (auto oldExtAudioFile = std::exchange(extAudioFile_, extAudioFile); oldExtAudioFile) {
        ExtAudioFileDispose(oldExtAudioFile);
    }
}

inline void ExtAudioFileWrapper::swap(ExtAudioFileWrapper &other) noexcept {
    std::swap(extAudioFile_, other.extAudioFile_);
}

inline ExtAudioFileRef _Nullable ExtAudioFileWrapper::release() noexcept {
    return std::exchange(extAudioFile_, nullptr);
}

} /* namespace audio_toolbox */
