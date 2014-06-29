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

		/*! @brief Output subclass supporting ASIO */
		class ASIOOutput : public Output
		{
		public:

			// ========================================
			/*! @name Driver dictionary keys */
			//@{
			static const CFStringRef kDriverIDKey;					/*!< @brief The driver's dylib ID */
			static const CFStringRef kDriverNumberKey;				/*!< @brief The driver number (\c CFNumber) */
			static const CFStringRef kDriverDisplayNameKey;			/*!< @brief The driver display name */
			static const CFStringRef kDriverCompanyKey;				/*!< @brief The company */
			static const CFStringRef kDriverFolderKey;				/*!< @brief The install folder */
			static const CFStringRef kDriverArchitecturesKey;		/*!< @brief The supported architectures */
			static const CFStringRef kDriverUIDKey;					/*!< @brief The driver's UID */
			//@}

			
			// ========================================
			/*! @name Driver discovery */
			//@{

			/*! @brief Query whether an ASIO driver is available */
			static bool IsAvailable();

			/*! @brief Create a list of available ASIO drivers */
			static CFArrayRef CreateAvailableDrivers();

			/*! @brief Create an \c ASIOOutput for the specified driver */
			static unique_ptr CreateInstanceForDriverUID(CFStringRef driverUID);

			//@}


			// ========================================
			/*! @name Creation and Destruction */
			// @{

			/*! @brief Create an \c ASIOOutput for the specified driver */
			ASIOOutput(CFStringRef driverUID);

			/*! @brief Destroy this \c ASIOOutput */
			virtual ~ASIOOutput();

			//@}


			/*! Device input/output format information */
			enum class DeviceIOFormat {
				eDeviceIOFormatPCM,		/*!< Pulse code modulation (PCM) */
				eDeviceIOFormatDSD		/*!< Direct stream digital (DSD) */
			};

			/*! @brief Get the format in use by the device for IO transactions */
			bool GetDeviceIOFormat(DeviceIOFormat& deviceIOFormat) const;

		protected:
			
			/*! @brief Set the format the device should use for IO transactions */
			bool SetDeviceIOFormat(const DeviceIOFormat& deviceIOFormat);

		public:

			/*! @brief Set a block to be invoked when the running state changes */
			void SetStateChangedBlock(dispatch_block_t block);

		private:

			ASIOOutput();

			virtual bool _Open();
			virtual bool _Close();

			virtual bool _Start();
			virtual bool _Stop();
			virtual bool _RequestStop();

			virtual bool _IsOpen() const;
			virtual bool _IsRunning() const;

			virtual bool _Reset();

			virtual bool _SupportsFormat(const AudioFormat& format) const;

			virtual bool _SetupForDecoder(const Decoder& decoder);

			virtual bool _CreateDeviceUID(CFStringRef& deviceUID) const;
			virtual bool _SetDeviceUID(CFStringRef deviceUID);

			virtual bool _GetDeviceSampleRate(Float64& sampleRate) const;
			virtual bool _SetDeviceSampleRate(Float64 sampleRate);

			virtual size_t _GetPreferredBufferSize() const;

			SFB::CFString							mDesiredDriverUID;		/*!< Requested ASIO driver UID */

			SFB::RingBuffer::unique_ptr				mEventQueue;			/*!< ASIO event queue */
			dispatch_source_t						mEventQueueTimer;		/*!< ASIO event queue timer */

			dispatch_block_t						mStateChangedBlock;		/*!< Block called when running state changes */

			ChannelLayout							mDriverChannelLayout;	/*!< Channel layout for ASIO driver transactions */
			std::vector<SInt32>						mChannelMap;			/*!< The channel map */

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
