XCFRAMEWORKS := \
	FLAC.xcframework \
	MAC.xcframework \
	dumb.xcframework \
	lame.xcframework \
	mpc.xcframework \
	mpg123.xcframework \
	ogg.xcframework \
	opus.xcframework \
	sndfile.xcframework \
	speex.xcframework\
	taglib.xcframework\
	tta-cpp.xcframework\
	vorbis.xcframework\
	wavpack.xcframework

all: $(XCFRAMEWORKS)
.phony: all

$(XCFRAMEWORKS): %.xcframework: %.xcframework.zip
	ditto -x -k "$<" "$(dir $@)"
