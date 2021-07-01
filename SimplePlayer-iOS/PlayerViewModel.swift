//
// Copyright (c) 2011 - 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

import SwiftUI
import Combine
import SFBAudioEngine

class PlayerViewModel: ObservableObject {
	let dataModel: DataModel

	private let displayLinkPublisher = DisplayLinkPublisher()
	private var cancellables = Set<AnyCancellable>()

	private lazy var playbackProgressSubject = PassthroughSubject<Double, Never>()
	var playbackProgress: AnyPublisher<Double, Never> {
		playbackProgressSubject
			.eraseToAnyPublisher()
	}

	init(dataModel: DataModel) {
		self.dataModel = dataModel
		displayLinkPublisher
			.receive(on: DispatchQueue.main)
			.sink { _ in
				if let progress = dataModel.player.time?.progress {
					self.playbackProgressSubject.send(progress)
				}
			}
			.store(in: &cancellables)
	}

	deinit {
		cancellables.removeAll()
	}

	func seekBackward() {
		dataModel.player.seekBackward()
	}

	func seekForward() {
		dataModel.player.seekForward()
	}

	func togglePlayPause() {
		try? dataModel.player.togglePlayPause()
	}

	func seek(position: Double) {
		if let current = dataModel.player.position?.progress {
			let tolerance = 0.01
			if abs(current - position) >= tolerance {
				dataModel.player.seek(position: position)
			}
		}
	}
}

