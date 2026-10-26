//
// SPDX-FileCopyrightText: 2026 Jon Leopard <contact@jonleopard.com>
// SPDX-License-Identifier: MIT
//
// Part of https://github.com/sbooth/SFBAudioEngine
//

import AVFAudio
import Darwin
import ObjectiveC
import XCTest
@testable import SFBAudioEngine

final class AudioPlayerLifetimeTests: XCTestCase {
    func testLastReferenceReleasedByDecoderCanceledCallbackDoesNotJoinItsOwnThread() throws {
        let decoding = expectation(description: "Decoding starts on the player's worker")
        let canceled = expectation(description: "Cancellation callback holds the player")
        let released = expectation(description: "The callback can release the last player reference")
        let nativeReleased = expectation(description: "Native engine is released after both worker joins")
        let gate = DispatchSemaphore(value: 0)
        defer { gate.signal() }
        let delegate = HeldCancellationDelegate(decoding: decoding, canceled: canceled, gate: gate)
        defer { withExtendedLifetime(delegate) {} }
        var player: AudioPlayer? = AudioPlayer()
        observeRelease(of: try XCTUnwrap(player), on: "AudioPlayer.Events", fulfilling: released, nativeReleased: nativeReleased)
        weak var reference = player
        player?.delegate = delegate
        try player?.enqueue(makeDecoder())
        wait(for: [decoding], timeout: 3)
        player?.stop()
        wait(for: [canceled], timeout: 3)
        player = nil
        XCTAssertNotNil(reference, "The SDK callback still holds the only player reference")
        gate.signal()
        wait(for: [released, nativeReleased], timeout: 3)
        XCTAssertNil(reference)
    }

    func testLastReferenceReleasedByGraphCallbackDoesNotJoinItsOwnThread() throws {
        let reconfiguring = expectation(description: "Graph callback holds the player")
        let released = expectation(description: "The graph callback can release the last player reference")
        let nativeReleased = expectation(description: "Native engine is released after both worker joins")
        let gate = DispatchSemaphore(value: 0)
        defer { gate.signal() }
        let mixer = AVAudioMixerNode()
        let delegate = HeldGraphDelegate(reconfiguring: reconfiguring, gate: gate, mixer: mixer)
        defer { withExtendedLifetime(delegate) {} }
        var player: AudioPlayer? = AudioPlayer()
        observeRelease(of: try XCTUnwrap(player), on: "AudioPlayer.Decoding", fulfilling: released, nativeReleased: nativeReleased)
        weak var reference = player
        player?.delegate = delegate
        player?.modifyProcessingGraph { engine in
            guard let player else { return }
            let format = player.sourceNode.outputFormat(forBus: 0)
            engine.attach(mixer)
            engine.disconnectNodeOutput(player.sourceNode)
            engine.connect(player.sourceNode, to: mixer, format: format)
            engine.connect(mixer, to: engine.mainMixerNode, format: format)
        }
        try player?.enqueue(makeDecoder(sampleRate: 48_000))
        wait(for: [reconfiguring], timeout: 3)
        player = nil
        XCTAssertNotNil(reference, "The SDK graph callback still holds the only player reference")
        gate.signal()
        wait(for: [released, nativeReleased], timeout: 3)
        XCTAssertNil(reference)
    }

    private func observeRelease(of player: AudioPlayer, on threadName: String, fulfilling released: XCTestExpectation, nativeReleased: XCTestExpectation) {
        // ~AudioPlayer joins both workers (AudioPlayer.mm:629–644) before releasing its engine_ member.
        player.modifyProcessingGraph { engine in
            objc_setAssociatedObject(engine, &releaseObserverKey, ReleaseObserver {
                nativeReleased.fulfill()
            }, .OBJC_ASSOCIATION_RETAIN_NONATOMIC)
        }
        // Associations are released after .cxx_destruct, so an inline self-join cannot look like a passed test.
        objc_setAssociatedObject(player, &releaseObserverKey, ReleaseObserver {
            var name = [CChar](repeating: 0, count: 64)
            pthread_getname_np(pthread_self(), &name, name.count)
            XCTAssertEqual(String(cString: name), threadName)
            released.fulfill()
        }, .OBJC_ASSOCIATION_RETAIN_NONATOMIC)
    }

    private func makeDecoder(sampleRate: UInt32 = 44_100) throws -> AudioDecoder {
        let frames = sampleRate
        let bytes = frames * 4
        var wav = Data("RIFF".utf8)
        func append<T: FixedWidthInteger>(_ value: T) {
            var littleEndian = value.littleEndian
            withUnsafeBytes(of: &littleEndian) { wav.append(contentsOf: $0) }
        }
        append(bytes + 36)
        wav.append(contentsOf: "WAVEfmt ".utf8)
        append(UInt32(16))
        append(UInt16(1))
        append(UInt16(2))
        append(sampleRate)
        append(sampleRate * 4)
        append(UInt16(4))
        append(UInt16(16))
        wav.append(contentsOf: "data".utf8)
        append(bytes)
        wav.append(Data(repeating: 0, count: Int(bytes)))
        return try AudioDecoder(inputSource: InputSource(data: wav), decoderName: .coreAudio)
    }
}

private var releaseObserverKey: UInt8 = 0

private final class ReleaseObserver: NSObject {
    private let released: () -> Void

    init(released: @escaping () -> Void) { self.released = released }

    deinit { released() }
}

private final class HeldCancellationDelegate: NSObject, AudioPlayer.Delegate {
    private let decoding: XCTestExpectation
    private let canceled: XCTestExpectation
    private let gate: DispatchSemaphore

    init(decoding: XCTestExpectation, canceled: XCTestExpectation, gate: DispatchSemaphore) {
        self.decoding = decoding
        self.canceled = canceled
        self.gate = gate
    }

    func audioPlayer(_ audioPlayer: AudioPlayer, decodingStarted decoder: any PCMDecoding) {
        decoding.fulfill()
    }

    func audioPlayer(_ audioPlayer: AudioPlayer, decoderCanceled decoder: any PCMDecoding, framesRendered: AVAudioFramePosition) {
        canceled.fulfill()
        XCTAssertEqual(gate.wait(timeout: .now() + 5), .success)
    }
}

private final class HeldGraphDelegate: NSObject, AudioPlayer.Delegate {
    private let reconfiguring: XCTestExpectation
    private let gate: DispatchSemaphore
    private let mixer: AVAudioMixerNode

    init(reconfiguring: XCTestExpectation, gate: DispatchSemaphore, mixer: AVAudioMixerNode) {
        self.reconfiguring = reconfiguring
        self.gate = gate
        self.mixer = mixer
    }

    func audioPlayer(_ audioPlayer: AudioPlayer, reconfigureProcessingGraph engine: AVAudioEngine, with format: AVAudioFormat) -> AVAudioNode {
        reconfiguring.fulfill()
        XCTAssertEqual(gate.wait(timeout: .now() + 5), .success)
        engine.disconnectNodeOutput(mixer)
        engine.connect(mixer, to: engine.mainMixerNode, format: format)
        return mixer
    }
}
