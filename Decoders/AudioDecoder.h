/*
 *  Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013, 2014, 2015 Stephen F. Booth <me@sbooth.org>
 *  All Rights Reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
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

#include <CoreFoundation/CoreFoundation.h>
#include <CoreAudio/CoreAudioTypes.h>

#include <memory>
#include <vector>
#include <algorithm>

#include "InputSource.h"
#include "AudioFormat.h"
#include "AudioChannelLayout.h"

/*! @file AudioDecoder.h @brief Support for decoding audio to PCM */

/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	/*! @brief %Audio functionality */
	namespace Audio {

		/*!
		 * @brief Base class for all audio decoder classes
		 *
		 * An AudioDecoder is responsible for reading audio data in some format and providing
		 * it in a PCM format that is handled by an \c AudioConverter
		 */
		class Decoder
		{

		public:
			
			/*! @brief The \c CFErrorRef error domain used by \c Decoder and subclasses */
			static const CFStringRef ErrorDomain;

			/*! @brief Possible \c CFErrorRef error codes used by \c Decoder */
			enum ErrorCode {
				FileFormatNotRecognizedError		= 0,	/*!< File format not recognized */
				FileFormatNotSupportedError			= 1,	/*!< File format not supported */
				InputOutputError					= 2		/*!< Input/output error */
			};

			// ========================================
			/*! @name Supported file formats */
			//@{

			/*!
			 * @brief Create an array containing the supported file extensions
			 * @note The returned array must be released by the caller
			 * @return An array containing the supported file extensions
			 */
			static CFArrayRef CreateSupportedFileExtensions();

			/*!
			 * @brief Create an array containing the supported MIME types
			 * @note The returned array must be released by the caller
			 * @return An array containing the supported MIME types
			 */
			static CFArrayRef CreateSupportedMIMETypes();


			/*! @brief Test whether a file extension is supported */
			static bool HandlesFilesWithExtension(CFStringRef extension);

			/*! @brief Test whether a MIME type is supported */
			static bool HandlesMIMEType(CFStringRef mimeType);

			//@}


			// ========================================
			/*! @name Factory Methods */
			//@{

			/*! @brief A \c std::unique_ptr for \c Decoder objects */
			using unique_ptr = std::unique_ptr<Decoder>;

			/*!
			 * @brief Create a \c Decoder object for the specified URL
			 * @param url The URL
			 * @param error An optional pointer to a \c CFErrorRef to receive error information
			 * @return A \c Decoder object, or \c nullptr on failure
			 */
			static unique_ptr CreateForURL(CFURLRef url, CFErrorRef *error = nullptr);

			/*!
			 * @brief Create a \c Decoder object for the specified URL
			 * @note The MIME type takes precedence over the file extension for type resolution
			 * @param url The URL
			 * @param mimeType The MIME type of the audio
			 * @param error An optional pointer to a \c CFErrorRef to receive error information
			 * @return A \c Decoder object, or \c nullptr on failure
			 */
			static unique_ptr CreateForURL(CFURLRef url, CFStringRef mimeType, CFErrorRef *error = nullptr);


			/*!
			 * @brief Create a \c Decoder object for the specified \c InputSource
			 * @note The decoder will take ownership of the input source on success
			 * @param inputSource The input source
			 * @param error An optional pointer to a \c CFErrorRef to receive error information
			 * @return A \c Decoder object, or \c nullptr on failure
			 */
			static unique_ptr CreateForInputSource(InputSource::unique_ptr inputSource, CFErrorRef *error = nullptr);

			/*!
			 * @brief Create a \c Decoder object for the specified \c InputSource
			 * @note The MIME type takes precedence over the file extension for type resolution
			 * @note The decoder will take ownership of the input source on success
			 * @param inputSource The input source
			 * @param mimeType The MIME type of the audio
			 * @param error An optional pointer to a \c CFErrorRef to receive error information
			 * @return A \c Decoder object, or \c nullptr on failure
			 */
			static unique_ptr CreateForInputSource(InputSource::unique_ptr inputSource, CFStringRef mimeType, CFErrorRef *error = nullptr);

			//@}


			// ========================================
			/*!
			 * @name Automatic opening behavior
			 * If \c AutomaticallyOpenDecoders() returns \c true then the factory methods will attempt to open the \c InputSource
			 */
			//@{

			/*! @brief Query whether decoders should be automatically opened */
			static inline bool AutomaticallyOpenDecoders()				{ return sAutomaticallyOpenDecoders.load(); }

			/*! @brief Set whether decoders should be automatically opened */
			static inline void SetAutomaticallyOpenDecoders(bool flag)	{ sAutomaticallyOpenDecoders.store(flag); }

			//@}


			// ========================================
			/*! @name Creation and Destruction */
			//@{

			/*! @brief Destroy this \c Decoder */
			virtual ~Decoder() = default;

			/*! @cond */

			/*! @internal This class is non-copyable */
			Decoder(const Decoder& rhs) = delete;

			/*! @internal This class is non-assignable */
			Decoder& operator=(const Decoder& rhs) = delete;

			/*! @endcond */
			//@}


			// ========================================
			/*!
			 * @name Represented object association
			 * A represented object allows a decoder to be associated with a model object such as
			 * a playlist or track
			 */
			//@{

			/*! @brief Get the represented object associated with this decoder */
			inline void * GetRepresentedObject() const					{ return mRepresentedObject; }

			/*! @brief Set the represented object associated with this decoder */
			inline void SetRepresentedObject(void *representedObject)	{ mRepresentedObject = representedObject; }

			//@}


			// ========================================
			/*! @name Source access */
			//@{

			/*! @brief Get the URL associated with this decoder's \c InputSource */
			inline CFURLRef GetURL() const								{ return _GetURL(); }

			/*! @brief Get the \c InputSource feeding this decoder */
			inline InputSource& GetInputSource() const					{ return _GetInputSource(); }

			//@}


			// ========================================
			/*! @name File access */
			//@{

			/*!
			 * @brief Open the decoder's \c InputSource
			 * @param error An optional pointer to a \c CFErrorRef to receive error information
			 * @return \c true on success, \c false otherwise
			 * @see InputSource::Open()
			 */
			bool Open(CFErrorRef *error = nullptr);

			/*!
			 * @brief Close the decoder's \c InputSource
			 * @param error An optional pointer to a \c CFErrorRef to receive error information
			 * @return \c true on success, \c false otherwise
			 * @see InputSource::Close()
			 */
			bool Close(CFErrorRef *error = nullptr);

			/*! @brief Query the decoder's \c InputSource to determine if it is open */
			inline bool IsOpen() const									{ return mIsOpen; }

			//@}


			// ========================================
			/*! @name Audio access */
			//@{

			/*! @brief Get the native format of the source audio */
			inline const AudioFormat& GetSourceFormat() const 	{ return mSourceFormat; }

			/*!
			 * @brief Create a description of the source audio's native format
			 * @note The returned string must be released by the caller
			 * @return A description of the source audio's native format
			 */
			CFStringRef CreateSourceFormatDescription() const;


			/*! @brief Get the type of PCM data provided by this decoder */
			inline const AudioFormat& GetFormat() const		{ return mFormat; }

			/*!
			 * @brief Create a description of the type of PCM data provided by this decoder
			 * @note The returned string must be released by the caller
			 * @return A description of the type of PCM data provided by this decoder
			 */
			CFStringRef CreateFormatDescription() const;


			/*! @brief Get the layout of the decoder's audio channels, or \c nullptr if not specified */
			inline const ChannelLayout& GetChannelLayout() const		{ return mChannelLayout; }

			/*!
			 * @brief Create a description of the layout of the decoder's audio channels
			 * @note The returned string must be released by the caller
			 * @return A description of the layout of the decoder's audio channels
			 */
			CFStringRef CreateChannelLayoutDescription() const;


			/*!
			 * @brief Decode audio into the specified buffer
			 * @param bufferList A buffer to receive the decoded audio
			 * @param frameCount The requested number of audio frames
			 * @return The actual number of frames read, or \c 0 on error
			 */
			UInt32 ReadAudio(AudioBufferList *bufferList, UInt32 frameCount);


			/*! @brief Get the total number of audio frames */
			SInt64 GetTotalFrames() const ;

			/*! @brief Get the current audio frame */
			SInt64 GetCurrentFrame() const;

			/*! @brief Get the number of audio frames remaining */
			inline SInt64 GetFramesRemaining() const					{ return GetTotalFrames() - GetCurrentFrame(); }


			/*! @brief Query whether the audio format and input source support seeking */
			bool SupportsSeeking() const;

			/*!
			 * @brief Seek to the specified audio frame
			 * @param frame The desired audio frame
			 * @return The current frame after seeking
			 */
			SInt64 SeekToFrame(SInt64 frame);

			//@}

		protected:

			InputSource::unique_ptr			mInputSource;		/*!< @brief The input source feeding this decoder */

			AudioFormat						mFormat;			/*!< @brief The type of PCM data provided by this decoder */
			ChannelLayout					mChannelLayout;		/*!< @brief The channel layout for the PCM data, or \c nullptr if unknown or unspecified */

			AudioFormat						mSourceFormat;		/*!< @brief The native format of the source file */


			/*! @brief Create a new \c Decoder and initialize \c Decoder::mInputSource to \c nullptr */
			Decoder();

			/*! @brief Create a new \c Decoder and initialize \c Decoder::mInputSource to \c inputSource */
			Decoder(InputSource::unique_ptr inputSource);

		private:

			// Override these carefully
			inline virtual CFURLRef _GetURL() const						{ return mInputSource->GetURL(); }
			inline virtual InputSource& _GetInputSource() const			{ return *mInputSource; }

			// Subclasses must implement these methods
			virtual SFB::CFString _GetSourceFormatDescription() const = 0;

			virtual bool _Open(CFErrorRef *error) = 0;
			virtual bool _Close(CFErrorRef *error) = 0;

			virtual UInt32 _ReadAudio(AudioBufferList *bufferList, UInt32 frameCount) = 0;

			virtual SInt64 _GetTotalFrames() const = 0;
			virtual SInt64 _GetCurrentFrame() const = 0;

			// Optional seeking support
			virtual bool _SupportsSeeking() const						{ return false; }
			virtual SInt64 _SeekToFrame(SInt64 /*frame*/)				{ return -1; }

			// Data members
			void							*mRepresentedObject;
			bool							mIsOpen;

			// ========================================
			// Controls whether Open() is called for decoders created in the factory methods
			static std::atomic_bool			sAutomaticallyOpenDecoders;

			// ========================================
			// Subclass registration support
			struct SubclassInfo
			{
				CFArrayRef (*mCreateSupportedFileExtensions)();
				CFArrayRef (*mCreateSupportedMIMETypes)();

				bool (*mHandlesFilesWithExtension)(CFStringRef);
				bool (*mHandlesMIMEType)(CFStringRef);

				Decoder::unique_ptr (*mCreateDecoder)(InputSource::unique_ptr);

				int mPriority;
			};

			static std::vector <SubclassInfo> sRegisteredSubclasses;

		public:

			/*!
			 * @brief Register a \c Decoder subclass
			 * @tparam T The subclass name
			 * @param priority The priority of the subclass
			 */
			template <typename T> static void RegisterSubclass(int priority = 0);
			
		};
		
		// ========================================
		// Template implementation
		template <typename T> void Decoder::RegisterSubclass(int priority)
		{
			SubclassInfo subclassInfo = {
				.mCreateSupportedFileExtensions = T::CreateSupportedFileExtensions,
				.mCreateSupportedMIMETypes = T::CreateSupportedMIMETypes,
				
				.mHandlesFilesWithExtension = T::HandlesFilesWithExtension,
				.mHandlesMIMEType = T::HandlesMIMEType,
				
				.mCreateDecoder = T::CreateDecoder,
				
				.mPriority = priority
			};
			
			sRegisteredSubclasses.push_back(subclassInfo);
			
			// Sort subclasses by priority
			std::sort(sRegisteredSubclasses.begin(), sRegisteredSubclasses.end(), [](const SubclassInfo& a, const SubclassInfo& b) {
				return a.mPriority > b.mPriority;
			});
		}
		
	}
}
