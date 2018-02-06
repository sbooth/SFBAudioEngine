/*
 * Copyright (c) 2014 - 2018 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
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

		class Player;

		/*!
		 * @brief Base class for an audio output device
		 *
		 * An Output is responsible for accepting data from a Player and
		 * sending it to an output device, in addition to handling device setup
		 * and parameter management.
		 */
		class Output
		{

			/*! @brief For access to Open(), Close(), Start(), Stop(), etc.*/
			friend class Player;

		public:

			/*! @brief A \c std::unique_ptr for \c Output objects */
			using unique_ptr = std::unique_ptr<Output>;

			/*!
			 * @brief A block called immediately before the output is configured for an \c AudioDecoder with the specified format
			 * @param format The next audio format
			 */
			using FormatBlock = void (^)(const AudioFormat& format);

			// ========================================
			/*! @name Creation and Destruction */
			// @{

			/*! @brief Destroy this \c Output */
			virtual ~Output();

			/*! @cond */

			/*! @internal This class is non-copyable */
			Output(const Output& rhs) = delete;

			/*! @internal This class is non-assignable */
			Output& operator=(const Output& rhs) = delete;

			/*! @endcond */

			//@}

			// ========================================
			/*! @name Device Information */
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


			// ========================================
			/*! @name Format Information */
			//@{

			/*! @brief Get the audio format this output requires */
			inline const AudioFormat& GetFormat() const					{ return mFormat; }

			/*! @brief Get the channel layout used by this output */
			inline const ChannelLayout& GetChannelLayout() const		{ return mChannelLayout; }

			/*! @brief Query whether this output supports audio in the given format */
			bool SupportsFormat(const AudioFormat& format) const;

			//@}


			// ========================================
			/*! @name Block-based callback support */
			//@{

			/*!
			 * @brief Set the block called immediately before the output is configured for an \c AudioDecoder with the specified format
			 * @note Normally the most relevant parameters are the sample rate and number of channels
			 * @note This block may be invoked from the decoding thread
			 * @param block The block to be invoked before the output is configured for th specified format
			 */
			void SetPrepareForFormatBlock(FormatBlock block);

			//@}

		protected:

			// ========================================
			/*! @name I/O Information */
			//@{

			/*! @brief Open the output */
			bool Open();

			/*! @brief Close the output */
			bool Close();


			/*! @brief Start the output */
			bool Start();

			/*! @brief Stop the output */
			bool Stop();

			/*! @brief Request a stop */
			bool RequestStop();


			/*! @brief Reset the output to the initial state */
			bool Reset();


			/*! @brief Determine if the output is open */
			inline bool IsOpen() const									{ return _IsOpen(); }

			/*! @brief Determine if the output is running */
			inline bool IsRunning() const								{ return _IsRunning(); }


			/*! @brief Set up the output for use with decoder, adjusting format and channel layout accordingly */
			bool SetupForDecoder(const Decoder& decoder);

			/*! @brief Get the preferred buffer size, or 0 if none */
			size_t GetPreferredBufferSize() const;

			//@}


			/*! @brief Get the player owning this Output */
			inline void SetPlayer(Player * player)					{ mPlayer = player; }

			/*! @brief Set the player owning this Output */
			inline Player * GetPlayer() const						{ return mPlayer; }


			/*! @brief Create a new \c Output and initialize \c Output::mPlayer to \c nullptr */
			Output();

			AudioFormat			mFormat;			/*!< @brief The required format for audio passed to this \c Output */
			ChannelLayout		mChannelLayout;		/*!< @brief The required channel layout for audio passed to this \c Output */

			Player				*mPlayer;			/*!< @brief Weak reference to owning player */

		private:

			// ========================================
			// Callbacks
			FormatBlock								mPrepareForFormatBlock;

			// ========================================
			// Subclasses must implement the following methods
			virtual bool _Open() = 0;
			virtual bool _Close() = 0;

			virtual bool _Start() = 0;
			virtual bool _Stop() = 0;
			virtual bool _RequestStop() = 0;

			virtual bool _IsOpen() const = 0;
			virtual bool _IsRunning() const = 0;

			virtual bool _Reset() = 0;

			virtual bool _SetupForDecoder(const Decoder& decoder) = 0;

			virtual bool _SupportsFormat(const AudioFormat& format) const = 0;

			// ========================================
			// Optional methods
			virtual bool _CreateDeviceUID(CFStringRef& /*deviceUID*/) const		{ return false; }
			virtual bool _SetDeviceUID(CFStringRef /*deviceUID*/)				{ return false; }

			virtual bool _GetDeviceSampleRate(Float64& /*sampleRate*/) const	{ return false; }
			virtual bool _SetDeviceSampleRate(Float64 /*sampleRate*/)			{ return false; }

			virtual size_t _GetPreferredBufferSize() const						{ return 0; }
		};
	}
}
