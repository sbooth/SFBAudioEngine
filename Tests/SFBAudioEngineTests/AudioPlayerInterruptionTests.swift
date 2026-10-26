//
// SPDX-FileCopyrightText: 2026 Jon Leopard <contact@jonleopard.com>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

import AVFAudio
import XCTest
@testable import SFBAudioEngine

#if os(iOS) || os(tvOS)
final class AudioPlayerInterruptionTests: XCTestCase {
    func testFailedResumePreservesInterruptedPlaybackForAutomaticResume() throws {
        let session = AVAudioSession.sharedInstance()
        try session.setCategory(.playback)
        try session.setActive(true)
        let player = AudioPlayer()
        defer { player.stop() }
        try player.play()
        XCTAssertTrue(player.isPlaying)
        postInterruption(.began)
        // A real interruption stops the engine; synthetic notifications only change SFB's flags.
        player.modifyProcessingGraph { $0.stop() }
        XCTAssertFalse(player.resume())
        postInterruption(.ended, options: .shouldResume)
        XCTAssertTrue(player.isPlaying, "An unsuccessful explicit resume must not discard the saved playback intent")
    }

    func testExplicitPauseAfterInterruptionPreventsAutomaticResume() throws {
        let session = AVAudioSession.sharedInstance()
        try session.setCategory(.playback)
        try session.setActive(true)
        let player = AudioPlayer()
        defer { player.stop() }
        try player.play()
        postInterruption(.began)
        player.modifyProcessingGraph { $0.stop() }
        _ = player.pause()
        postInterruption(.ended, options: .shouldResume)
        XCTAssertFalse(player.isPlaying, "Explicit Pause cancels the interruption's saved playback intent")
        XCTAssertFalse(player.engineIsRunning)
    }

    private func postInterruption(_ type: AVAudioSession.InterruptionType, options: AVAudioSession.InterruptionOptions = []) {
        NotificationCenter.default.post(name: AVAudioSession.interruptionNotification,
                                        object: AVAudioSession.sharedInstance(),
                                        userInfo: [AVAudioSessionInterruptionTypeKey: type.rawValue,
                                                   AVAudioSessionInterruptionOptionKey: options.rawValue])
    }
}
#endif
