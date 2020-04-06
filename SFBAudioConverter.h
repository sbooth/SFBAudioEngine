/*
 * Copyright (c) 2011 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <Foundation/Foundation.h>

#import "SFBAudioDecoder.h"

NS_ASSUME_NONNULL_BEGIN

/*! @brief Thin wrapper around \c AudioConverter */
@interface SFBAudioConverter : NSObject

- (instancetype)init NS_UNAVAILABLE;

/*!
 * @brief Returns an initialized  \c SFBAudioConverter object with the specified output format, or \c nil on failure
 * @param decoder The \c SFBAudioDecoder providing audio data to be converted
 * @param outputFormat The desired output format
 */
- (nullable instancetype)initWithDecoder:(SFBAudioDecoder *)decoder outputFormat:(SFBAudioFormat *)outputFormat;

/*!
 * @brief Returns an initialized  \c SFBAudioConverter object with the specified output format, or \c nil on failure
 * @param decoder The \c SFBAudioDecoder providing audio data to be converted
 * @param outputFormat The desired output format
 * @param error An optional pointer to a \c NSError to receive error information
 */
- (nullable instancetype)initWithDecoder:(SFBAudioDecoder *)decoder outputFormat:(SFBAudioFormat *)outputFormat error:(NSError * _Nullable *)error;

/*!
 * @brief Returns an initialized  \c SFBAudioConverter object with the specified output format, or \c nil on failure
 * @param decoder The \c SFBAudioDecoder providing audio data to be converted
 * @param outputFormat The desired output format
 * @param preferredBufferSize The preferred buffer size, in frames
 * @param error An optional pointer to a \c NSError to receive error information
 */
- (nullable instancetype)initWithDecoder:(SFBAudioDecoder *)decoder outputFormat:(SFBAudioFormat *)outputFormat preferredBufferSize:(NSInteger)preferredBufferSize error:(NSError * _Nullable *)error NS_DESIGNATED_INITIALIZER;

/*!
 * @brief Converts audio into the specified buffer
 * @param bufferList A buffer to receive the converted audio
 * @param frameCount The requested number of audio frames
 * @param framesConverted The actual number of frames converted
 * @param error An optional pointer to a \c NSError to receive error information
 * @return \c YES on success, \c NO otherwise
 */
- (BOOL)convertAudio:(SFBAudioBufferList *)bufferList frameCount:(NSInteger)frameCount framesConverted:(NSInteger *)framesConverted error:(NSError * _Nullable *)error;

/*! @brief Reset the \c SFBAudioConverter internal state */
- (BOOL)reset;

/*! @brief Returns the number of valid audio frames in  this \c SFBAudioConverter */
@property (nonatomic, readonly) SFBAudioDecoder *decoder;

/*! @brief Returns the output format of this \c SFBAudioConverter */
@property (nonatomic, readonly) SFBAudioFormat *outputFormat;

@end

NS_ASSUME_NONNULL_END
