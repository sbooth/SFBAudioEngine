//
// SPDX-FileCopyrightText: 2021 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#pragma once

#include <AudioToolbox/AudioFile.h>

#include <utility>

namespace audio_toolbox {

/// A bare-bones AudioFile wrapper modeled after std::unique_ptr.
class AudioFileWrapper final {
  public:
    /// Creates an empty audio file wrapper.
    AudioFileWrapper() noexcept = default;

    // This class is non-copyable
    AudioFileWrapper(const AudioFileWrapper &) = delete;

    // This class is non-assignable
    AudioFileWrapper &operator=(const AudioFileWrapper &) = delete;

    /// Move constructor.
    AudioFileWrapper(AudioFileWrapper &&other) noexcept;

    /// Move assignment operator.
    AudioFileWrapper &operator=(AudioFileWrapper &&other) noexcept;

    /// Calls AudioFileClose on the managed AudioFile object.
    ~AudioFileWrapper() noexcept;

    /// Creates an audio file wrapper managing an existing AudioFile object.
    explicit AudioFileWrapper(AudioFileID _Nullable audioFile) noexcept;

    /// Returns true if the managed AudioFile object is not null.
    [[nodiscard]] explicit operator bool() const noexcept;

    /// Returns the managed AudioFile object.
    [[nodiscard]] operator AudioFileID _Nullable() const noexcept;

    /// Returns the managed AudioFile object.
    [[nodiscard]] AudioFileID _Nullable get() const noexcept;

    /// Replaces the managed AudioFile object with another AudioFile object.
    /// @note The object assumes responsibility for closing the passed AudioFile object using AudioFileClose.
    void reset(AudioFileID _Nullable audioFile = nullptr) noexcept;

    /// Swaps the managed AudioFile object with the managed AudioFile object from another audio file wrapper.
    void swap(AudioFileWrapper &other) noexcept;

    /// Releases ownership of the managed AudioFile object and returns it.
    /// @note The caller assumes responsibility for closing the returned AudioFile object using AudioFileClose.
    [[nodiscard]] AudioFileID _Nullable release() noexcept;

  private:
    /// The managed AudioFile object.
    AudioFileID _Nullable audioFile_{nullptr};
};

// MARK: - Implementation -

inline AudioFileWrapper::AudioFileWrapper(AudioFileWrapper &&other) noexcept : audioFile_{other.release()} {}

inline AudioFileWrapper &AudioFileWrapper::operator=(AudioFileWrapper &&other) noexcept {
    reset(other.release());
    return *this;
}

inline AudioFileWrapper::~AudioFileWrapper() noexcept { reset(); }

inline AudioFileWrapper::AudioFileWrapper(AudioFileID _Nullable audioFile) noexcept : audioFile_{audioFile} {}

inline AudioFileWrapper::operator bool() const noexcept { return audioFile_ != nullptr; }

inline AudioFileWrapper::operator AudioFileID _Nullable() const noexcept { return audioFile_; }

inline AudioFileID _Nullable AudioFileWrapper::get() const noexcept { return audioFile_; }

inline void AudioFileWrapper::reset(AudioFileID _Nullable audioFile) noexcept {
    if (auto old = std::exchange(audioFile_, audioFile); old) {
        AudioFileClose(old);
    }
}

inline void AudioFileWrapper::swap(AudioFileWrapper &other) noexcept { std::swap(audioFile_, other.audioFile_); }

inline AudioFileID _Nullable AudioFileWrapper::release() noexcept { return std::exchange(audioFile_, nullptr); }

} /* namespace audio_toolbox */
