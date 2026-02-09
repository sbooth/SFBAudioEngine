//
// SPDX-FileCopyrightText: 2020 Stephen F. Booth <contact@sbooth.dev>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

#import "SFBFileOutputTarget.h"

#import "SFBOutputTarget+Internal.h"

#import <stdio.h>

@interface SFBFileOutputTarget () {
  @private
    FILE *_file;
}
@end

@implementation SFBFileOutputTarget

- (BOOL)openReturningError:(NSError **)error {
    _file = fopen(_url.fileSystemRepresentation, "w+");
    if (!_file) {
        int err = errno;
        os_log_error(gSFBOutputTargetLog, "fopen failed: %{public}s (%d)", strerror(err), err);
        if (error) {
            *error = [self posixErrorWithCode:err];
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
            os_log_error(gSFBOutputTargetLog, "fclose failed: %{public}s (%d)", strerror(err), err);
            if (error) {
                *error = [self posixErrorWithCode:err];
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
        os_log_error(gSFBOutputTargetLog, "fread error: %{public}s (%d)", strerror(err), err);
        if (error) {
            *error = [self posixErrorWithCode:err];
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
        os_log_error(gSFBOutputTargetLog, "fwrite error: %{public}s (%d)", strerror(err), err);
        if (error) {
            *error = [self posixErrorWithCode:err];
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
        os_log_error(gSFBOutputTargetLog, "ftello failed: %{public}s (%d)", strerror(err), err);
        if (error) {
            *error = [self posixErrorWithCode:err];
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
        os_log_error(gSFBOutputTargetLog, "ftello failed: %{public}s (%d)", strerror(err), err);
        if (error) {
            *error = [self posixErrorWithCode:err];
        }
        return NO;
    }

    if (fseeko(_file, 0, SEEK_END)) {
        int err = errno;
        os_log_error(gSFBOutputTargetLog, "fseeko(0, SEEK_END) error: %{public}s (%d)", strerror(err), err);
        if (error) {
            *error = [self posixErrorWithCode:err];
        }
        return NO;
    }

    off_t len = ftello(_file);
    if (len == -1) {
        int err = errno;
        os_log_error(gSFBOutputTargetLog, "ftello failed: %{public}s (%d)", strerror(err), err);
        if (error) {
            *error = [self posixErrorWithCode:err];
        }
        return NO;
    }

    if (fseeko(_file, offset, SEEK_SET)) {
        int err = errno;
        os_log_error(gSFBOutputTargetLog, "fseeko(%ld, SEEK_SET) error: %{public}s (%d)", (long)offset, strerror(err),
                     err);
        if (error) {
            *error = [self posixErrorWithCode:err];
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
        os_log_error(gSFBOutputTargetLog, "fseeko(%ld, SEEK_SET) error: %{public}s (%d)", (long)offset, strerror(err),
                     err);
        if (error) {
            *error = [self posixErrorWithCode:err];
        }
        return NO;
    }
    return YES;
}

@end
