//
// Copyright (c) 2011 - 2020 Stephen F. Booth <me@sbooth.org>
// See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
//

import SwiftUI
import Combine

struct PlayerView: View {
	@State private var currentPlaybackState: AudioPlayer.PlaybackState = .stopped
	@State private var currentTime: TimeInterval = 0.0
	@State private var currentTimeString: String = ""
	@State private var currentPosition: Double = 0.0

//	@State private var currentMetadata: AudioMetadata = AudioMetadata()

	private var player: AudioPlayer
	private let timePublisher: PassthroughSubject<AudioPlayer.PlaybackTime, Never>
	private let playbackStatePublisher: NSObject.KeyValueObservingPublisher<AudioPlayer, AudioPlayer.PlaybackState>

	init(_ player: AudioPlayer) {
		self.player = player
		timePublisher = player.timePublisher
		playbackStatePublisher = player.publisher(for: \.playbackState)
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
			VStack(spacing: 24) {
				Image(systemName: "s.square")
					.resizable()
					.frame(width: geometry.size.width - 24, height: geometry.size.width - 24)
					.padding()
					.cornerRadius(20)
					.shadow(radius: 10)

				VStack(spacing: 8) {
					Text("Track Title")
						.font(Font.system(.title).bold())
					Text("Artist Name")
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
							try? self.player.playPause()
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

					Text(self.currentTimeString)
						.font(.system(.headline))
				}
			}
		}
		.onReceive(timePublisher.receive(on: RunLoop.main)) {
			self.currentTime = $0.current
			self.currentPosition = $0.current / $0.total
			if $0.current != -1 {
				self.currentTimeString = self.formatter.string(from: $0.current) ?? ""
			}
			else {
				self.currentTimeString = ""
			}
		}
		.onReceive(playbackStatePublisher.receive(on: RunLoop.main)) {
			self.currentPlaybackState = $0
		}
	}
}

struct ContentView_Previews: PreviewProvider {
    static var previews: some View {
		PlayerView(AudioPlayer())
    }
}
