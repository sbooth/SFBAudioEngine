/*
 * Copyright (c) 2011 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#include <AudioToolbox/AudioToolbox.h>

#include "AudioDecoder.h"

/*! @file AudioConverter.h @brief Support for converting audio from one PCM format to another */

/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	/*! @brief %Audio functionality */
	namespace Audio {

		/*! @brief A \c Converter converts the output of a \c Decoder to a different PCM format */
		class Converter
		{
		public:

			// ========================================
			/*! @name Creation and Destruction */
			//@{

			/*!
			 * @brief Create a new \c Converter
			 * @note The \c Converter will take ownership of \c decoder
			 * @param decoder The \c AudioDecoder providing the input
			 * @param format The desired output format
			 * @param channelLayout The desired output channel layout or \c nullptr if not specified
			 */
			Converter(Decoder::unique_ptr decoder, const AudioStreamBasicDescription& format, ChannelLayout channelLayout = nullptr);

			/*! @brief Destroy this \c Converter */
			~Converter();

			/*! @cond */

			/*! @internal This class is non-copyable */
			Converter(const Converter& rhs) = delete;

			/*! @internal This class is non-assignable */
			Converter& operator=(const Converter& rhs) = delete;

			/*! @endcond */
			//@}


			// ========================================
			/*! @name Opening and closing */
			//@{

			/*!
			 * @brief Open the converter's \c Decoder and set up for conversion
			 * @param preferredBufferSizeFrames The anticipated number of frames to be requested in \c ConvertAudio
			 * @param error An optional pointer to a \c CFErrorRef to receive error information
			 * @return \c true on success, \c false otherwise
			 * @see SFB::Audio::Decoder::Open()
			 */
			bool Open(UInt32 preferredBufferSizeFrames = 512, CFErrorRef *error = nullptr);

			/*!
			 * @brief Close the converter
			 * @param error An optional pointer to a \c CFErrorRef to receive error information
			 * @return \c true on success, \c false otherwise
			 * @see SFB::Audio::Decoder::Close()
			 */
			bool Close(CFErrorRef *error = nullptr);

			/*! @brief Query whether this converter is open */
			inline bool IsOpen() const									{ return mIsOpen; }

			//@}


			// ========================================
			/*! @name Internals */
			//@{

			/*! @brief Get the \c Decoder feeding this converter */
			inline const Decoder& GetDecoder() const					{ return *mDecoder; }

			//@}


			// ========================================
			/*! @name Audio access */
			//@{

			/*! @brief Get the type of PCM data provided by this converter */
			inline AudioStreamBasicDescription GetFormat() const		{ return mFormat; }

			/*!
			 * @brief Create a description of the type of PCM data provided by this converter
			 * @note The returned string must be released by the caller
			 * @return A description of the type of PCM data provided by this converter
			 */
			CFStringRef CreateFormatDescription() const;


			/*! @brief Get the layout of the converter's audio channels, or \c nullptr if not specified */
			inline const ChannelLayout& GetChannelLayout() const		{ return mChannelLayout; }

			/*!
			 * @brief Create a description of the layout of the converter's audio channels
			 * @note The returned string must be released by the caller
			 * @return A description of the layout of the converter's audio channels
			 */
			CFStringRef CreateChannelLayoutDescription() const;


			/*!
			 * @brief Convert audio into the specified buffer
			 * @param bufferList A buffer to receive the decoded audio
			 * @param frameCount The requested number of audio frames
			 * @return The actual number of frames converted, or \c 0 on error
			 */
			UInt32 ConvertAudio(AudioBufferList *bufferList, UInt32 frameCount);

			/*! @brief Reset the internal conversion state */
			bool Reset();


			//@}


			/*! @cond */

			/*! @internal This method is exposed so it can be used inside C callbacks */
			UInt32 DecodeAudio(AudioBufferList *bufferList, UInt32 frameCount);

			/*! @endcond */

		private:

			AudioStreamBasicDescription			mFormat;			/*!< The format produced by this converter */
			ChannelLayout						mChannelLayout;		/*!< The channel layout of the audio produced by this converter */
			Decoder::unique_ptr					mDecoder;			/*!< The Decoder providing the audio */
			AudioConverterRef					mConverter;			/*!< The actual object performing the conversion */
			BufferList 							mBufferList;		/*!< Buffer for decoded audio pending conversion */
			bool								mIsOpen;			/*!< Flag indicating if \c mConverter is open */
		};

	}
}
