/*
 * Copyright (c) 2013 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <CoreAudio/CoreAudioTypes.h>
#import <Foundation/Foundation.h>

#import "SFBAudioFormat.h"

NS_ASSUME_NONNULL_BEGIN

/*! @brief Thin wrapper around \c AudioBufferList */
@interface SFBAudioBufferList : NSObject

- (instancetype)init NS_UNAVAILABLE;

/*!
 * @brief Returns an initialized  \c SFBAudioBufferList object with the specified frame capacity, or \c nil if memory allocation fails
 * @param format The format of the audio the \c SFBAudioBufferList will hold
 * @param capacityFrames The desired buffer capacity in audio frames
 */
- (nullable instancetype)initWithFormat:(SFBAudioFormat *)format capacityFrames:(NSInteger)capacityFrames NS_DESIGNATED_INITIALIZER;

/*!
 * @brief Reset the \c SFBAudioBufferList to the default state in preparation for reading
 * This will set the \c mDataByteSize of each \c AudioBuffer to `[format frameCountToByteCount:self.capacityFrames]`
 */
- (void)reset;

/*!
 * @brief Empty the \c SFBAudioBufferList
 * This will set the \c mDataByteSize of each \c AudioBuffer to 0
 */
- (void)empty;

/*! @brief Returns the capacity of this \c SFBAudioBufferList in audio frames */
@property (nonatomic, readonly) NSInteger capacityFrames NS_SWIFT_NAME(capacity);

/*! @brief Returns the number of valid audio frames in  this \c SFBAudioBufferList */
@property (nonatomic, readonly) NSInteger framesInBuffer  NS_SWIFT_NAME(validFrames);

/*! @brief Get the format of this \c SFBAudioBufferList */
@property (nonatomic, readonly) SFBAudioFormat *format;

/*! @brief Returns a pointer to this object's internal \c AudioBufferList */
@property (nonatomic, nullable, readonly) AudioBufferList *bufferList NS_RETURNS_INNER_POINTER;

@end

NS_ASSUME_NONNULL_END
