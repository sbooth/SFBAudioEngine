//
// Copyright (c) 2010-2024 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

#import <Foundation/Foundation.h>

#import <taglib/tstring.h>

NS_ASSUME_NONNULL_BEGIN

namespace TagLib {

	/// Creates a `TagLib::String` from the specified Foundation string
	inline String StringFromNSString(NSString * _Nullable s) { if(s) return {s.UTF8String, String::UTF8}; else return {}; }

}

NS_ASSUME_NONNULL_END
