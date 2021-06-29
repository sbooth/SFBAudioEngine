//
// Copyright (c) 2011 - 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

import SwiftUI
import Combine

struct PlayerView: View {
	@ObservedObject	var viewModel: PlayerViewModel
	@Environment(\.presentationMode) var presentationMode
	@State var currentPosition: Double = 0

	var body: some View {
		GeometryReader { geometry in
			VStack(spacing: 16) {
				image
					.resizable()
					.frame(width: geometry.size.width - 48, height: geometry.size.width - 48)
					.cornerRadius(20)
					.shadow(radius: 10)

				VStack(spacing: 8) {
					Text(viewModel.dataModel.nowPlaying?.metadata.title ?? "title")
						.font(Font.system(.title).bold())
					Text(viewModel.dataModel.nowPlaying?.metadata.artist ?? "artist")
						.font(.system(.headline))
				}

				VStack(spacing: 8) {
					HStack(spacing: 40) {
						Button(action: {
							viewModel.seekBackward()
						}) {
							ZStack {
								Circle()
									.frame(width: 80, height: 80)
									.accentColor(.pink)
									.shadow(radius: 10)
								Image(systemName: "backward.fill")
									.foregroundColor(.white)
									.font(.system(.title))
							}
						}
						.disabled(viewModel.dataModel.playbackState == .stopped)

						Button(action: {
							viewModel.togglePlayPause()
						}) {
							ZStack {
								Circle()
									.frame(width: 80, height: 80)
									.accentColor(.pink)
									.shadow(radius: 10)
								Image(systemName: viewModel.dataModel.playbackState == .playing ? "pause.fill" : "play.fill")
									.foregroundColor(.white)
									.font(.system(.title))
							}

						}
						.disabled(viewModel.dataModel.playbackState == .stopped)

						Button(action: {
							viewModel.seekForward()
						}) {
							ZStack {
								Circle()
									.frame(width: 80, height: 80)
									.accentColor(.pink)
									.shadow(radius: 10)
								Image(systemName: "forward.fill")
									.foregroundColor(.white)
									.font(.system(.title))
							}
						}
						.disabled(viewModel.dataModel.playbackState == .stopped)
					}
					Slider(value: Binding(
						get: { currentPosition },
						set: { viewModel.seek(position: $0) }
					))
						.padding(.horizontal, 20.0)
						.accentColor(.pink)
						.disabled(viewModel.dataModel.playbackState == .stopped)
				}
			}
		}
		.onReceive(viewModel.playbackProgress) { currentPosition = $0 }
		.onReceive(viewModel.dataModel.$playbackState) {
			if $0 == .stopped {
				self.presentationMode.wrappedValue.dismiss()
			}
		}
	}

	private var image: Image {
		if let image = viewModel.dataModel.nowPlaying?.metadata.randomImage {
			return Image(uiImage: image)
		} else {
			return Image(systemName: "s.square")
		}
	}
}

extension Track {
	var image: some View {
		if let image = metadata.randomImage {
			return Image(uiImage: image)
		} else {
			return Image(systemName: "s.square")
		}
	}
}

struct PlayerView_Previews: PreviewProvider {
    static var previews: some View {
		let model = DataModel()
		return PlayerView(viewModel: PlayerViewModel(dataModel: model))
    }
}
