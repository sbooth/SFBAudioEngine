/*
 * Copyright (c) 2013 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#pragma once

#import <vector>

#import <AudioToolbox/AudioToolbox.h>

#import "CFWrapper.h"

/*! @file AudioChannelLayout.h @brief A Core %Audio \c AudioChannelLayout wrapper  */

/*! @brief \c SFBAudioEngine's encompassing namespace */
namespace SFB {

	/*! @brief %Audio functionality */
	namespace Audio {

		/*!
		 * @brief Returns the size of an \c AudioChannelLayout struct
		 * @param channelLayout A pointer to an \c AudioChannelLayout struct
		 * @return The size of \c channelLayout in bytes
		 */
		size_t ChannelLayoutSize(const AudioChannelLayout *channelLayout);

		/*! @brief A class wrapping a Core %Audio \c AudioChannelLayout */
		class ChannelLayout
		{
		public:

			/*! @brief Mono layout */
			static const ChannelLayout Mono;

			/*! @brief Stereo layout */
			static const ChannelLayout Stereo;

			// ========================================
			/*! @name Factory Methods */
			//@{

			/*!
			 * @brief Create a \c ChannelLayout
			 * @param layoutTag The layout tag for the channel layout
			 * @return A \c ChannelLayout
			 */
			static ChannelLayout ChannelLayoutWithTag(AudioChannelLayoutTag layoutTag);

			/*!
			 * @brief Create a \c ChannelLayout
			 * @param channelLabels A \c std::vector of the desired channel labels
			 * @return A \c ChannelLayout
			 */
			static ChannelLayout ChannelLayoutWithChannelLabels(std::vector<AudioChannelLabel> channelLabels);

			/*!
			 * @brief Create a \c ChannelLayout
			 * @param channelBitmap The channel bitmap for the channel layout
			 * @return A \c ChannelLayout
			 */
			static ChannelLayout ChannelLayoutWithBitmap(UInt32 channelBitmap);

			//@}


			// ========================================
			/*! @name Creation and Destruction */
			//@{

			/*! @brief Create a new, empty \c ChannelLayout */
			ChannelLayout();

			/*! @brief Destroy the \c ChannelLayout and release all associated resources. */
			~ChannelLayout();

			/*! @brief Create a new \c ChannelLayout by performing a deep copy of \c channelLayout */
			ChannelLayout(const AudioChannelLayout *channelLayout);

			/*! @cond */

			/*! @internal Move constructor */
			ChannelLayout(ChannelLayout&& rhs);

			/*! @internal Move assignment operator */
			ChannelLayout& operator=(ChannelLayout&& rhs);

			/*! @internal Copy constructor */
			ChannelLayout(const ChannelLayout& rhs);

			/*! @internal Assignment operator */
			ChannelLayout& operator=(const ChannelLayout& rhs);

			/*! @brief Makes a deep copy of rhs */
			ChannelLayout& operator=(const AudioChannelLayout *rhs);

			/*! @endcond */
			//@}


			// ========================================
			/*! @name Functionality */
			//@{

			/*! @brief Get the number of channels contained in this channel layout */
			size_t ChannelCount() const;

			/*!
			 * @brief Create a channel map for converting audio from this channel layout
			 * @param outputLayout The output channel layout
			 * @param channelMap A \c std::vector to receive the channel map on success
			 * @return \c true on success, \c false otherwise
			 */
			bool MapToLayout(const ChannelLayout& outputLayout, std::vector<SInt32>& channelMap) const;

			//@}


			// ========================================
			/*! @name AudioChannelLayout access */
			//@{

			/*! @brief Retrieve a const pointer to this object's internal \c AudioChannelLayout */
			inline const AudioChannelLayout * Layout() const		{ return mChannelLayout; }


			/*! @brief Query whether this \c ChannelLayout is empty */
			inline explicit operator bool() const					{ return mChannelLayout != nullptr; }

			/*! @brief Query whether this \c ChannelLayout is not empty */
			inline bool operator!() const							{ return mChannelLayout == nullptr; }


			/*! @brief Retrieve a const pointer to this object's internal \c AudioChannelLayout */
			inline const AudioChannelLayout * operator->() const	{ return mChannelLayout; }

			/*! @brief Retrieve a const pointer to this object's internal \c AudioChannelLayout */
			inline operator const AudioChannelLayout *() const		{ return mChannelLayout; }


			/*! @brief Compare two \c ChannelLayout objects for equality*/
			bool operator==(const ChannelLayout& rhs) const;

			/*! @brief Compare two \c ChannelLayout objects for inequality*/
			inline bool operator!=(const ChannelLayout& rhs) const { return !operator==(rhs); }

			//@}

			/*! @brief Returns a string representation of this channel layout suitable for logging */
			CFString Description() const;

		private:
			AudioChannelLayout *mChannelLayout;
		};

	}
}
