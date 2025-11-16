//
// Copyright © 2021-2025 Stephen F. Booth
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#pragma once

#import <utility>

#import <AudioToolbox/AudioFile.h>

namespace SFB {

/// A bare-bones AudioFile wrapper modeled after std::unique_ptr.
class AudioFileWrapper final {
public:

	/// Creates an empty audio file wrapper.
	AudioFileWrapper() noexcept = default;

	// This class is non-copyable
	AudioFileWrapper(const AudioFileWrapper&) = delete;

	// This class is non-assignable
	AudioFileWrapper& operator=(const AudioFileWrapper&) = delete;

	/// Move constructor.
	AudioFileWrapper(AudioFileWrapper&& rhs) noexcept
	: audioFile_{rhs.release()}
	{}

	/// Move assignment operator.
	AudioFileWrapper& operator=(AudioFileWrapper&& rhs) noexcept
	{
		if(this != &rhs)
			reset(rhs.release());
		return *this;
	}

	/// Calls AudioFileClose on the managed AudioFile object.
	~AudioFileWrapper() noexcept
	{
		reset();
	}

	/// Creates an audio file wrapper managing an existing AudioFile object.
	AudioFileWrapper(AudioFileID _Nullable audioFile) noexcept
	: audioFile_{audioFile}
	{}

	/// Returns true if the managed AudioFile object is not null.
	explicit operator bool() const noexcept
	{
		return audioFile_ != nullptr;
	}

	/// Returns the managed AudioFile object.
	operator AudioFileID _Nullable() const noexcept
	{
		return audioFile_;
	}

	/// Returns the managed AudioFile object.
	AudioFileID _Nullable get() const noexcept
	{
		return audioFile_;
	}

	/// Closes the managed AudioFile object and replaces it with another.
	void reset(AudioFileID _Nullable audioFile = nullptr) noexcept
	{
		if(auto old = std::exchange(audioFile_, audioFile); old)
			AudioFileClose(old);
	}

	/// Swaps the managed AudioFile object with the managed AudioFile object from another audio file wrapper.
	void swap(AudioFileWrapper& other) noexcept
	{
		std::swap(audioFile_, other.audioFile_);
	}

	/// Releases ownership of the managed AudioFile object and returns it.
	AudioFileID _Nullable release() noexcept
	{
		return std::exchange(audioFile_, nullptr);
	}

private:
	/// The managed AudioFile object.
	AudioFileID _Nullable audioFile_{nullptr};
};

} /* namespace SFB */
