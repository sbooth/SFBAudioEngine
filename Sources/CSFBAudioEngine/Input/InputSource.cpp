//
// SPDX-FileCopyrightText: 2010 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import "InputSource.hpp"

#import "BufferInput.hpp"
#import "DataInput.hpp"
#import "FileContentsInput.hpp"
#import "FileInput.hpp"
#import "MemoryMappedFileInput.hpp"
#import "scope_exit.hpp"

#import <limits>
#import <stdexcept>

namespace SFB {

const os_log_t InputSource::log_ = os_log_create("org.sbooth.AudioEngine", "InputSource");

} /* namespace SFB */

SFB::InputSource::unique_ptr SFB::InputSource::createForURL(CFURLRef url, FileReadMode mode) {
    switch (mode) {
    case FileReadMode::normal:
        return std::make_unique<FileInput>(url);
    case FileReadMode::memoryMap:
        return std::make_unique<MemoryMappedFileInput>(url);
    case FileReadMode::loadInMemory:
        return std::make_unique<FileContentsInput>(url);
    }
}

SFB::InputSource::unique_ptr SFB::InputSource::createWithData(CFDataRef data) {
    return std::make_unique<DataInput>(data);
}

SFB::InputSource::unique_ptr SFB::InputSource::createWithBytes(const void *buf, int64_t len) {
    return std::make_unique<BufferInput>(buf, len, BufferInput::BufferAdoption::copy);
}

SFB::InputSource::unique_ptr SFB::InputSource::createWithBytesNoCopy(const void *buf, int64_t len, bool free) {
    return std::make_unique<BufferInput>(
            buf, len, free ? BufferInput::BufferAdoption::noCopyAndFree : BufferInput::BufferAdoption::noCopy);
}

SFB::InputSource::~InputSource() noexcept {
    if (url_) {
        CFRelease(url_);
    }
}

void SFB::InputSource::open() {
    if (isOpen()) {
        os_log_debug(log_, "Open() called on <InputSource: %p> that is already open", this);
        return;
    }

    _open();
    isOpen_ = true;
}

void SFB::InputSource::close() {
    if (!isOpen()) {
        os_log_debug(log_, "Close() called on <InputSource: %p> that hasn't been opened", this);
        return;
    }

    const auto defer = scope_exit{[this]() noexcept { isOpen_ = false; }};
    _close();
}

int64_t SFB::InputSource::read(void *buffer, int64_t count) {
    if (!isOpen()) {
        os_log_error(log_, "Read() called on <InputSource: %p> that hasn't been opened", this);
        throw std::logic_error("Input source not open");
    }

    if (!buffer || count < 0) {
        os_log_error(log_, "Read() called on <InputSource: %p> with null buffer or invalid count", this);
        throw std::invalid_argument("Null buffer or negative count");
    }

    return _read(buffer, count);
}

CFDataRef SFB::InputSource::copyData(int64_t count) {
    if (!isOpen()) {
        os_log_error(log_, "CopyData() called on <InputSource: %p> that hasn't been opened", this);
        throw std::logic_error("Input source not open");
    }

    if (count < 0 || count > std::numeric_limits<CFIndex>::max()) {
        os_log_error(log_, "CopyData() called on <InputSource: %p> with invalid count", this);
        throw std::invalid_argument("Invalid count");
    }

    if (count == 0) {
        return CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, nullptr, 0, kCFAllocatorNull);
    }

    void *buf = std::malloc(count);
    if (!buf) {
        throw std::bad_alloc();
    }

    try {
        const auto read = _read(buf, count);
        auto data =
                CFDataCreateWithBytesNoCopy(kCFAllocatorDefault, static_cast<UInt8 *>(buf), read, kCFAllocatorMalloc);
        if (!data) {
            std::free(buf);
        }
        return data;
    } catch (...) {
        std::free(buf);
        throw;
    }
}

std::vector<uint8_t> SFB::InputSource::readBlock(std::vector<uint8_t>::size_type count) {
    if (!isOpen()) {
        os_log_error(log_, "ReadBlock() called on <InputSource: %p> that hasn't been opened", this);
        throw std::logic_error("Input source not open");
    }

    if (count == 0) {
        return {};
    }

    std::vector<uint8_t> vec;
    vec.reserve(count);
    vec.resize(_read(vec.data(), vec.capacity()));
    return vec;
}

bool SFB::InputSource::atEOF() const {
    if (!isOpen()) {
        os_log_error(log_, "AtEOF() called on <InputSource: %p> that hasn't been opened", this);
        throw std::logic_error("Input source not open");
    }

    return _atEOF();
}

int64_t SFB::InputSource::position() const {
    if (!isOpen()) {
        os_log_error(log_, "Position() called on <InputSource: %p> that hasn't been opened", this);
        throw std::logic_error("Input source not open");
    }

    return _position();
}

int64_t SFB::InputSource::length() const {
    if (!isOpen()) {
        os_log_error(log_, "Length() called on <InputSource: %p> that hasn't been opened", this);
        throw std::logic_error("Input source not open");
    }

    return _length();
}

bool SFB::InputSource::supportsSeeking() const {
    if (!isOpen()) {
        os_log_error(log_, "SupportsSeeking() called on <InputSource: %p> that hasn't been opened", this);
        throw std::logic_error("Input source not open");
    }

    return _supportsSeeking();
}

void SFB::InputSource::seekToOffset(int64_t offset, SeekAnchor whence) {
    if (!isOpen()) {
        os_log_error(log_, "SeekToOffset() called on <InputSource: %p> that hasn't been opened", this);
        throw std::logic_error("Input source not open");
    }

    if (!_supportsSeeking()) {
        os_log_error(log_, "SeekToOffset() called on <InputSource: %p> that doesn't support seeking", this);
        throw std::logic_error("Seeking not supported");
    }

    const auto len = _length();

    switch (whence) {
    case SeekAnchor::start:
        /* unchanged */
        break;

    case SeekAnchor::current:
        offset += _position();
        break;

    case SeekAnchor::end:
        offset += len;
        break;
    }

    if (offset < 0 || offset > len) {
        os_log_error(log_, "SeekToOffset() called on <InputSource: %p> with invalid position %lld", this, offset);
        throw std::out_of_range("Invalid seek position");
    }

    return _seekToPosition(offset);
}

CFStringRef SFB::InputSource::copyDescription() const noexcept { return _copyDescription(); }
