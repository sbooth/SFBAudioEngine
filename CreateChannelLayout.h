/*
 *  Copyright (C) 2011, 2012, 2013 Stephen F. Booth <me@sbooth.org>
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

#pragma once

#include <CoreAudio/CoreAudioTypes.h>

/*! @file CreateChannelLayout.h @brief Utility functions for allocating \c AudioChannelLayout structs */

/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	/*! @brief Get the size in bytes of an \c AudioChannelLayout with the specified number of channel descriptions */
	size_t GetChannelLayoutSize(UInt32 numberChannelDescriptions);

	/*! @brief Get the size, in bytes, of \c layout */
	size_t GetChannelLayoutSize(const AudioChannelLayout *layout);


	/*!
	 * @brief Allocate an \c AudioChannelLayout
	 * @param numberChannelDescriptions The number of channel descriptions that will be stored in the channel layout
	 * @return An \c AudioChannelLayout
	 */
	AudioChannelLayout * CreateChannelLayout(UInt32 numberChannelDescriptions = 0);

	/*!
	 * @brief Allocate an \c AudioChannelLayout
	 * @param layoutTag The layout tag for the channel layout
	 * @return An \c AudioChannelLayout
	 */
	AudioChannelLayout * CreateChannelLayoutWithTag(AudioChannelLayoutTag layoutTag);

	/*!
	 * @brief Allocate an \c AudioChannelLayout
	 * @param channelBitmap The channel bitmap for the channel layout
	 * @return An \c AudioChannelLayout
	 */
	AudioChannelLayout * CreateChannelLayoutWithBitmap(UInt32 channelBitmap);


	/*! @brief Create a copy of \c rhs */
	AudioChannelLayout * CopyChannelLayout(const AudioChannelLayout *rhs);

}
