#include <CoreFoundation/CoreFoundation.h>

#include "SFBAudioEngine/AudioDecoder.h"
#include "SFBAudioEngine/AudioPlayer.h"

#include <unistd.h>

void
myCB1(void *context, const AudioDecoder *d)
{
	printf("DECODING STARTED (%s) !!\n", context);	
}

void
myCB2(void *context, const AudioDecoder *d)
{
	printf("DECODING FINISHED (%s) !!\n", context);	
}

void
myCB3(void *context, const AudioDecoder *d)
{
	printf("RENDERING STARTED (%s) !!\n", context);	
}

void
myCB4(void *context, const AudioDecoder *d)
{
	printf("RENDERING FINISHED (%s) !!\n", context);	
}

int main(int argc, char *argv [])
{
	if(1 == argc) {
		printf("Usage: %s file [file ...]", argv[0]);
		return EXIT_FAILURE;
	}
	
	AudioPlayer player;
	
	int arg = argc;
	while(0 < --arg) {
		
		const char *path = argv[argc - arg];
//		const char *path = argv[argc];

		printf("attempting to load %s\n", path);
		
		CFURLRef fileURL = CFURLCreateFromFileSystemRepresentation(kCFAllocatorDefault, (const UInt8 *)path, strlen(path), FALSE);

		AudioDecoder *decoder = AudioDecoder::CreateDecoderForURL(fileURL);
		if(NULL == decoder)
			continue;
		
		decoder->SetDecodingStartedCallback(myCB1, const_cast<char *>(path));
		decoder->SetDecodingFinishedCallback(myCB2, const_cast<char *>(path));
		decoder->SetRenderingStartedCallback(myCB3, const_cast<char *>(path));
		decoder->SetRenderingFinishedCallback(myCB4, const_cast<char *>(path));
		
		CFShow(decoder->GetFormatDescription());
		
		if(false == player.Enqueue(decoder))
			puts("couldn't enqueue");
	}

	player.Play();

	while(player.IsPlaying()) {
		sleep(1);
	}
	
	player.Stop();
	
	return EXIT_SUCCESS;
}
