//
// SPDX-FileCopyrightText: 2026 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#pragma once

#include <memory>

namespace util {

/// A `std::unique_ptr` deleter that releases an object by calling a disposal function.
/// @tparam Dispose The function used to release the object, such as `AudioFileClose` or `ExtAudioFileDispose`.
/// It must accept a single pointer argument. Its return value, if any, is ignored.
template <auto Dispose> struct dispose_deleter {
    /// Releases the object by calling `Dispose(p)`.
    /// @param p The object to release. `std::unique_ptr` never passes null.
    template <typename T> void operator()(T *p) const noexcept { Dispose(p); }
};

/// A `std::unique_ptr` that owns an opaque reference and releases it with a disposal function.
///
/// For example:
/// @code
/// using audio_file_unique_ptr = util::unique_ref<AudioFileID, AudioFileClose>;
/// @endcode
///
/// @tparam Ref The opaque reference type, which must be a pointer type such as `AudioFileID` or `ExtAudioFileRef`.
/// @tparam Dispose The function used to release the reference when the `std::unique_ptr` is destroyed or reset.
/// @note Errors returned by `Dispose` are discarded. To check them, call `Dispose(ptr.release())` explicitly.
template <typename Ref, auto Dispose>
using unique_ref = std::unique_ptr<std::remove_pointer_t<Ref>, dispose_deleter<Dispose>>;

} /* namespace util */
