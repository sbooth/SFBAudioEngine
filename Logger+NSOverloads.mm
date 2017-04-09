/*
 * Copyright (c) 2011 - 2017 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

#include "Logger.h"

std::ostream& operator<<(std::ostream& out, NSString *s)
{
	out << (__bridge CFStringRef)s;
	return out;
}

std::ostream& operator<<(std::ostream& out, NSNumber *n)
{
	out << (__bridge CFNumberRef)n;
	return out;
}

std::ostream& operator<<(std::ostream& out, NSURL *u)
{
	out << (__bridge CFURLRef)u;
	return out;
}

std::ostream& operator<<(std::ostream& out, NSError *e)
{
	out << (__bridge CFErrorRef)e;
	return out;
}

