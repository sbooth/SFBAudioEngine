/*
 * Copyright (c) 2014 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include "AudioOutput.h"
#include "Logger.h"

SFB::Audio::Output::Output()
	: mPlayer(nullptr)
{}

#pragma mark -

bool SFB::Audio::Output::SupportsFormat(const AudioFormat& format) const
{
	return _SupportsFormat(format);
}

bool SFB::Audio::Output::Open()
{
	LOGGER_DEBUG("org.sbooth.AudioEngine.Output", "Opening output");

	if(_IsOpen())
		return true;

	return _Open();
}

bool SFB::Audio::Output::Close()
{
	LOGGER_DEBUG("org.sbooth.AudioEngine.Output", "Closing output");

	if(!_IsOpen())
		return true;

	return _Close();
}


bool SFB::Audio::Output::Start()
{
	LOGGER_DEBUG("org.sbooth.AudioEngine.Output", "Starting output");

	if(!_IsOpen())
		return false;

	if(_IsRunning())
		return true;

	return _Start();
}

bool SFB::Audio::Output::Stop()
{
	LOGGER_DEBUG("org.sbooth.AudioEngine.Output", "Stopping output");

	if(!_IsOpen())
		return false;

	if(!_IsRunning())
		return true;

	return _Stop();
}

bool SFB::Audio::Output::RequestStop()
{
	LOGGER_DEBUG("org.sbooth.AudioEngine.Output", "Requesting output stop");

	if(!_IsOpen())
		return false;

	if(!_IsRunning())
		return true;

	return _RequestStop();
}

bool SFB::Audio::Output::Reset()
{
	LOGGER_DEBUG("org.sbooth.AudioEngine.Output", "Resetting output");

	if(!_IsOpen())
		return false;

	// Some outputs may be able to reset while running

	return _Reset();
}

bool SFB::Audio::Output::SetupForDecoder(const Decoder& decoder)
{
	return _SetupForDecoder(decoder);
}

#pragma mark -

bool SFB::Audio::Output::CreateDeviceUID(CFStringRef& deviceUID) const
{
	return _CreateDeviceUID(deviceUID);
}

bool SFB::Audio::Output::SetDeviceUID(CFStringRef deviceUID)
{
	LOGGER_DEBUG("org.sbooth.AudioEngine.Output", "Setting device UID to " << deviceUID);
	return _SetDeviceUID(deviceUID);
}

bool SFB::Audio::Output::GetDeviceSampleRate(Float64& sampleRate) const
{
	return _GetDeviceSampleRate(sampleRate);
}

bool SFB::Audio::Output::SetDeviceSampleRate(Float64 sampleRate)
{
	LOGGER_DEBUG("org.sbooth.AudioEngine.Output", "Setting device sample rate to " << sampleRate);
	return _SetDeviceSampleRate(sampleRate);
}

size_t SFB::Audio::Output::GetPreferredBufferSize() const
{
	return _GetPreferredBufferSize();
}
