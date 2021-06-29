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

	@Published var currentPosition: Double = 0

//	var foo: Binding = Binding(
//		get: { return dataModel },
//		set: {
//		if let current = playerController.player.position?.progress {
//			let tolerance = 0.01
//			if abs(current - $0) >= tolerance {
//				playerController.player.seek(position: $0)
//			}

	private let displayLinkPublisher = DisplayLinkPublisher()
	private var bag = Set<AnyCancellable>()

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
			.store(in: &bag)
	}

	deinit {
		bag.removeAll()
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

