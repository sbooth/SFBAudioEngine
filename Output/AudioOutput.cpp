/*
 * Copyright (c) 2014 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include <os/log.h>

#include "AudioOutput.h"

SFB::Audio::Output::Output()
	: mPlayer(nullptr),	mPrepareForFormatBlock(nullptr)
{}
SFB::Audio::Output::~Output()
{
	if(mPrepareForFormatBlock) {
		Block_release(mPrepareForFormatBlock);
		mPrepareForFormatBlock = nullptr;
	}
}

#pragma mark -

bool SFB::Audio::Output::SupportsFormat(const AudioFormat& format) const
{
	return _SupportsFormat(format);
}

void SFB::Audio::Output::SetPrepareForFormatBlock(FormatBlock block)
{
	if(mPrepareForFormatBlock) {
		Block_release(mPrepareForFormatBlock);
		mPrepareForFormatBlock = nullptr;
	}
	if(block)
		mPrepareForFormatBlock = Block_copy(block);
}

bool SFB::Audio::Output::Open()
{
	os_log_debug(OS_LOG_DEFAULT, "Opening output");

	if(_IsOpen())
		return true;

	return _Open();
}

bool SFB::Audio::Output::Close()
{
	os_log_debug(OS_LOG_DEFAULT, "Closing output");

	if(!_IsOpen())
		return true;

	return _Close();
}


bool SFB::Audio::Output::Start()
{
	os_log_debug(OS_LOG_DEFAULT, "Starting output");

	if(!_IsOpen())
		return false;

	if(_IsRunning())
		return true;

	return _Start();
}

bool SFB::Audio::Output::Stop()
{
	os_log_debug(OS_LOG_DEFAULT, "Stopping output");

	if(!_IsOpen())
		return false;

	if(!_IsRunning())
		return true;

	return _Stop();
}

bool SFB::Audio::Output::RequestStop()
{
	os_log_debug(OS_LOG_DEFAULT, "Requesting output stop");

	if(!_IsOpen())
		return false;

	if(!_IsRunning())
		return true;

	return _RequestStop();
}

bool SFB::Audio::Output::Reset()
{
	os_log_debug(OS_LOG_DEFAULT, "Resetting output");

	if(!_IsOpen())
		return false;

	// Some outputs may be able to reset while running

	return _Reset();
}

bool SFB::Audio::Output::SetupForDecoder(const Decoder& decoder)
{
	if(mPrepareForFormatBlock)
		mPrepareForFormatBlock(decoder.GetFormat());
	return _SetupForDecoder(decoder);
}

#pragma mark -

bool SFB::Audio::Output::CreateDeviceUID(CFStringRef& deviceUID) const
{
	return _CreateDeviceUID(deviceUID);
}

bool SFB::Audio::Output::SetDeviceUID(CFStringRef deviceUID)
{
	os_log_debug(OS_LOG_DEFAULT, "Setting device UID to %{public}@", deviceUID);
	return _SetDeviceUID(deviceUID);
}

bool SFB::Audio::Output::GetDeviceSampleRate(Float64& sampleRate) const
{
	return _GetDeviceSampleRate(sampleRate);
}

bool SFB::Audio::Output::SetDeviceSampleRate(Float64 sampleRate)
{
	os_log_debug(OS_LOG_DEFAULT, "Setting device sample rate to %f", sampleRate);
	return _SetDeviceSampleRate(sampleRate);
}

size_t SFB::Audio::Output::GetPreferredBufferSize() const
{
	return _GetPreferredBufferSize();
}
