SFBAudioEngine is a set of C++ classes enabling applications to easily play audio in the following formats:

* WAVE
* AIFF
* Apple Lossless
* AAC
* FLAC
* MP3
* Musepack
* WavePack
* Ogg Vorbis
* Monkey's Audio
* Ogg Speex
* True Audio
* All other formats supported natively by Core Audio

In addition to playback, SFBAudioEngine supports reading and writing of metadata for most supported formats.

SFBAudioEngine uses C++11 language features (such as delegated constructors and range-based for), but not C++11 STL features (such as std::shared_ptr).  This means that clang must be used to compile SFBAudioEngine and any application using it, but either GNU's libstdc++ or clang's libc++ may be used provided that SFBAudioEngine's dependencies are compiled using the same library.

A command line audio player using SFBAudioEngine is as simple as:

	#include <CoreFoundation/CoreFoundation.h>
	
	#include "SFBAudioEngine/AudioDecoder.h"
	#include "SFBAudioEngine/AudioPlayer.h"
	
	#include <unistd.h>
	
	int main(int argc, char *argv [])
	{
		if(1 == argc) {
			printf("Usage: %s file [file ...]\n", argv[0]);
			return EXIT_FAILURE;
		}
		
		AudioPlayer player;
		
		while(0 < --argc) {
			const char *path = *++argv;
			CFURLRef fileURL = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault, 
																	   reinterpret_cast<const UInt8 *>(path), 
																	   strlen(path), 
																	   FALSE);
	
			AudioDecoder *decoder = AudioDecoder::CreateDecoderForURL(fileURL);
	
			CFRelease(fileURL), fileURL = NULL;
			
			if(NULL == decoder) {
				puts("Couldn't create decoder");
				continue;
			}
	
			if(!player.Enqueue(decoder)) {
				puts("Couldn't enqueue decoder");
				delete decoder, decoder = NULL;
			}
		}
	
		player.Play();
	
		// Display progress every 2 seconds
		while(player.IsPlaying()) {
			CFRunLoopRunInMode(kCFRunLoopDefaultMode, 2, true);
			printf("%.2f / %.2f [%.2f]\n", player.GetCurrentTime(), player.GetTotalTime(), player.GetRemainingTime());
		}

		player.Stop();
	
		return EXIT_SUCCESS;
	}

