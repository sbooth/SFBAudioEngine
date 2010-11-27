/*
 *  Copyright (C) 2010 Stephen F. Booth <me@sbooth.org>
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

#include "AudioDitherer.h"

AudioDitherer::AudioDitherer(DitherType type)
	: mDitherType(type)
{
	Reset();
}

AudioDitherer::~AudioDitherer()
{}

void AudioDitherer::SetDitherType(DitherType ditherType)
{
	mDitherType = ditherType;
	Reset();
}

void AudioDitherer::Reset()
{
    mTriangleState = 0;
}

UInt32 AudioDitherer::Dither(AudioBuffer *buffer, UInt32 frameCount)
{
	assert(NULL != buffer);

	switch(mDitherType) {
		case eRectangularDither:		return ApplyRectangularDither(buffer, frameCount);
		case eTriangularDither:			return ApplyTriangularDither(buffer, frameCount);
	}

	return 0;
}

UInt32 AudioDitherer::ApplyRectangularDither(AudioBuffer *buffer, UInt32 frameCount)
{
	double *doubleBuffer = static_cast<double *>(buffer->mData);
	UInt32 framesToProcess = frameCount;
	while(--framesToProcess)
		*doubleBuffer++ -= rand() / (double)RAND_MAX - 0.5;
	return frameCount;
}

UInt32 AudioDitherer::ApplyTriangularDither(AudioBuffer *buffer, UInt32 frameCount)
{
	double *doubleBuffer = static_cast<double *>(buffer->mData);
	double r;
	UInt32 framesToProcess = frameCount;
	while(--framesToProcess) {
		r = rand() / (double)RAND_MAX - 0.5;
		*doubleBuffer++ += r - mTriangleState;
		mTriangleState = r;	
	}
	return frameCount;
}
