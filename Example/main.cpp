/*
 *  Copyright (C) 2009, 2010 Stephen F. Booth <me@sbooth.org>
 *  All Rights Reserved
 */

#include <CoreFoundation/CoreFoundation.h>

#include "SFBAudioEngine/AudioDecoder.h"
#include "SFBAudioEngine/AudioPlayer.h"
#include "SFBAudioEngine/DSPAudioPlayer.h"

#include <unistd.h>


static void
decodingStarted(void					*context,
				const AudioDecoder		*decoder)
{
#pragma unused(context)
	printf("Decoding started: ");
	CFShow(const_cast<CFURLRef>(const_cast<AudioDecoder *>(decoder)->GetURL()));
}

static void
decodingFinished(void					*context,
				 const AudioDecoder		*decoder)
{
#pragma unused(context)
	printf("Decoding finished: ");
	CFShow(const_cast<CFURLRef>(const_cast<AudioDecoder *>(decoder)->GetURL()));
}

static void
renderingStarted(void					*context,
				 const AudioDecoder		*decoder)
{
#pragma unused(context)
	printf("Rendering started: ");
	CFShow(const_cast<CFURLRef>(const_cast<AudioDecoder *>(decoder)->GetURL()));
}

static void
renderingFinished(void					*context,
				  const AudioDecoder		*decoder)
{
#pragma unused(context)
	printf("Rendering finished: ");
	CFShow(const_cast<CFURLRef>(const_cast<AudioDecoder *>(decoder)->GetURL()));
}


int main(int argc, char *argv [])
{
	if(1 == argc) {
		printf("Usage: %s file [file ...]", argv[0]);
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

		decoder->SetDecodingStartedCallback(decodingStarted, NULL);
		decoder->SetDecodingFinishedCallback(decodingFinished, NULL);
		decoder->SetRenderingStartedCallback(renderingStarted, NULL);
		decoder->SetRenderingFinishedCallback(renderingFinished, NULL);
		
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
