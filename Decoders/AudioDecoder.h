/*
 *  Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011, 2012, 2013 Stephen F. Booth <me@sbooth.org>
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

#include <CoreFoundation/CoreFoundation.h>
#include <CoreAudio/CoreAudioTypes.h>

#include <vector>
#include <algorithm>

#include "InputSource.h"

/*! @file AudioDecoder.h @brief Support for decoding audio to PCM */

/*! @brief The \c CFErrorRef error domain used by \c AudioDecoder */
extern const CFStringRef		AudioDecoderErrorDomain;

/*! @brief Possible \c CFErrorRef error codes used by \c AudioDecoder */
enum {
	AudioDecoderFileFormatNotRecognizedError		= 0,	/*!< File format not recognized */
	AudioDecoderFileFormatNotSupportedError			= 1,	/*!< File format not supported */
	AudioDecoderInputOutputError					= 2		/*!< Input/output error */
};


/*!
 * @brief Base class for all audio decoder classes
 *
 * An AudioDecoder is responsible for reading audio data in some format and providing
 * it in a PCM format that is handled by an \c AudioConverter
 */
class AudioDecoder
{

public:

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

	/*!
	 * @brief Create an \c AudioDecoder object for the specified URL
	 * @param url The URL 
	 * @param error An optional pointer to a \c CFErrorRef to receive error information
	 * @return An \c AudioDecoder object, or \c nullptr on failure
	 */
	static AudioDecoder * CreateDecoderForURL(CFURLRef url, CFErrorRef *error = nullptr);

	/*!
	 * @brief Create an \c AudioDecoder object for the specified URL
	 * @note The MIME type takes precedence over the file extension for type resolution
	 * @param url The URL
	 * @param mimeType The MIME type of the audio
	 * @param error An optional pointer to a \c CFErrorRef to receive error information
	 * @return An \c AudioDecoder object, or \c nullptr on failure
	 */
	static AudioDecoder * CreateDecoderForURL(CFURLRef url, CFStringRef mimeType, CFErrorRef *error = nullptr);


	/*!
	 * @brief Create an \c AudioDecoder object for the specified \c InputSource
	 * @note The decoder will take ownership of the input source on success
	 * @param inputSource The input source
	 * @param error An optional pointer to a \c CFErrorRef to receive error information
	 * @return An \c AudioDecoder object, or \c nullptr on failure
	 */
	static AudioDecoder * CreateDecoderForInputSource(InputSource *inputSource, CFErrorRef *error = nullptr);

	/*!
	 * @brief Create an \c AudioDecoder object for the specified \c InputSource
	 * @note The MIME type takes precedence over the file extension for type resolution
	 * @note The decoder will take ownership of the input source on success
	 * @param inputSource The input source
	 * @param mimeType The MIME type of the audio
	 * @param error An optional pointer to a \c CFErrorRef to receive error information
	 * @return An \c AudioDecoder object, or \c nullptr on failure
	 */
	static AudioDecoder * CreateDecoderForInputSource(InputSource *inputSource, CFStringRef mimeType, CFErrorRef *error = nullptr);


	/*!
	 * @brief Create an \c AudioDecoder object for a region of the specified URL
	 * @param url The URL
	 * @param startingFrame The first frame to decode
	 * @param error An optional pointer to a \c CFErrorRef to receive error information
	 * @return An \c AudioDecoder object, or \c nullptr on failure
	 */
	static AudioDecoder * CreateDecoderForURLRegion(CFURLRef url, SInt64 startingFrame, CFErrorRef *error = nullptr);

	/*!
	 * @brief Create an \c AudioDecoder object for a region of the specified URL
	 * @param url The URL
	 * @param startingFrame The first frame to decode
	 * @param frameCount The number of frames to decode
	 * @param error An optional pointer to a \c CFErrorRef to receive error information
	 * @return An \c AudioDecoder object, or \c nullptr on failure
	 */
	static AudioDecoder * CreateDecoderForURLRegion(CFURLRef url, SInt64 startingFrame, UInt32 frameCount, CFErrorRef *error = nullptr);

	/*!
	 * @brief Create an \c AudioDecoder object for a region of the specified URL
	 * @param url The URL
	 * @param startingFrame The first frame to decode
	 * @param frameCount The number of frames to decode
	 * @param repeatCount The number of times to repeat
	 * @param error An optional pointer to a \c CFErrorRef to receive error information
	 * @return An \c AudioDecoder object, or \c nullptr on failure
	 */
	static AudioDecoder * CreateDecoderForURLRegion(CFURLRef url, SInt64 startingFrame, UInt32 frameCount, UInt32 repeatCount, CFErrorRef *error = nullptr);


	/*!
	 * @brief Create an \c AudioDecoder object for a region of the specified \c InputSource
	 * @param inputSource The input source
	 * @param startingFrame The first frame to decode
	 * @param error An optional pointer to a \c CFErrorRef to receive error information
	 * @return An \c AudioDecoder object, or \c nullptr on failure
	 */
	static AudioDecoder * CreateDecoderForInputSourceRegion(InputSource *inputSource, SInt64 startingFrame, CFErrorRef *error = nullptr);

	/*!
	 * @brief Create an \c AudioDecoder object for a region of the specified \c InputSource
	 * @param inputSource The input source
	 * @param startingFrame The first frame to decode
	 * @param frameCount The number of frames to decode
	 * @param error An optional pointer to a \c CFErrorRef to receive error information
	 * @return An \c AudioDecoder object, or \c nullptr on failure
	 */
	static AudioDecoder * CreateDecoderForInputSourceRegion(InputSource *inputSource, SInt64 startingFrame, UInt32 frameCount, CFErrorRef *error = nullptr);

	/*!
	 * @brief Create an \c AudioDecoder object for a region of the specified \c InputSource
	 * @param inputSource The input source
	 * @param startingFrame The first frame to decode
	 * @param frameCount The number of frames to decode
	 * @param repeatCount The number of times to repeat
	 * @param error An optional pointer to a \c CFErrorRef to receive error information
	 * @return An \c AudioDecoder object, or \c nullptr on failure
	 */
	static AudioDecoder * CreateDecoderForInputSourceRegion(InputSource *inputSource, SInt64 startingFrame, UInt32 frameCount, UInt32 repeatCount, CFErrorRef *error = nullptr);


	/*!
	 * @brief Create an \c AudioDecoder object for a region of the specified \c AudioDecoder
	 * @param decoder The decoder
	 * @param startingFrame The first frame to decode
	 * @param error An optional pointer to a \c CFErrorRef to receive error information
	 * @return An \c AudioDecoder object, or \c nullptr on failure
	 */
	static AudioDecoder * CreateDecoderForDecoderRegion(AudioDecoder *decoder, SInt64 startingFrame, CFErrorRef *error = nullptr);

	/*!
	 * @brief Create an \c AudioDecoder object for a region of the specified \c AudioDecoder
	 * @param decoder The decoder
	 * @param startingFrame The first frame to decode
	 * @param frameCount The number of frames to decode
	 * @param error An optional pointer to a \c CFErrorRef to receive error information
	 * @return An \c AudioDecoder object, or \c nullptr on failure
	 */
	static AudioDecoder * CreateDecoderForDecoderRegion(AudioDecoder *decoder, SInt64 startingFrame, UInt32 frameCount, CFErrorRef *error = nullptr);

	/*!
	 * @brief Create an \c AudioDecoder object for a region of the specified \c AudioDecoder
	 * @param decoder The decoder
	 * @param startingFrame The first frame to decode
	 * @param frameCount The number of frames to decode
	 * @param repeatCount The number of times to repeat
	 * @param error An optional pointer to a \c CFErrorRef to receive error information
	 * @return An \c AudioDecoder object, or \c nullptr on failure
	 */
	static AudioDecoder * CreateDecoderForDecoderRegion(AudioDecoder *decoder, SInt64 startingFrame, UInt32 frameCount, UInt32 repeatCount, CFErrorRef *error = nullptr);

	//@}


	// ========================================
	/*!
	 * @name Automatic opening behavior
	 * If \c AutomaticallyOpenDecoders() returns \c true then the factory methods will attempt to open the \c InputSource
	 */
	//@{

	/*! @brief Query whether decoders should be automatically opened */
	static inline bool AutomaticallyOpenDecoders()				{ return sAutomaticallyOpenDecoders; }

	/*! @brief Set whether decoders should be automatically opened */
	static inline void SetAutomaticallyOpenDecoders(bool flag)	{ sAutomaticallyOpenDecoders = flag; }

	//@}


	// ========================================
	/*! @name Creation and Destruction */
	//@{

	/*! @brief Destroy this \c AudioDecoder */
	virtual ~AudioDecoder();

	/*! @cond */

	/*! @internal This class is non-copyable */
	AudioDecoder(const AudioDecoder& rhs) = delete;

	/*! @internal This class is non-assignable */
	AudioDecoder& operator=(const AudioDecoder& rhs) = delete;

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
	inline CFURLRef GetURL() const								{ return mInputSource->GetURL(); }
	
	/*! @brief Get the \c InputSource feeding this decoder */
	inline InputSource * GetInputSource() const					{ return mInputSource; }

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
	virtual bool Open(CFErrorRef *error = nullptr) = 0;

	/*!
	 * @brief Close the decoder's \c InputSource
	 * @param error An optional pointer to a \c CFErrorRef to receive error information
	 * @return \c true on success, \c false otherwise
	 * @see InputSource::Close()
	 */
	virtual bool Close(CFErrorRef *error = nullptr) = 0;
	
	/*! @brief Query the decoder's \c InputSource to determine if it is open */
	inline bool IsOpen() const									{ return mIsOpen; }

	//@}


	// ========================================
	/*! @name Audio access */
	//@{

	/*! @brief Get the native format of the source audio */
	inline AudioStreamBasicDescription GetSourceFormat() const 	{ return mSourceFormat; }

	/*!
	 * @brief Create a description of the source audio's native format
	 * @note The returned string must be released by the caller
	 * @return A description of the source audio's native format
	 */
	virtual CFStringRef CreateSourceFormatDescription() const;


	/*! @brief Get the type of PCM data provided by this decoder */
	inline AudioStreamBasicDescription GetFormat() const		{ return mFormat; }

	/*!
	 * @brief Create a description of the type of PCM data provided by this decoder
	 * @note The returned string must be released by the caller
	 * @return A description of the type of PCM data provided by this decoder
	 */
	CFStringRef CreateFormatDescription() const;


	/*! @brief Get the layout of the decoder's audio channels, or \c nullptr if not specified */
	inline AudioChannelLayout * GetChannelLayout() const		{ return mChannelLayout; }

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
	virtual UInt32 ReadAudio(AudioBufferList *bufferList, UInt32 frameCount) = 0;


	/*! @brief Get the total number of audio frames */
	virtual SInt64 GetTotalFrames() const = 0;

	/*! @brief Get the current audio frame */
	virtual SInt64 GetCurrentFrame() const = 0;

	/*! @brief Get the number of audio frames remaining */
	inline SInt64 GetFramesRemaining() const					{ return GetTotalFrames() - GetCurrentFrame(); }


	/*! @brief Query whether the audio format and input source support seeking */
	virtual bool SupportsSeeking() const						{ return false; }

	/*!
	 * @brief Seek to the specified audio frame
	 * @param frame The desired audio frame
	 * @return The current frame after seeking
	 */
	virtual SInt64 SeekToFrame(SInt64 frame)					{ return -1; }

	//@}

protected:

	InputSource						*mInputSource;		/*!< @brief The input source feeding this decoder */

	AudioStreamBasicDescription		mFormat;			/*!< @brief The type of PCM data provided by this decoder */
	AudioChannelLayout				*mChannelLayout;	/*!< @brief The channel layout for the PCM data, or \c nullptr if unknown or unspecified */

	AudioStreamBasicDescription		mSourceFormat;		/*!< @brief The native format of the source file */

	bool							mIsOpen;			/*!< @brief Subclasses should set this to \c true if Open() is successful and \c false if Close() is successful */


	/*! @brief Create a new \c AudioDecoder and initialize \c AudioDecoder::mInputSource to \c nullptr */
	AudioDecoder();

	/*! @brief Create a new \c AudioDecoder and initialize \c AudioDecoder::mInputSource to \c inputSource */
	AudioDecoder(InputSource *inputSource);

private:

	void							*mRepresentedObject;

	// ========================================
	// Controls whether Open() is called for decoders created in the factory methods
	static bool						sAutomaticallyOpenDecoders;

	// ========================================
	// Subclass registration support
	struct SubclassInfo
	{
		CFArrayRef (*mCreateSupportedFileExtensions)();
		CFArrayRef (*mCreateSupportedMIMETypes)();

		bool (*mHandlesFilesWithExtension)(CFStringRef);
		bool (*mHandlesMIMEType)(CFStringRef);

		AudioDecoder * (*mCreateDecoder)(InputSource *);

		int mPriority;
	};

	static std::vector <SubclassInfo> sRegisteredSubclasses;

public:

	/*!
	 * @brief Register an \c AudioDecoder subclass
	 * @tparam The subclass name
	 * @param priority The priority of the subclass
	 */
	template <typename T> static void RegisterSubclass(int priority = 0);

};

// ========================================
// Template implementation
template <typename T> void AudioDecoder::RegisterSubclass(int priority)
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
