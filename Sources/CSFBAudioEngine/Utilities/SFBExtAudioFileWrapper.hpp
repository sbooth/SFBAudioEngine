//
// Copyright © 2021-2025 Stephen F. Booth
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#pragma once

#import <utility>

#import <AudioToolbox/ExtendedAudioFile.h>

namespace SFB {

/// A bare-bones ExtAudioFile wrapper modeled after std::unique_ptr.
class ExtAudioFileWrapper final {
public:

	/// Creates an empty extended audio file wrapper.
	ExtAudioFileWrapper() noexcept = default;

	// This class is non-copyable
	ExtAudioFileWrapper(const ExtAudioFileWrapper&) = delete;

	// This class is non-assignable
	ExtAudioFileWrapper& operator=(const ExtAudioFileWrapper&) = delete;

	/// Move constructor.
	ExtAudioFileWrapper(ExtAudioFileWrapper&& rhs) noexcept
	: mExtAudioFile{rhs.release()}
	{}

	/// Move assignment operator.
	ExtAudioFileWrapper& operator=(ExtAudioFileWrapper&& rhs) noexcept
	{
		if(this != &rhs)
			reset(rhs.release());
		return *this;
	}

	/// Calls ExtAudioFileDispose on the managed ExtAudioFile object.
	~ExtAudioFileWrapper() noexcept
	{
		reset();
	}

	/// Creates an extended audio file wrapper managing an existing ExtAudioFile object.
	ExtAudioFileWrapper(ExtAudioFileRef _Nullable extAudioFile) noexcept
	: mExtAudioFile{extAudioFile}
	{}

	/// Returns true if the managed ExtAudioFile object is not null.
	explicit operator bool() const noexcept
	{
		return mExtAudioFile != nullptr;
	}

	/// Returns the managed ExtAudioFile object.
	operator ExtAudioFileRef _Nullable() const noexcept
	{
		return mExtAudioFile;
	}

	/// Returns the managed ExtAudioFile object.
	ExtAudioFileRef _Nullable get() const noexcept
	{
		return mExtAudioFile;
	}

	/// Disposes of the managed ExtAudioFile object and replaces it with extAudioFile.
	void reset(ExtAudioFileRef _Nullable extAudioFile = nullptr) noexcept
	{
		if(auto oldExtAudioFile = std::exchange(mExtAudioFile, extAudioFile); oldExtAudioFile)
			ExtAudioFileDispose(oldExtAudioFile);
	}

	/// Swaps the managed ExtAudioFile object with the managed ExtAudioFile object from another extended audio file wrapper.
	void swap(ExtAudioFileWrapper& other) noexcept
	{
		std::swap(mExtAudioFile, other.mExtAudioFile);
	}

	/// Releases ownership of the managed ExtAudioFile object and returns it.
	ExtAudioFileRef _Nullable release() noexcept
	{
		return std::exchange(mExtAudioFile, nullptr);
	}

private:
	/// The managed ExtAudioFile object.
	ExtAudioFileRef _Nullable mExtAudioFile{nullptr};
};

} /* namespace SFB */
