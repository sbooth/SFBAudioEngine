/*
 *  Copyright (C) 2013, 2014, 2015 Stephen F. Booth <me@sbooth.org>
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

#include <AudioToolbox/AudioToolbox.h>
#include <memory>

#include "AudioFormat.h"

/*! @file AudioBufferList.h @brief A Core Audio \c AudioBufferList wrapper  */

/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	/*! @brief %Audio functionality */
	namespace Audio {

		/*! @brief A class wrapping a Core %Audio \c AudioBufferList */
		class BufferList
		{
		public:
			// ========================================
			/*! @name Creation and Destruction */
			//@{

			/*! @brief Create a new, empty \c BufferList */
			BufferList();

			/*! 
			 * @brief Create a new \c BufferList
			 * @param format The format of the audio the \c BufferList will hold
			 * @param capacityFrames The desired buffer capacity in audio frames
			 * @throws std::bad_alloc
			 */
			BufferList(const AudioFormat& format, UInt32 capacityFrames);

			/*! @cond */

			/*! @internal This class is non-copyable */
			BufferList(const BufferList& rhs) = delete;

			/*! @internal This class is non-assignable */
			BufferList& operator=(const BufferList& rhs) = delete;

			/*! @endcond */

			//@}

			// ========================================
			/*! @name Buffer management */
			//@{

			/*!
			 * @brief Create a new \c BufferList
			 * @param format The format of the audio the \c BufferList will hold
			 * @param capacityFrames The desired buffer capacity in audio frames
			 * @return \c true on sucess, \c false otherwise
			 */
			bool Allocate(const AudioFormat& format, UInt32 capacityFrames);

			/*! @brief Deallocate the memory associated with this \c BufferList */
			bool Deallocate();


			/*!
			 * @brief Reset the \c BufferList to the default state in preparation for reading
			 * This will set the \c mDataByteSize of each \c AudioBuffer to GetFormat().FrameCountToByteCount(GetCapacityFrames())
			 */
			bool Reset();


			/*! @brief Get the capacity of this \c BufferList in audio frames */
			inline UInt32 GetCapacityFrames() const			{ return mCapacityFrames; }

			/*! @brief Get the format of this \c BufferList */
			inline const AudioFormat& GetFormat() const		{ return mFormat; }

			//@}


			// ========================================
			/*! @name AudioBufferList access */
			//@{

			/*! @brief Retrieve a pointer to this object's internal \c AudioBufferList */
			inline AudioBufferList * GetABL()					{ return mBufferList.get(); }

			/*! @brief Retrieve a const pointer to this object's internal \c AudioBufferList */
			inline const AudioBufferList * GetABL() const		{ return mBufferList.get(); }


			/*! @brief Query whether this \c BufferList is empty */
			inline explicit operator bool() const				{ return (bool)mBufferList; }

			/*! @brief Query whether this \c BufferList is not empty */
			inline bool operator!() const						{ return !mBufferList; }


			/*! @brief Retrieve a pointer to this object's internal \c AudioBufferList */
			inline AudioBufferList * operator->()				{ return mBufferList.get(); }

			/*! @brief Retrieve a const pointer to this object's internal \c AudioBufferList */
			inline const AudioBufferList * operator->() const	{ return mBufferList.get(); }

			/*! @brief Retrieve a pointer to this object's internal \c AudioBufferList */
			inline operator AudioBufferList *()					{ return mBufferList.get(); }

			/*! @brief Retrieve a const pointer to this object's internal \c AudioBufferList */
			inline operator const AudioBufferList *() const		{ return mBufferList.get(); }

			//@}

		private:
			
			std::unique_ptr<AudioBufferList, void (*)(AudioBufferList *)> mBufferList;
			AudioFormat mFormat;
			UInt32 mCapacityFrames;
		};

	}
}
