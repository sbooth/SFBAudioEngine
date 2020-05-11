//
// Copyright (c) 2011 - 2020 Stephen F. Booth <me@sbooth.org>
// See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
//

import SwiftUI
import Combine

struct PlayerView: View {
	@State private var currentPlaybackState: AudioPlayer.PlaybackState = .stopped
	@State private var currentPosition: Double = 0.0

	private var title: String = "[title]"
	private var artist: String = "[artist]"
	private var image: UIImage? = nil

	private let player: AudioPlayer
	private let timePublisher: PassthroughSubject<AudioPlayer.PlaybackTime, Never>
	private let playbackStatePublisher: NSObject.KeyValueObservingPublisher<AudioPlayer, AudioPlayer.PlaybackState>

	init(_ player: AudioPlayer, track: Track) {
		self.player = player
		timePublisher = player.timePublisher
		playbackStatePublisher = player.publisher(for: \.playbackState)

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
				if self.image != nil {
					Image(uiImage: self.image!)
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
					Text(self.title)
						.font(Font.system(.title).bold())
					Text(self.artist)
						.font(.system(.headline))
				}

				VStack(spacing: 8) {
					HStack(spacing: 40) {
						Button(action: {
							self.player.seekBackward()
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
						.disabled(self.currentPlaybackState == .stopped)

						Button(action: {
							try? self.player.togglePlayPause()
						}) {
							ZStack {
								Circle()
									.frame(width: 80, height: 80)
									.accentColor(.pink)
									.shadow(radius: 10)
								Image(systemName: self.currentPlaybackState == .playing ? "pause.fill" : "play.fill")
									.foregroundColor(.white)
									.font(.system(.title))
							}

						}
						.disabled(self.currentPlaybackState == .stopped)

						Button(action: {
							self.player.seekForward()
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
						.disabled(self.currentPlaybackState == .stopped)
					}

					Slider(value: Binding(
						get: { return self.currentPosition },
						set: { self.player.seek(position: Float($0)) }
					))
						.padding(.horizontal, 20.0)
						.accentColor(.pink)
						.disabled(self.currentPlaybackState == .stopped)
				}
			}
		}
		.onReceive(timePublisher.receive(on: RunLoop.main)) {
			self.currentPosition = $0.current / $0.total
		}
		.onReceive(playbackStatePublisher.receive(on: RunLoop.main)) {
			self.currentPlaybackState = $0
		}
	}
}

struct PlayerView_Previews: PreviewProvider {
    static var previews: some View {
		PlayerView(AudioPlayer(), track: Track(URL(fileURLWithPath: "fnord")))
    }
}
