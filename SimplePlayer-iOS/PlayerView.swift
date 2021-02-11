//
// Copyright (c) 2011 - 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

import SwiftUI
import Combine

struct PlayerView: View {
	@State private var currentPlaybackState: AudioPlayer.PlaybackState = .stopped {
		didSet {
			if currentPlaybackState == .stopped {
				self.presentationMode.wrappedValue.dismiss()
			}
		}
	}
	@State private var currentPosition: Double = 0.0

	@Environment(\.presentationMode) var presentationMode

	private var title: String = "[title]"
	private var artist: String = "[artist]"
	private var image: UIImage? = nil

	private let playerController: PlayerController
	private let displayLinkPublisher = DisplayLinkPublisher()

	init(_ playerController: PlayerController, track: Track) {
		self.playerController = playerController

		self.title = track.metadata.title ?? "[title]"
		self.artist = track.metadata.artist ?? "[artist]"

		if let attachedPicture = track.metadata.attachedPictures.randomElement() {
			self.image = UIImage(data: attachedPicture.imageData)
		}
	}

	private var formatter: DateComponentsFormatter = {
		let formatter = DateComponentsFormatter()
		formatter.zeroFormattingBehavior = .pad
		formatter.allowedUnits = [.minute, .second]
		formatter.unitsStyle = .positional
		return formatter
	}()

    var body: some View {
		GeometryReader { geometry in
			VStack(spacing: 16) {
				if image != nil {
					Image(uiImage: image!)
						.resizable()
						.frame(width: geometry.size.width - 48, height: geometry.size.width - 48)
						.cornerRadius(20)
						.shadow(radius: 10)
				}
				else {
					Image(systemName: "s.square")
						.resizable()
						.frame(width: geometry.size.width - 48, height: geometry.size.width - 48)
						.cornerRadius(20)
						.shadow(radius: 10)
				}

				VStack(spacing: 8) {
					Text(title)
						.font(Font.system(.title).bold())
					Text(artist)
						.font(.system(.headline))
				}

				VStack(spacing: 8) {
					HStack(spacing: 40) {
						Button(action: {
							playerController.player.seekBackward()
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
						.disabled(currentPlaybackState == .stopped)

						Button(action: {
							try? playerController.player.togglePlayPause()
						}) {
							ZStack {
								Circle()
									.frame(width: 80, height: 80)
									.accentColor(.pink)
									.shadow(radius: 10)
								Image(systemName: currentPlaybackState == .playing ? "pause.fill" : "play.fill")
									.foregroundColor(.white)
									.font(.system(.title))
							}

						}
						.disabled(currentPlaybackState == .stopped)

						Button(action: {
							playerController.player.seekForward()
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
						.disabled(currentPlaybackState == .stopped)
					}

					Slider(value: Binding(
						get: { return currentPosition },
						set: {
							if let current = playerController.player.position?.progress {
								let tolerance = 0.01
								if abs(current - $0) >= tolerance {
									playerController.player.seek(position: $0)
								}
							}
						}
					))
						.padding(.horizontal, 20.0)
						.accentColor(.pink)
						.disabled(currentPlaybackState == .stopped)
				}
			}
		}
		.onReceive(displayLinkPublisher.receive(on: RunLoop.main)) { _ in
			if let progress = playerController.player.time?.progress {
				currentPosition = progress
			}
		}
		.onReceive(playerController.playbackStatePublisher.receive(on: RunLoop.main)) {
			currentPlaybackState = $0
		}
	}
}

struct PlayerView_Previews: PreviewProvider {
    static var previews: some View {
		PlayerView(PlayerController(), track: Track(URL(fileURLWithPath: "fnord")))
    }
}
