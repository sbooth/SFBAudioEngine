//
// Copyright (c) 2026 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import "SFBErrorWithLocalizedDescription.h"

NSError *SFBErrorWithLocalizedDescription(NSErrorDomain domain, NSInteger code, NSString *format,
                                          NSDictionary *userInfo, ...) {
    NSCParameterAssert(domain != nil);
    NSCParameterAssert(format != nil);

    va_list ap;
    va_start(ap, userInfo);
    NSString *description = [[NSString alloc] initWithFormat:format locale:[NSLocale currentLocale] arguments:ap];
    va_end(ap);

    NSMutableDictionary *dictionary = [NSMutableDictionary dictionary];
    [dictionary setObject:description forKey:NSLocalizedDescriptionKey];

    if (userInfo) {
        [dictionary addEntriesFromDictionary:userInfo];
    }

    return [NSError errorWithDomain:domain code:code userInfo:dictionary];
}
