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

#include "AudioOutput.h"
#include "RingBuffer.h"

/*! @file ASIOOutput.h @brief ASIO output functionality */

/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	/*! @brief %Audio functionality */
	namespace Audio {

		/*! @brief Output subclass supporting exaSound's ASIO driver */
		class ASIOOutput : public Output
		{
		public:

			// ASIO only supports a single instance
//			static ASIOOutput * GetInstance();

			ASIOOutput();
			virtual ~ASIOOutput();

			/*! Device input/output format information */
			enum class DeviceIOFormat {
				eDeviceIOFormatPCM,		/*!< Pulse code modulation (PCM) */
				eDeviceIOFormatDSD		/*!< Direct stream digital (DSD) */
			};

			bool GetDeviceIOFormat(DeviceIOFormat& deviceIOFormat) const;

		protected:
			
			bool SetDeviceIOFormat(const DeviceIOFormat& deviceIOFormat);

		public:

			/*! @brief Open the stereo ASIO driver */
			bool OpenStereo();

			/*! @brief Open the multichannel ASIO driver */
			bool OpenMultichannel();

			/*! @brief Set a block to be invoked when the running state changes */
			void SetStateChangedBlock(dispatch_block_t block);

		private:

			virtual bool _Open();
			virtual bool _Close();

			virtual bool _Start();
			virtual bool _Stop();
			virtual bool _RequestStop();

			virtual bool _IsOpen() const;
			virtual bool _IsRunning() const;

			virtual bool _Reset();

			virtual bool _SetupForDecoder(const Decoder& decoder);

			virtual bool _CreateDeviceUID(CFStringRef& deviceUID) const;
			virtual bool _SetDeviceUID(CFStringRef deviceUID);

			virtual bool _GetDeviceSampleRate(Float64& sampleRate) const;
			virtual bool _SetDeviceSampleRate(Float64 sampleRate);

			virtual size_t _GetPreferredBufferSize() const;

			bool _OpenOutput(uint32_t index);

			SFB::RingBuffer::unique_ptr				mEventQueue;			/*!< ASIO event queue */
			dispatch_source_t						mEventQueueTimer;		/*!< ASIO event queue timer */

			dispatch_block_t						mStateChangedBlock;		/*!< Block called when running state changes */

		public:

			// ========================================
			/*! @cond */

			/*! @internal ASIO message callback */
			long HandleASIOMessage(long selector, long value, void *message, double *opt);

			/*! @internal ASIO render callback */
			void FillASIOBuffer(long doubleBufferIndex);

			/*! @endcond */
		};
	}
}
