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

#pragma once

#include <memory>

#include <CoreFoundation/CoreFoundation.h>

#include "AudioDecoder.h"

/*! @file AudioOutput.h @brief Audio output functionality */

/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	/*! @brief %Audio functionality */
	namespace Audio {

		namespace ASIO {
			class Player;
		}

		/*!
		 * @brief Base class for an audio output device
		 *
		 * An Output is responsible for accepting data from an AudioPlayer and
		 * sending it to an output device, in addition to handling device setup
		 * and parameter
		 */
		class Output
		{
		public:

			/*! @brief A \c std::unique_ptr for \c Output objects */
			typedef std::unique_ptr<Output> unique_ptr;

			// ========================================
			/*! @name Creation and Destruction */
			// @{

			/*! @brief Destroy this \c Output */
			inline virtual ~Output() = default;

			/*! @cond */

			/*! @internal This class is non-copyable */
			Output(const Output& rhs) = delete;

			/*! @internal This class is non-assignable */
			Output& operator=(const Output& rhs) = delete;

			/*! @endcond */
			
			//@}

			// ========================================
			/*! @name Device Management */
			//@{

			/*!
			 * @brief Create the UID of the output device
			 * @note The returned string must be released by the caller
			 * @param deviceUID A \c CFStringRef to receive the UID
			 * @return \c true on success, \c false otherwise
			 */
			bool CreateDeviceUID(CFStringRef& deviceUID) const;

			/*!
			 * @brief Set the output device to the device matching the provided UID
			 * @param deviceUID The UID of the desired device
			 * @return \c true on success, \c false otherwise
			 */
			bool SetDeviceUID(CFStringRef deviceUID);

			/*!
			 * @brief Get the sample rate of the output device
			 * @param sampleRate A \c Float64 to receive the sample rate
			 * @return \c true on success, \c false otherwise
			 */
			bool GetDeviceSampleRate(Float64& sampleRate) const;

			/*!
			 * @brief Set the sample rate of the output device
			 * @param sampleRate The desired sample rate
			 * @return \c true on success, \c false otherwise
			 */
			bool SetDeviceSampleRate(Float64 sampleRate);
			
			//@}

			bool SetupForDecoder(const Decoder& decoder, AudioFormat& format, ChannelLayout& channelLayout);

			void SetStateChangedBlock(dispatch_block_t block);

			size_t GetPreferredBufferSize() const;

			// ========================================
			// Output setup and Control
			bool Open();
			bool Close();

			bool Start();
			bool Stop();
			bool RequestStop();

			inline bool IsOpen() const								{ return _IsOpen(); }
			inline bool IsRunning() const							{ return _IsRunning(); }

			bool Reset();

			// FIXME: This should probably be passed in the ctor
			inline void SetPlayer(ASIO::Player * player)			{ mPlayer = player; }
			inline ASIO::Player * GetPlayer() const					{ return mPlayer; }

		protected:

			/*! @brief Create a new \c Output and initialize \c Output::mPlayer to \c nullptr */
			Output();

			ASIO::Player		*mPlayer;
			dispatch_block_t	mStateChangedBlock;

		private:

			// Subclasses must implement the following methods
			virtual bool _Open() = 0;
			virtual bool _Close() = 0;

			virtual bool _Start() = 0;
			virtual bool _Stop() = 0;
			virtual bool _RequestStop() = 0;

			virtual bool _IsOpen() const = 0;
			virtual bool _IsRunning() const = 0;

			virtual bool _Reset() = 0;

			virtual bool _SetupForDecoder(const Decoder& decoder, AudioFormat& format, ChannelLayout& channelLayout) = 0;

			// Optional methods
			virtual bool _CreateDeviceUID(CFStringRef& deviceUID) const		{ return false; }
			virtual bool _SetDeviceUID(CFStringRef deviceUID)				{ return false; }

			virtual bool _GetDeviceSampleRate(Float64& sampleRate) const	{ return false; }
			virtual bool _SetDeviceSampleRate(Float64 sampleRate)			{ return false; }

			virtual size_t _GetPreferredBufferSize() const					{ return 0; }

		};
	}
}
