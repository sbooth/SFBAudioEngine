/*
 *  Copyright (C) 2010, 2011 Stephen F. Booth <me@sbooth.org>
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

#include <cstdlib>
#include "AudioDitherer.h"

// arc4random() returns pseudo-random integers in the range [0, 2^32 - 1]
#define ARC4RANDOM_MAX		4294967295.0

// Evaluates to a random number in the interval [0, 1]
#define DITHER_NOISE		(arc4random() / ARC4RANDOM_MAX)

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

void AudioDitherer::Dither(double *buffer, unsigned long frameCount)
{
	if(NULL == buffer || 0 == frameCount)
		return;

	switch(mDitherType) {
		case eNoDither:
			break;

		case eRectangularDither:
			while(frameCount--)
				*buffer++ -= DITHER_NOISE;
			break;

		case eTriangularDither:
		{
			double r;
			while(frameCount--) {
				r = DITHER_NOISE - 0.5;
				*buffer++ -= (r - mTriangleState);
				mTriangleState = r;	
			}

			break;
		}
	}
}
