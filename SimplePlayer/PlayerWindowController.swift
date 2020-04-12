/*
 * Copyright (c) 2009 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

import Cocoa

extension SFBAttachedPicture {
	var image: NSImage? {
		return NSImage(data: imageData)
	}
}

class PlayerWindowController: NSWindowController, NSWindowDelegate {
	var player: SFBAudioPlayer!
	var timer: DispatchSourceTimer!

	@IBOutlet weak var slider: NSSlider!
	@IBOutlet weak var elapsed: NSTextField!
	@IBOutlet weak var remaining: NSTextField!
	@IBOutlet weak var playButton: NSButton!
	@IBOutlet weak var forwardButton: NSButton!
	@IBOutlet weak var backwardButton: NSButton!
	@IBOutlet weak var albumArt: NSImageView!
	@IBOutlet weak var title: NSTextField!
	@IBOutlet weak var artist: NSTextField!

	override var windowNibName: NSNib.Name? {
		return "PlayerWindow"
	}

	override func awakeFromNib() {
		player = SFBAudioPlayer()

		player.renderingStartedNotificationHandler = { decoder in
			let url = decoder.inputSource.url
			DispatchQueue.main.async {
				self.updateWindow()
				if let url = url, url.isFileURL {
					NSDocumentController.shared.noteNewRecentDocumentURL(url)
				}
			}
		}

		player.renderingFinishedNotificationHandler = { decoder in
			DispatchQueue.main.async {
				self.updateWindow()
			}
		}

		player.errorNotificationHandler = { error in
			DispatchQueue.main.async {
				NSApp.presentError(error)
			}
		}

		timer = DispatchSource.makeTimerSource(queue: DispatchQueue.main)
		timer.schedule(deadline: DispatchTime.now(), repeating: .milliseconds(200), leeway: .milliseconds(300))

		timer.setEventHandler {
			if self.player.isPlaying {
				self.playButton.title = "Resume"
			}
			else {
				self.playButton.title = "Pause"
			}

			let position = self.player.position
			if position.current != -1 && position.total != -1 {
				self.slider.doubleValue = Double(position.current) / Double(position.total)
			}

			let time = self.player.time
			if time.current != -1 {
				self.elapsed.doubleValue = time.current
				if time.total != -1 {
					self.remaining.doubleValue = -1 * (time.total - time.current)
				}
			}
		}

		timer.resume()

		updateWindow()
	}

	func windowWillClose(_ notification: Notification) {
		try? player.stop()
	}

	@IBAction func playPause(_ sender: AnyObject?) {
		try? player.playPause()
	}

	@IBAction func seekForward(_ sender: AnyObject?) {
		player.seekForward()
	}

	@IBAction func seekBackward(_ sender: AnyObject?) {
		player.seekBackward()
	}

	@IBAction func seek(_ sender: AnyObject?) {
		if let position = sender?.floatValue {
			player.seek(position: position)
		}
	}

	@IBAction func skipToNextTrack(_ sender: AnyObject?) {
		player.skipToNext()
	}

	func updateWindow() {
		// Nothing happening, reset the window
		guard let url = player.url else {
			window?.representedURL = nil
			window?.title = ""

			slider.isEnabled = false
			playButton.state = .off
			playButton.isEnabled = false
			backwardButton.isEnabled = false
			forwardButton.isEnabled = false

			elapsed.isHidden = true
			remaining.isHidden = true

			albumArt.image = NSImage(named: "NSApplicationIcon")
			title.stringValue = ""
			artist.stringValue = ""

			return
		}

		let seekable = player.supportsSeeking

		// Update the window's title and represented file
		window?.representedURL = url
		window?.title = FileManager.default.displayName(atPath: url.path)

		NSDocumentController.shared.noteNewRecentDocumentURL(url)

		// Update the UI
		slider.isEnabled = seekable
		playButton.state = .off
		playButton.isEnabled = true
		backwardButton.isEnabled = seekable
		forwardButton.isEnabled = seekable

		// Show the times
		elapsed.isHidden = false
		if player.totalFrames != -1 {
			remaining.isHidden = false
		}

		// Load and display some metadata.  Normally the metadata would be read and stored in the background,
		// but for simplicity's sake it is done here.
		if let audioFile = try? SFBAudioFile(readingPropertiesAndMetadataFrom: url) {
			let metadata = audioFile.metadata

			if let picture = metadata.attachedPictures.randomElement() {
				albumArt.image = picture.image
			}
			else {
				albumArt.image = nil
			}

			title.stringValue = metadata.title ?? ""
			artist.stringValue = metadata.artist ?? ""
		}
		else {
			albumArt.image = NSImage(named: "NSApplicationIcon")
			title.stringValue = ""
			artist.stringValue = ""
		}
	}
}
