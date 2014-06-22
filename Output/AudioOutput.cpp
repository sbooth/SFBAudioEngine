/*
 *  Copyright (C) 2014 Stephen F. Booth <me@sbooth.org>
 *  All Rights Reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *    - Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    - Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *    - Neither the name of Stephen F. Booth nor the names of its
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *  LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *  THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
