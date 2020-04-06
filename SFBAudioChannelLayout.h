/*
 * Copyright (c) 2013 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <CoreAudio/CoreAudioTypes.h>
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

/*! @brief Immutable thin wrapper around \c AudioChannelLayout */
@interface SFBAudioChannelLayout : NSObject <NSCopying>

/*! @brief Mono layout */
@property (nonatomic, class, readonly) SFBAudioChannelLayout *mono;

/*! @brief Stereo layout */
@property (nonatomic, class, readonly) SFBAudioChannelLayout *stereo;

/*!
 * @brief Returns an initialized  \c SFBAudioChannelLayout object for the specified layout tag
 * @param tag The layout tag for the channel layout
 */
- (instancetype)initWithTag:(AudioChannelLayoutTag)tag;

/*!
 * @brief Returns an initialized  \c SFBAudioChannelLayout object for the specified channel bitmap
 * @param bitmap The channel bitmap for the channel layout
 */
- (instancetype)initWithBitmap:(AudioChannelBitmap)bitmap;

/*!
 * @brief Returns an initialized  \c SFBAudioChannelLayout object with the specified channel labels
 * @param labels The channel labels for the channel layout
 */
- (instancetype)initWithLabels:(NSArray<NSNumber *> *)labels;

/*!
 * @brief Returns an initialized  \c SFBAudioChannelLayout object for the specified \c AudioChannelLayout
 * @param layout The \c AudioChannelLayout to copy
 */
- (instancetype)initWithLayout:(const AudioChannelLayout *)layout;

/*! @brief Returns \c YES if  this \c SFBAudioChannelLayout is equivalent to \c layout */
- (BOOL)isEquivalentToLayout:(const AudioChannelLayout *)layout;

/*! @brief Returns  the number of channels  in this channel layout */
@property (nonatomic, readonly) NSInteger channelCount;

@property (nonatomic, readonly) BOOL isMono;
@property (nonatomic, readonly) BOOL isStereo;

/*!
 * @brief Returns a channel map for mapping audio channels from this channel layout to another
 * @param outputLayout The output channel layout
 * @return The channel map or \c nil on error
 */
- (NSArray<NSNumber *> *)mapToLayout:(SFBAudioChannelLayout *)layout;

/*! @brief Returns a \c const pointer to this object's internal \c AudioChannelLayout */
@property (nonatomic, nullable, readonly) const AudioChannelLayout *layout NS_RETURNS_INNER_POINTER;

@end

NS_ASSUME_NONNULL_END
