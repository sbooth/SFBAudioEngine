/*
 * Copyright (c) 2006 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#import "SFBAudioDecoder.h"

NS_ASSUME_NONNULL_BEGIN

@interface SFBAudioDecoder ()
{
@package
	SFBInputSource *_inputSource;
	SFBAudioFormat *_sourceFormat;
	AVAudioFormat *_processingFormat;
	AVAudioFramePosition _currentFrame;
	AVAudioFramePosition _totalFrames;
}
@property (nonatomic) SFBAudioFormat *sourceFormat;
@property (nonatomic) AVAudioFormat *processingFormat;
@end

@interface SFBAudioDecoderSubclassInfo : NSObject
@property (nonatomic) Class klass;
@property (nonatomic) int priority;
@end

@interface SFBAudioDecoder (SFBAudioDecoderSubclassLookup)
+ (nullable Class)subclassForURL:(NSURL *)url;
+ (nullable Class)subclassForPathExtension:(NSString *)extension;
+ (nullable Class)subclassForMIMEType:(NSString *)mimeType;
@end

NS_ASSUME_NONNULL_END
