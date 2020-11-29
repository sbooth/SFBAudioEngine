/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import <os/log.h>

#import "SFBAudioDecoder.h"

NS_ASSUME_NONNULL_BEGIN

extern os_log_t gSFBAudioDecoderLog;

@interface SFBAudioDecoder ()
{
@package
	SFBInputSource *_inputSource;
@protected
	AVAudioFormat *_sourceFormat;
	AVAudioFormat *_processingFormat;
}
/// Returns the decoder name
@property (class, nonatomic, readonly) SFBAudioDecoderName decoderName;
@end

#pragma mark - Subclass Registration and Lookup

@interface SFBAudioDecoder (SFBAudioDecoderSubclassRegistration)
/// Register a subclass with the default priority (\c 0)
+ (void)registerSubclass:(Class)subclass;
/// Register a subclass with the specified priority
+ (void)registerSubclass:(Class)subclass priority:(int)priority;
@end

@interface SFBAudioDecoder (SFBAudioDecoderSubclassLookup)
/// Returns the appropriate \c SFBAudioDecoder subclass for decoding \c url
+ (nullable Class)subclassForURL:(NSURL *)url;
/// Returns the appropriate \c SFBAudioDecoder subclass for decoding paths with \c extension
+ (nullable Class)subclassForPathExtension:(NSString *)extension;
/// Returns the appropriate \c SFBAudioDecoder subclass for decoding data of \c mimeType
+ (nullable Class)subclassForMIMEType:(NSString *)mimeType;
/// Returns the appropriate \c SFBAudioDecoder subclass corresponding to \c decoderName
+ (nullable Class)subclassForDecoderName:(SFBAudioDecoderName)decoderName;
@end

NS_ASSUME_NONNULL_END
