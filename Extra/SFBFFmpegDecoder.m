//
// Copyright (c) 2013-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "SFBFFmpegDecoder.h"

#import "AVAudioPCMBuffer+SFBBufferUtilities.h"

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"
#pragma clang diagnostic ignored "-Wdocumentation"

#import <libavcodec/avcodec.h>
#import <libavformat/avformat.h>
#import <libavutil/channel_layout.h>
#import <libavutil/mathematics.h>

#pragma clang diagnostic pop

#import <os/log.h>

#define BUF_SIZE 4096
#define ERRBUF_SIZE 512

// MARK: Initialization

static void SetupFFmpeg(void) __attribute__((constructor));
static void SetupFFmpeg() { av_log_set_level(AV_LOG_QUIET); }

// MARK: Callbacks

static int my_read_packet(void *opaque, unsigned char *buf, int buf_size) {
    assert(opaque != NULL);

    SFBFFmpegDecoder *decoder = (__bridge SFBFFmpegDecoder *)opaque;

    NSInteger bytesRead;
    if (![decoder->_inputSource readBytes:buf length:buf_size bytesRead:&bytesRead error:nil]) {
        return -1;
    }
    return (int)bytesRead;
}

static int64_t my_seek(void *opaque, int64_t offset, int whence) {
    assert(opaque != NULL);

    SFBFFmpegDecoder *decoder = (__bridge SFBFFmpegDecoder *)opaque;
    if (!decoder->_inputSource.supportsSeeking) {
        return -1;
    }

    switch (whence) {
    case SEEK_SET:
        // offset remains unchanged
        break;
    case SEEK_CUR: {
        NSInteger inputSourceOffset;
        if ([decoder->_inputSource getOffset:&inputSourceOffset error:nil]) {
            offset += inputSourceOffset;
        }
        break;
    }
    case SEEK_END: {
        NSInteger inputSourceLength;
        if ([decoder->_inputSource getLength:&inputSourceLength error:nil]) {
            offset += inputSourceLength;
        }
        break;
    }
    case AVSEEK_SIZE: {
        NSInteger length;
        if (![decoder->_inputSource getLength:&length error:nil]) {
            return -1;
        }
        return length;
        /* break; */
    }
    }

    if (![decoder->_inputSource seekToOffset:offset error:nil]) {
        return -1;
    }

    NSInteger inputSourceOffset;
    if (![decoder->_inputSource getOffset:&inputSourceOffset error:nil]) {
        return -1;
    }

    return inputSourceOffset;
}

@interface SFBFFmpegDecoder () {
  @private
    AVFrame *_frame;
    AVIOContext *_ioContext;
    AVFormatContext *_formatContext;
    AVCodecContext *_codecContext;
    int _streamIndex;
    AVAudioFramePosition _framePosition;
    AVAudioPCMBuffer *_buffer;
}
- (int)readFrame;
- (int)decodeFrame;
@end

@implementation SFBFFmpegDecoder

+ (void)load {
    [SFBAudioDecoder registerSubclass:[self class] priority:-100];
}

+ (NSSet *)supportedPathExtensions {
    static NSSet *pathExtensions = nil;

    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        NSMutableSet *inputFormatExtensions = [NSMutableSet set];
        // Loop through each input format
        void *opaque = NULL;
        const AVInputFormat *inputFormat = NULL;
        while ((inputFormat = av_demuxer_iterate(&opaque))) {
            if (inputFormat->extensions) {
                NSString *extensions = [NSString stringWithUTF8String:inputFormat->extensions];
                [inputFormatExtensions addObjectsFromArray:[extensions componentsSeparatedByString:@","]];
            }
        }
        pathExtensions = [inputFormatExtensions copy];
    });

    return pathExtensions;
}

+ (NSSet *)supportedMIMETypes {
    static NSSet *mimeTypes = nil;

    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        NSMutableSet *inputFormatMIMETypes = [NSMutableSet set];
        // Loop through each input format
        void *opaque = NULL;
        const AVInputFormat *inputFormat = NULL;
        while ((inputFormat = av_demuxer_iterate(&opaque))) {
            if (inputFormat->mime_type) {
                NSString *types = [NSString stringWithUTF8String:inputFormat->mime_type];
                [inputFormatMIMETypes addObjectsFromArray:[types componentsSeparatedByString:@","]];
            }
        }
        mimeTypes = [inputFormatMIMETypes copy];
    });

    return mimeTypes;
}

- (BOOL)openReturningError:(NSError **)error {
    if (![super openReturningError:error]) {
        return NO;
    }

    unsigned char *buf = (unsigned char *)av_malloc(BUF_SIZE);
    if (!buf) {
        os_log_error(gSFBAudioDecoderLog, "av_malloc failed");
        if (error) {
            *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
        }
        return NO;
    }

    _ioContext = avio_alloc_context(buf, BUF_SIZE, 0, (__bridge void *)self, my_read_packet, NULL, my_seek);
    if (!_ioContext) {
        os_log_error(gSFBAudioDecoderLog, "avio_alloc_context failed");
        av_free(buf);
        if (error) {
            *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
        }
        return NO;
    }

    _formatContext = avformat_alloc_context();
    if (!_formatContext) {
        os_log_error(gSFBAudioDecoderLog, "avformat_alloc_context failed");
        avio_context_free(&_ioContext);
        if (error) {
            *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
        }
        return NO;
    }

    int result = avformat_open_input(&_formatContext, self.inputSource.url.fileSystemRepresentation, NULL, NULL);
    if (result != 0) {
        char errbuf[ERRBUF_SIZE];
        if (av_strerror(result, errbuf, ERRBUF_SIZE) == 0) {
            os_log_error(gSFBAudioDecoderLog, "avformat_open_input failed: %{public}s", errbuf);
        } else {
            os_log_error(gSFBAudioDecoderLog, "avformat_open_input failed");
        }

        avformat_free_context(_formatContext);
        avio_context_free(&_ioContext);

        if (error) {
            *error = [NSError
                    errorWithDomain:SFBAudioDecoderErrorDomain
                               code:SFBAudioDecoderErrorCodeInvalidFormat
                           userInfo:@{
                               NSURLErrorKey : _inputSource.url,
                               NSLocalizedFailureReasonErrorKey : NSLocalizedString(@"Format not recognized", @"")
                           }];
        }
        return NO;
    }

    // Retrieve stream information
    if (avformat_find_stream_info(_formatContext, NULL) < 0) {
        os_log_error(gSFBAudioDecoderLog, "Could not find stream information");

        avformat_free_context(_formatContext);
        avio_context_free(&_ioContext);

        if (error) {
            *error = [NSError
                    errorWithDomain:SFBAudioDecoderErrorDomain
                               code:SFBAudioDecoderErrorCodeInvalidFormat
                           userInfo:@{
                               NSURLErrorKey : _inputSource.url,
                               NSLocalizedFailureReasonErrorKey : NSLocalizedString(@"Format not recognized", @"")
                           }];
        }
        return NO;
    }

    // Use the best audio stream present in the file
    const AVCodec *codec = NULL;
    result = av_find_best_stream(_formatContext, AVMEDIA_TYPE_AUDIO, -1, -1, &codec, 0);
    if (result == AVERROR_STREAM_NOT_FOUND || !codec) {
        char errbuf[ERRBUF_SIZE];
        if (av_strerror(result, errbuf, ERRBUF_SIZE) == 0) {
            os_log_error(gSFBAudioDecoderLog, "av_find_best_stream failed: %{public}s", errbuf);
        } else {
            os_log_error(gSFBAudioDecoderLog, "av_find_best_stream failed: %d", result);
        }

        avformat_free_context(_formatContext);
        avio_context_free(&_ioContext);

        if (error) {
            *error = [NSError
                    errorWithDomain:SFBAudioDecoderErrorDomain
                               code:SFBAudioDecoderErrorCodeInvalidFormat
                           userInfo:@{
                               NSURLErrorKey : _inputSource.url,
                               NSLocalizedFailureReasonErrorKey : NSLocalizedString(@"Format not recognized", @"")
                           }];
        }
        return NO;
    }

    _streamIndex = result;

    _codecContext = avcodec_alloc_context3(codec);
    if (!_codecContext) {
        os_log_error(gSFBAudioDecoderLog, "avcodec_alloc_context3 failed");

        avformat_free_context(_formatContext);
        avio_context_free(&_ioContext);

        if (error) {
            *error = [NSError
                    errorWithDomain:SFBAudioDecoderErrorDomain
                               code:SFBAudioDecoderErrorCodeInvalidFormat
                           userInfo:@{
                               NSURLErrorKey : _inputSource.url,
                               NSLocalizedFailureReasonErrorKey : NSLocalizedString(@"Format not recognized", @"")
                           }];
        }
        return NO;
    }

    result = avcodec_parameters_to_context(_codecContext, _formatContext->streams[_streamIndex]->codecpar);
    if (result) {
        os_log_error(gSFBAudioDecoderLog, "avcodec_parameters_to_context failed");
    }

    result = avcodec_open2(_codecContext, codec, NULL);
    if (result) {
        char errbuf[ERRBUF_SIZE];
        if (0 == av_strerror(result, errbuf, ERRBUF_SIZE)) {
            os_log_error(gSFBAudioDecoderLog, "avcodec_open2 failed: %{public}s", errbuf);
        } else {
            os_log_error(gSFBAudioDecoderLog, "avcodec_open2 failed: %d", result);
        }

        avcodec_free_context(&_codecContext);
        avformat_free_context(_formatContext);
        avio_context_free(&_ioContext);

        if (error) {
            *error = [NSError
                    errorWithDomain:SFBAudioDecoderErrorDomain
                               code:SFBAudioDecoderErrorCodeInvalidFormat
                           userInfo:@{
                               NSURLErrorKey : _inputSource.url,
                               NSLocalizedFailureReasonErrorKey : NSLocalizedString(@"Format not recognized", @"")
                           }];
        }
        return NO;
    }

    AVAudioChannelCount channelCount =
            (AVAudioChannelCount)_formatContext->streams[_streamIndex]->codecpar->ch_layout.nb_channels;

    AVAudioChannelLayout *channelLayout = nil;
    if (_formatContext->streams[_streamIndex]->codecpar->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC)
        // nothing to do; leave channelLayout nil
        ;
    else if (_formatContext->streams[_streamIndex]->codecpar->ch_layout.order == AV_CHANNEL_ORDER_NATIVE) {
        AudioChannelLayout layout = {0};
        layout.mChannelLayoutTag = kAudioChannelLayoutTag_UseChannelBitmap;
        layout.mChannelBitmap = (AudioChannelBitmap)_formatContext->streams[_streamIndex]->codecpar->ch_layout.u.mask;
        channelLayout = [[AVAudioChannelLayout alloc] initWithLayout:&layout];

        // Sanity check
        if (channelLayout.channelCount != channelCount) {
            os_log_error(gSFBAudioDecoderLog,
                         "Channel count mismatch between channelLayout.channelCount (%u) and "
                         "codec_par->ch_layout.nb_channels (%u)",
                         channelLayout.channelCount, channelCount);
            channelLayout = nil;
        }
    } else if (_formatContext->streams[_streamIndex]->codecpar->ch_layout.order == AV_CHANNEL_ORDER_CUSTOM) {
        os_log_error(gSFBAudioDecoderLog, "ffmpeg custom channel layouts not (yet) supported");
    } else {
        os_log_error(gSFBAudioDecoderLog, "Unsupported channel layout order %u",
                     _formatContext->streams[_streamIndex]->codecpar->ch_layout.order);
    }

    // Generate PCM output
    switch (_formatContext->streams[_streamIndex]->codecpar->format) {

    case AV_SAMPLE_FMT_U8P: {
        AudioStreamBasicDescription format = {0};
        format.mFormatID = kAudioFormatLinearPCM;

        format.mSampleRate = _formatContext->streams[_streamIndex]->codecpar->sample_rate;
        format.mChannelsPerFrame = (UInt32)channelCount;

        format.mFormatFlags = kAudioFormatFlagIsPacked | kAudioFormatFlagIsNonInterleaved;
        format.mBitsPerChannel = 8;
        format.mBytesPerPacket = 1;
        format.mFramesPerPacket = 1;
        format.mBytesPerFrame = 1;

        _processingFormat = [[AVAudioFormat alloc] initWithStreamDescription:&format channelLayout:channelLayout];
        break;
    }

    case AV_SAMPLE_FMT_U8: {
        AudioStreamBasicDescription format = {0};
        format.mFormatID = kAudioFormatLinearPCM;

        format.mSampleRate = _formatContext->streams[_streamIndex]->codecpar->sample_rate;
        format.mChannelsPerFrame = (UInt32)channelCount;

        format.mFormatFlags = kAudioFormatFlagIsPacked;
        format.mBitsPerChannel = 8;
        format.mBytesPerPacket = format.mChannelsPerFrame;
        format.mFramesPerPacket = 1;
        format.mBytesPerFrame = format.mChannelsPerFrame;

        _processingFormat = [[AVAudioFormat alloc] initWithStreamDescription:&format channelLayout:channelLayout];
        break;
    }

    case AV_SAMPLE_FMT_S16P:
        if (channelLayout) {
            _processingFormat = [[AVAudioFormat alloc]
                    initWithCommonFormat:AVAudioPCMFormatInt16
                              sampleRate:_formatContext->streams[_streamIndex]->codecpar->sample_rate
                             interleaved:NO
                           channelLayout:channelLayout];
        } else {
            _processingFormat = [[AVAudioFormat alloc]
                    initWithCommonFormat:AVAudioPCMFormatInt16
                              sampleRate:_formatContext->streams[_streamIndex]->codecpar->sample_rate
                                channels:channelCount
                             interleaved:NO];
        }
        break;

    case AV_SAMPLE_FMT_S16:
        if (channelLayout) {
            _processingFormat = [[AVAudioFormat alloc]
                    initWithCommonFormat:AVAudioPCMFormatInt16
                              sampleRate:_formatContext->streams[_streamIndex]->codecpar->sample_rate
                             interleaved:YES
                           channelLayout:channelLayout];
        } else {
            _processingFormat = [[AVAudioFormat alloc]
                    initWithCommonFormat:AVAudioPCMFormatInt16
                              sampleRate:_formatContext->streams[_streamIndex]->codecpar->sample_rate
                                channels:channelCount
                             interleaved:YES];
        }
        break;

    case AV_SAMPLE_FMT_S32P:
        if (channelLayout) {
            _processingFormat = [[AVAudioFormat alloc]
                    initWithCommonFormat:AVAudioPCMFormatInt32
                              sampleRate:_formatContext->streams[_streamIndex]->codecpar->sample_rate
                             interleaved:NO
                           channelLayout:channelLayout];
        } else {
            _processingFormat = [[AVAudioFormat alloc]
                    initWithCommonFormat:AVAudioPCMFormatInt32
                              sampleRate:_formatContext->streams[_streamIndex]->codecpar->sample_rate
                                channels:channelCount
                             interleaved:NO];
        }
        break;

    case AV_SAMPLE_FMT_S32:
        if (channelLayout) {
            _processingFormat = [[AVAudioFormat alloc]
                    initWithCommonFormat:AVAudioPCMFormatInt32
                              sampleRate:_formatContext->streams[_streamIndex]->codecpar->sample_rate
                             interleaved:YES
                           channelLayout:channelLayout];
        } else {
            _processingFormat = [[AVAudioFormat alloc]
                    initWithCommonFormat:AVAudioPCMFormatInt32
                              sampleRate:_formatContext->streams[_streamIndex]->codecpar->sample_rate
                                channels:channelCount
                             interleaved:YES];
        }
        break;

    case AV_SAMPLE_FMT_FLTP:
        if (channelLayout) {
            _processingFormat = [[AVAudioFormat alloc]
                    initWithCommonFormat:AVAudioPCMFormatFloat32
                              sampleRate:_formatContext->streams[_streamIndex]->codecpar->sample_rate
                             interleaved:NO
                           channelLayout:channelLayout];
        } else {
            _processingFormat = [[AVAudioFormat alloc]
                    initWithCommonFormat:AVAudioPCMFormatFloat32
                              sampleRate:_formatContext->streams[_streamIndex]->codecpar->sample_rate
                                channels:channelCount
                             interleaved:NO];
        }
        break;

    case AV_SAMPLE_FMT_FLT:
        if (channelLayout) {
            _processingFormat = [[AVAudioFormat alloc]
                    initWithCommonFormat:AVAudioPCMFormatFloat32
                              sampleRate:_formatContext->streams[_streamIndex]->codecpar->sample_rate
                             interleaved:YES
                           channelLayout:channelLayout];
        } else {
            _processingFormat = [[AVAudioFormat alloc]
                    initWithCommonFormat:AVAudioPCMFormatFloat32
                              sampleRate:_formatContext->streams[_streamIndex]->codecpar->sample_rate
                                channels:channelCount
                             interleaved:YES];
        }
        break;

    case AV_SAMPLE_FMT_DBLP:
        if (channelLayout) {
            _processingFormat = [[AVAudioFormat alloc]
                    initWithCommonFormat:AVAudioPCMFormatFloat64
                              sampleRate:_formatContext->streams[_streamIndex]->codecpar->sample_rate
                             interleaved:NO
                           channelLayout:channelLayout];
        } else {
            _processingFormat = [[AVAudioFormat alloc]
                    initWithCommonFormat:AVAudioPCMFormatFloat64
                              sampleRate:_formatContext->streams[_streamIndex]->codecpar->sample_rate
                                channels:channelCount
                             interleaved:NO];
        }
        break;

    case AV_SAMPLE_FMT_DBL:
        if (channelLayout) {
            _processingFormat = [[AVAudioFormat alloc]
                    initWithCommonFormat:AVAudioPCMFormatFloat64
                              sampleRate:_formatContext->streams[_streamIndex]->codecpar->sample_rate
                             interleaved:YES
                           channelLayout:channelLayout];
        } else {
            _processingFormat = [[AVAudioFormat alloc]
                    initWithCommonFormat:AVAudioPCMFormatFloat64
                              sampleRate:_formatContext->streams[_streamIndex]->codecpar->sample_rate
                                channels:channelCount
                             interleaved:YES];
        }
        break;

    default:
        os_log_error(gSFBAudioDecoderLog, "Unknown sample format");
        break;
    }

    // Set up the source format
    AudioStreamBasicDescription format = {0};

    format.mFormatID = 'FFMP';
    format.mSampleRate = _processingFormat.streamDescription->mSampleRate;
    format.mChannelsPerFrame = _processingFormat.streamDescription->mChannelsPerFrame;
    format.mBitsPerChannel = _processingFormat.streamDescription->mBitsPerChannel;

    // TODO: Determine max frame size
    _buffer = [[AVAudioPCMBuffer alloc] initWithPCMFormat:_processingFormat frameCapacity:4096];
    _buffer.frameLength = 0;

    _frame = av_frame_alloc();
    if (!_frame) {
        os_log_error(gSFBAudioDecoderLog, "av_frame_alloc failed");

        avcodec_free_context(&_codecContext);
        avformat_free_context(_formatContext);
        avio_context_free(&_ioContext);

        if (error) {
            *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:ENOMEM userInfo:nil];
        }

        return NO;
    }

    return YES;
}

- (BOOL)closeReturningError:(NSError **)error {
    if (_ioContext) {
        avio_context_free(&_ioContext);
    }

    if (_formatContext) {
        avformat_free_context(_formatContext);
        _formatContext = NULL;
    }

    if (_codecContext) {
        avcodec_free_context(&_codecContext);
    }

    if (_frame) {
        av_frame_free(&_frame);
    }

    return [super closeReturningError:error];
}

- (BOOL)isOpen {
    return _ioContext != NULL;
}

- (AVAudioFramePosition)framePosition {
    return _framePosition;
}

- (AVAudioFramePosition)frameLength {
    if (_formatContext->streams[_streamIndex]->nb_frames) {
        return _formatContext->streams[_streamIndex]->nb_frames;
    } else if (_formatContext->streams[_streamIndex]->duration != AV_NOPTS_VALUE) {
        return av_rescale(_formatContext->streams[_streamIndex]->duration,
                          _formatContext->streams[_streamIndex]->time_base.num,
                          _formatContext->streams[_streamIndex]->time_base.den) *
               (int64_t)_processingFormat.sampleRate;
    } else {
        return -1;
    }
}

- (BOOL)decodeIntoBuffer:(AVAudioPCMBuffer *)buffer frameLength:(AVAudioFrameCount)frameLength error:(NSError **)error {
    NSParameterAssert(buffer != nil);

    // Reset output buffer data size
    buffer.frameLength = 0;

    if (![buffer.format isEqual:_processingFormat]) {
        os_log_debug(gSFBAudioDecoderLog, "-decodeAudio:frameLength:error: called with invalid parameters");
        return NO;
    }

    if (frameLength > buffer.frameCapacity) {
        frameLength = buffer.frameCapacity;
    }

    AVAudioFrameCount framesProcessed = 0;

    for (;;) {
        AVAudioFrameCount framesRemaining = frameLength - framesProcessed;
        AVAudioFrameCount framesCopied = [buffer appendFromBuffer:_buffer
                                                readingFromOffset:0
                                                      frameLength:framesRemaining];
        [_buffer trimAtOffset:0 frameLength:framesCopied];

        framesProcessed += framesCopied;

        // All requested frames were read
        if (framesProcessed == frameLength) {
            break;
        }

        // Decode some audio
        int result = [self decodeFrame];

        // EOF reached
        if (result == AVERROR_EOF) {
            break;
        }
        // Need to provide input data to the codec
        else if (result == AVERROR(EAGAIN)) {
            result = [self readFrame];

            if (result == AVERROR_EOF) {
                if (_codecContext->codec->capabilities & AV_CODEC_CAP_DELAY) {
                    // TODO: Flush buffer
                }
                break;
            } else if (result == AVERROR(EAGAIN)) {
            } else if (result < 0) {
                os_log_error(gSFBAudioDecoderLog, "ReadFrame() failed: %d", result);
                break;
            }
        }
    }

    _framePosition += framesProcessed;

    return YES;
}

- (BOOL)seekToFrame:(AVAudioFramePosition)frame error:(NSError **)error {
    NSParameterAssert(frame >= 0);

    int64_t timestamp = av_rescale(frame / (SInt64)_processingFormat.sampleRate,
                                   _formatContext->streams[_streamIndex]->time_base.den,
                                   _formatContext->streams[_streamIndex]->time_base.num);
    int result = av_seek_frame(_formatContext, _streamIndex, timestamp, 0);
    if (result < 0) {
        char errbuf[ERRBUF_SIZE];
        if (0 == av_strerror(result, errbuf, ERRBUF_SIZE)) {
            os_log_error(gSFBAudioDecoderLog, "av_seek_frame failed: %{public}s", errbuf);
        } else {
            os_log_error(gSFBAudioDecoderLog, "av_seek_frame failed: %d", result);
        }

        return NO;
    }

    avcodec_flush_buffers(_codecContext);

    _framePosition = frame;

    return YES;
}

- (int)readFrame {
    AVPacket *packet = av_packet_alloc();
    if (!packet) {
        return AVERROR(ENOMEM);
    }

    int result = av_read_frame(_formatContext, packet);

    if (result == AVERROR_EOF) {
        // EOF reached?
    } else if (result < 0) {
        // Other error encountered
        char errbuf[ERRBUF_SIZE];
        if (av_strerror(result, errbuf, ERRBUF_SIZE) == 0) {
            os_log_error(gSFBAudioDecoderLog, "av_read_frame failed: %{public}s", errbuf);
        } else {
            os_log_error(gSFBAudioDecoderLog, "av_read_frame failed: %d", result);
        }
    } else {
        // Send the packet with the compressed data to the decoder
        result = avcodec_send_packet(_codecContext, packet);

        if (result == AVERROR_EOF) {
            // Decoder has been flushed
        } else if (result == AVERROR(EAGAIN)) {
            // Input not accepted in current state
        } else if (result) {
            // Other error encountered
            char errbuf[ERRBUF_SIZE];
            if (av_strerror(result, errbuf, ERRBUF_SIZE) == 0) {
                os_log_error(gSFBAudioDecoderLog, "avcodec_send_packet failed: %{public}s", errbuf);
            } else {
                os_log_error(gSFBAudioDecoderLog, "avcodec_send_packet failed: %d", result);
            }
        }
    }

    av_packet_free(&packet);

    return result;
}

- (int)decodeFrame {
    // Attempt to read decoded audio
    int result = avcodec_receive_frame(_codecContext, _frame);

    if (result == AVERROR_EOF) {
        // EOF reached?
    } else if (result == AVERROR(EAGAIN)) {
        // Need to provide input data to the codec
    } else if (result > 0) {
        // Other error encountered
        char errbuf[ERRBUF_SIZE];
        if (av_strerror(result, errbuf, ERRBUF_SIZE) == 0) {
            os_log_error(gSFBAudioDecoderLog, "avcodec_receive_frame failed: %{public}s", errbuf);
        } else {
            os_log_error(gSFBAudioDecoderLog, "avcodec_receive_frame failed: %d", result);
        }

        return result;
    } else {
        // Copy received audio to _buffer
        UInt32 bytesPerFrame = _processingFormat.streamDescription->mBytesPerFrame;
        size_t spaceRemaining = (_buffer.frameCapacity - _buffer.frameLength) * bytesPerFrame;
        if (spaceRemaining < (UInt32)_frame->linesize[0]) {
            os_log_error(gSFBAudioDecoderLog, "Insufficient space in buffer for decoded frame: %lu available, need %d",
                         spaceRemaining, _frame->linesize[0]);
            return AVERROR(ENOMEM);
        }

        // Planar formats are not interleaved
        const AudioBufferList *bufferList = _buffer.audioBufferList;
        if (av_sample_fmt_is_planar(_codecContext->sample_fmt)) {
            for (UInt32 i = 0; i < bufferList->mNumberBuffers; ++i) {
                memcpy((unsigned char *)bufferList->mBuffers[i].mData + bufferList->mBuffers[i].mDataByteSize,
                       _frame->extended_data[i], (size_t)_frame->linesize[0]);
            }
        } else {
            memcpy((unsigned char *)bufferList->mBuffers[0].mData + bufferList->mBuffers[0].mDataByteSize,
                   _frame->extended_data[0], (size_t)_frame->linesize[0]);
        }

        _buffer.frameLength += (AVAudioFrameCount)_frame->linesize[0] / bytesPerFrame;
    }

    return result;
}

@end
