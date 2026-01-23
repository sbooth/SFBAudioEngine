//
// Copyright (c) 2020-2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "SFBFileOutputSource.h"

#import "SFBOutputSource+Internal.h"

#import <stdio.h>

@interface SFBFileOutputSource () {
  @private
    FILE *_file;
}
@end

@implementation SFBFileOutputSource

#if 0
- (instancetype)initWithURL:(NSURL *)url
{
    NSParameterAssert(url != nil);
    NSParameterAssert(url.isFileURL);
    return [super initWithURL:url];
}
#endif

- (BOOL)openReturningError:(NSError **)error {
    _file = fopen(_url.fileSystemRepresentation, "w+");
    if (!_file) {
        int err = errno;
        os_log_error(gSFBOutputSourceLog, "fopen failed: %{public}s (%d)", strerror(err), err);
        if (error) {
            *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:err userInfo:@{NSURLErrorKey : _url}];
        }
        return NO;
    }

    return YES;
}

- (BOOL)closeReturningError:(NSError **)error {
    if (_file) {
        int result = fclose(_file);
        _file = NULL;
        if (result) {
            int err = errno;
            os_log_error(gSFBOutputSourceLog, "fclose failed: %{public}s (%d)", strerror(err), err);
            if (error) {
                *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:err userInfo:@{NSURLErrorKey : _url}];
            }
            return NO;
        }
    }
    return YES;
}

- (BOOL)isOpen {
    return _file != NULL;
}

- (BOOL)readBytes:(void *)buffer length:(NSInteger)length bytesRead:(NSInteger *)bytesRead error:(NSError **)error {
    NSParameterAssert(buffer != NULL);
    NSParameterAssert(length >= 0);
    NSParameterAssert(bytesRead != NULL);

    size_t read = fread(buffer, 1, (size_t)length, _file);
    if (read != (size_t)length && ferror(_file)) {
        int err = errno;
        os_log_error(gSFBOutputSourceLog, "fread error: %{public}s (%d)", strerror(err), err);
        if (error) {
            *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:err userInfo:@{NSURLErrorKey : _url}];
        }
        return NO;
    }
    *bytesRead = (NSInteger)read;
    return YES;
}

- (BOOL)writeBytes:(const void *)buffer
            length:(NSInteger)length
      bytesWritten:(NSInteger *)bytesWritten
             error:(NSError **)error {
    NSParameterAssert(buffer != NULL);
    NSParameterAssert(length >= 0);
    NSParameterAssert(bytesWritten != NULL);

    size_t written = fwrite(buffer, 1, (size_t)length, _file);
    if (written != (size_t)length) {
        int err = errno;
        os_log_error(gSFBOutputSourceLog, "fwrite error: %{public}s (%d)", strerror(err), err);
        if (error) {
            *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:err userInfo:@{NSURLErrorKey : _url}];
        }
        return NO;
    }
    *bytesWritten = (NSInteger)written;
    return YES;
}

- (BOOL)atEOF {
    return feof(_file) != 0;
}

- (BOOL)getOffset:(NSInteger *)offset error:(NSError **)error {
    NSParameterAssert(offset != NULL);
    off_t result = ftello(_file);
    if (result == -1) {
        int err = errno;
        os_log_error(gSFBOutputSourceLog, "ftello failed: %{public}s (%d)", strerror(err), err);
        if (error) {
            *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:err userInfo:@{NSURLErrorKey : _url}];
        }
        return NO;
    }
    *offset = result;
    return YES;
}

- (BOOL)getLength:(NSInteger *)length error:(NSError **)error {
    NSParameterAssert(length != NULL);
    off_t offset = ftello(_file);
    if (offset == -1) {
        int err = errno;
        os_log_error(gSFBOutputSourceLog, "ftello failed: %{public}s (%d)", strerror(err), err);
        if (error) {
            *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:err userInfo:@{NSURLErrorKey : _url}];
        }
        return NO;
    }

    if (fseeko(_file, 0, SEEK_END)) {
        int err = errno;
        os_log_error(gSFBOutputSourceLog, "fseeko(0, SEEK_END) error: %{public}s (%d)", strerror(err), err);
        if (error) {
            *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:err userInfo:@{NSURLErrorKey : _url}];
        }
        return NO;
    }

    off_t len = ftello(_file);
    if (len == -1) {
        int err = errno;
        os_log_error(gSFBOutputSourceLog, "ftello failed: %{public}s (%d)", strerror(err), err);
        if (error) {
            *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:err userInfo:@{NSURLErrorKey : _url}];
        }
        return NO;
    }

    if (fseeko(_file, offset, SEEK_SET)) {
        int err = errno;
        os_log_error(gSFBOutputSourceLog, "fseeko(%ld, SEEK_SET) error: %{public}s (%d)", (long)offset, strerror(err),
                     err);
        if (error) {
            *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:err userInfo:@{NSURLErrorKey : _url}];
        }
        return NO;
    }

    *length = len;

    return YES;
}

- (BOOL)supportsSeeking {
    return YES;
}

- (BOOL)seekToOffset:(NSInteger)offset error:(NSError **)error {
    if (fseeko(_file, offset, SEEK_SET)) {
        int err = errno;
        os_log_error(gSFBOutputSourceLog, "fseeko(%ld, SEEK_SET) error: %{public}s (%d)", (long)offset, strerror(err),
                     err);
        if (error) {
            *error = [NSError errorWithDomain:NSPOSIXErrorDomain code:err userInfo:@{NSURLErrorKey : _url}];
        }
        return NO;
    }
    return YES;
}

@end
