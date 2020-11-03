/*
 * Copyright (c) 2009 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

import Cocoa

/// An item in a playlist
struct PlaylistItem: Identifiable {
	/// The unique identifier of this item
	let id = UUID()
	/// The URL holding the audio data
	let url: URL

	/// Audio properties for the playlist item
	let properties: AudioProperties
	/// Audio metadata and attached pictures for the playlist item
	let metadata: AudioMetadata

	/// Reads audio properties and metadata and initializes a playlist item
	init(_ url: URL) {
		self.url = url
		if let audioFile = try? AudioFile(readingPropertiesAndMetadataFrom: url) {
			self.properties = audioFile.properties
			self.metadata = audioFile.metadata
		}
		else {
			self.properties = AudioProperties()
			self.metadata = AudioMetadata()
		}
	}

	/// Returns a decoder for this playlist item or `nil` if the audio type is unknown
	func decoder(enableDoP: Bool = false) throws -> PCMDecoding? {
		let pathExtension = url.pathExtension.lowercased()
		if AudioDecoder.handlesPaths(withExtension: pathExtension) {
			return try AudioDecoder(url: url)
		}
		else if DSDDecoder.handlesPaths(withExtension: pathExtension) {
			let dsdDecoder = try DSDDecoder(url: url)
			return enableDoP ? try DoPDecoder(decoder: dsdDecoder) : try DSDPCMDecoder(decoder: dsdDecoder)
		}
		return nil
	}
}

extension PlaylistItem: Equatable {
	/// Returns true if the two playlist items have the same `id`
	static func ==(lhs: PlaylistItem, rhs: PlaylistItem) -> Bool {
		return lhs.id == rhs.id
	}
}

extension AttachedPicture {
	/// Returns an `NSImage` initialized with `self.imageData`
	var image: NSImage? {
		return NSImage(data: imageData)
	}
}

/// A window controller managing a playlist and audio player
class PlayerWindowController: NSWindowController {
	/// Returns an array of path extensions supported by the player
	static var supportedPathExtensions: [String] = {
		var pathExtensions = [String]()
		pathExtensions.append(contentsOf: AudioDecoder.supportedPathExtensions)
		pathExtensions.append(contentsOf: DSDDecoder.supportedPathExtensions)
		return pathExtensions
	}()

	@IBOutlet weak var slider: NSSlider!
	@IBOutlet weak var elapsed: NSTextField!
	@IBOutlet weak var remaining: NSTextField!
	@IBOutlet weak var playButton: NSButton!
	@IBOutlet weak var forwardButton: NSButton!
	@IBOutlet weak var backwardButton: NSButton!
	@IBOutlet weak var albumArt: NSImageView!
	@IBOutlet weak var title: NSTextField!
	@IBOutlet weak var artist: NSTextField!
	@IBOutlet weak var playlistTable: NSTableView!

	/// The audio player instance
	let player = AudioPlayer()
	/// Dispatch source for periodic UI updates
	private var timer: DispatchSourceTimer!
	/// The list of items managed by this object
	var playlist: [PlaylistItem] = []

	override var windowNibName: NSNib.Name? {
		return "PlayerWindow"
	}

	override func windowDidLoad() {
		player.delegate = self

		// Create a repeating timer to update the UI with the player's playback position
		timer = DispatchSource.makeTimerSource(queue: DispatchQueue.main)
		timer.schedule(deadline: DispatchTime.now(), repeating: .milliseconds(200), leeway: .milliseconds(100))

		timer.setEventHandler {
			switch self.player.playbackState {
			case .playing:
				self.playButton.title = "Pause"
			case .paused:
				self.playButton.title = "Resume"
			case .stopped:
				self.playButton.title = "Stopped"
			@unknown default:
				fatalError()
			}

			if let time = self.player.time {
				if let progress = time.progress {
					self.slider.doubleValue = progress
				}

				if let current = time.current {
					self.elapsed.doubleValue = current
					if let remaining = time.remaining {
						self.remaining.doubleValue = -1 * remaining
					}
				}
			}
		}

		timer.resume()

		disableUI()

		if let urls = UserDefaults.standard.object(forKey: "playlistURLs") as? [String] {
			for url in urls {
				if let u = URL(string: url) {
					let item = PlaylistItem(u)
					playlist.append(item)
				}
			}
		}
	}

	// MARK: - Actions

	@IBAction func play(_ sender: AnyObject?) {
		do {
			try player.play()
		}
		catch let error {
			NSApp.presentError(error)
		}
	}

	@IBAction func pause(_ sender: AnyObject?) {
		player.pause()
	}

	@IBAction func stop(_ sender: AnyObject?) {
		player.stop()
	}

	@IBAction func playPause(_ sender: AnyObject?) {
		do {
			try player.togglePlayPause()
		}
		catch let error {
			NSApp.presentError(error)
		}
	}

	@IBAction func seekForward(_ sender: AnyObject?) {
		player.seekForward()
	}

	@IBAction func seekBackward(_ sender: AnyObject?) {
		player.seekBackward()
	}

	@IBAction func seek(_ sender: AnyObject?) {
		if let requested = sender?.doubleValue, let current = player.position?.progress {
			let tolerance = 0.01
			if abs(current - requested) >= tolerance {
				player.seek(position: requested)
			}
		}
	}

	@IBAction func playNextItem(_ sender: AnyObject?) {
		guard let item = nextItem else {
			return
		}
		sequence(item: item)
	}

	@IBAction func playPreviousItem(_ sender: AnyObject?) {
		guard let item = previousItem else {
			return
		}
		sequence(item: item)
	}

	@IBAction func playlistDoubleAction(_ sender: AnyObject?) {
		let row = playlistTable.clickedRow
		let item = playlist[row]
		play(item: item)
	}

	@IBAction func delete(_ sender: AnyObject?) {
		removeFromPlaylist(items: selectedItems)
	}

	// MARK: - Player Control

	func play(url: URL) {
		addToPlaylist(url: url)
		if let item = item(for: url) {
			play(item: item)
		}
	}

	func play(item: PlaylistItem) {
		do {
			if let decoder = try item.decoder() {
				try player.play(decoder)
			}
		}
		catch let error {
			NSApp.presentError(error)
		}
	}

	// MARK: - Playlist

	func addToPlaylist(url: URL) {
		addToPlaylist(urls: [url])
	}

	func addToPlaylist(urls: [URL]) {
		for url in urls {
			guard !playlist.contains(where: { $0.url == url}) else {
				continue
			}
			let item = PlaylistItem(url)
			playlist.append(item)
		}
		playlistTable.reloadData()
	}

	func removeFromPlaylist(url: URL) {
		removeFromPlaylist(urls: [url])
	}

	func removeFromPlaylist(urls: [URL]) {
		for url in urls {
			guard let index = itemIndex(of: url) else {
				continue
			}

			if player.nowPlaying?.inputSource.url == url {
				player.stop()
			}

			playlist.remove(at: index)
		}
		playlistTable.reloadData()
	}

	func removeFromPlaylist(item: PlaylistItem) {
		removeFromPlaylist(items: [item])
	}

	func removeFromPlaylist(items: [PlaylistItem]) {
		removeFromPlaylist(urls: items.map({ $0.url }))
	}

	func item(for url: URL) -> PlaylistItem? {
		guard let index = itemIndex(of: url) else {
			return nil
		}
		return playlist[index]
	}

	func itemIndex(of url: URL) -> Int? {
		return playlist.firstIndex(where: { $0.url == url })
	}

	func itemIndex(of item: PlaylistItem) -> Int? {
		return itemIndex(of: item.url)
	}

	func playingItem() -> PlaylistItem? {
		guard let url = player.nowPlaying?.inputSource.url else {
			return nil
		}
		return item(for: url)
	}

	var selectedItems: [PlaylistItem] {
		var items: [PlaylistItem] = []
		for row in playlistTable.selectedRowIndexes {
			items.append(playlist[row])
		}
		return items
	}

	var nextItem: PlaylistItem? {
		guard let url = player.nowPlaying?.inputSource.url, let index = itemIndex(of: url) else {
			return nil
		}
		guard playlist.indices.contains(index + 1) else {
			return nil
		}
		return playlist[index + 1]
	}

	var previousItem: PlaylistItem? {
		guard let url = player.nowPlaying?.inputSource.url, let index = itemIndex(of: url) else {
			return nil
		}
		guard playlist.indices.contains(index - 1) else {
			return nil
		}
		return playlist[index - 1]
	}

	// MARK: - UI

	private func updateForNowPlayingChange() {
		guard let nowPlaying = player.nowPlaying else {
			disableUI()
			return
		}

		self.playButton.state = .off
		self.playButton.isEnabled = true

		let seekable = player.supportsSeeking
		slider.isEnabled = seekable
		backwardButton.isEnabled = seekable
		forwardButton.isEnabled = seekable

		elapsed.isHidden = false
		if player.frameLength != UnknownFrameLength {
			remaining.isHidden = false
		}

		// Update the track display
		updateUI(for: nowPlaying)
	}

	private func disableUI() {
		// Disable the playback controls
		playButton.state = .off
		playButton.isEnabled = false

		slider.isEnabled = false
		backwardButton.isEnabled = false
		forwardButton.isEnabled = false

		elapsed.isHidden = true
		remaining.isHidden = true

		// Blank the track display
		window?.representedURL = nil
		window?.title = ""

		albumArt.image = NSImage(named: "NSApplicationIcon")
		title.stringValue = ""
		artist.stringValue = ""
	}

	private func updateUI(for decoder: PCMDecoding) {
		guard let url = decoder.inputSource.url else {
			return
		}

		// Update the window's title and represented file
		window?.representedURL = url
		window?.title = FileManager.default.displayName(atPath: url.path)

		NSDocumentController.shared.noteNewRecentDocumentURL(url)

		if let url = decoder.inputSource.url, let metadata = item(for: url)?.metadata {
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

	// MARK: - Internals

	/// Enqueues or plays the playlist item based on the player's playback state
	private func sequence(item: PlaylistItem) {
		let wasPlaying = player.isPlaying
		player.pause()
		do {
			if let decoder = try item.decoder() {
				if wasPlaying {
					try player.play(decoder)
				}
				else {
					try player.enqueue(decoder, immediate: true)
				}
			}
		}
		catch let error {
			NSApp.presentError(error)
		}
	}

}

// MARK: - NSTableViewDataSource
extension PlayerWindowController: NSTableViewDataSource {
	func numberOfRows(in tableView: NSTableView) -> Int {
		return playlist.count
	}
}

// MARK: - NSTableViewDelegate
extension PlayerWindowController: NSTableViewDelegate {
	func tableView(_ tableView: NSTableView, viewFor tableColumn: NSTableColumn?, row: Int) -> NSView? {
		guard playlist.indices.contains(row) else {
			return nil
		}

		let item = playlist[row]
		if let identifier = tableColumn?.identifier, let view = tableView.makeView(withIdentifier: identifier, owner: nil) as? NSTableCellView {
			view.textField?.stringValue = item.metadata.title ?? FileManager.default.displayName(atPath: item.url.lastPathComponent)
			return view
		}

		return nil
	}
}

// MARK: - NSMenuItemValidation
extension PlayerWindowController: NSMenuItemValidation {
	func validateMenuItem(_ menuItem: NSMenuItem) -> Bool {
		if menuItem.action == #selector(PlayerWindowController.play(_:)) {
			return !player.isPlaying
		}
		else if menuItem.action == #selector(PlayerWindowController.pause(_:)) {
			return !player.isPaused
		}
		else if menuItem.action == #selector(PlayerWindowController.stop(_:)) {
			return !player.isStopped
		}
		else if menuItem.action == #selector(PlayerWindowController.playPause(_:)) {
			let playbackState = player.playbackState
			if playbackState == .playing {
				menuItem.title = "Pause"
				return true
			}
			else if playbackState == .paused {
				menuItem.title = "Resume"
				return true
			}
			else {
				menuItem.title = "Play"
				return false
			}
		}
		else if menuItem.action == #selector(PlayerWindowController.playNextItem(_:)) {
			return nextItem != nil
		}
		else if menuItem.action == #selector(PlayerWindowController.playPreviousItem(_:)) {
			return previousItem != nil
		}
		else if menuItem.action == #selector(PlayerWindowController.delete(_:)) {
			return !selectedItems.isEmpty
		}

		return responds(to: menuItem.action)
	}
}

// MARK: - NSWindowDelegate
extension PlayerWindowController: NSWindowDelegate {
	func windowWillClose(_ notification: Notification) {
		player.stop()

		let urls = playlist.map({ $0.url.absoluteString })
		UserDefaults.standard.set(urls, forKey: "playlistURLs")
	}
}

// MARK: - AudioPlayer.Delegate
extension PlayerWindowController: AudioPlayer.Delegate {
	func audioPlayer(_ audioPlayer: AudioPlayer, decodingComplete decoder: PCMDecoding) {
		if let url = decoder.inputSource.url, let index = itemIndex(of: url) {
			let nextIndex = playlist.index(after: index)
			if playlist.count > nextIndex {
				do {
					if let decoder = try playlist[nextIndex].decoder() {
						try player.enqueue(decoder)
					}
				}
				catch let error {
					DispatchQueue.main.async {
						NSApp.presentError(error)
					}
				}
			}
		}
	}

	func audioPlayerNowPlayingChanged(_ audioPlayer: AudioPlayer) {
		DispatchQueue.main.async {
			self.updateForNowPlayingChange()
		}
	}

	func audioPlayer(_ audioPlayer: AudioPlayer, encounteredError error: Error) {
		DispatchQueue.main.async {
			NSApp.presentError(error)
		}
	}
}
