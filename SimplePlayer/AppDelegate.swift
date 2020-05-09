/*
 * Copyright (c) 2009 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

import os.log

import Cocoa

@NSApplicationMain
class AppDelegate: NSObject {
	@IBOutlet weak var playerWindowController: PlayerWindowController!
	@IBOutlet weak var openURLPanel: NSWindow!
	@IBOutlet weak var openURLPanelTextField: NSTextField!

	@IBAction func openFile(_ sender: AnyObject?) {
		let openPanel = NSOpenPanel()

		openPanel.allowsMultipleSelection = false
		openPanel.canChooseDirectories = false
		openPanel.allowedFileTypes = playerWindowController.supportedPathExtensions

		if(openPanel.runModal() == .OK) {
			if let url = openPanel.urls.first {
				playerWindowController.play(url)
			}
		}
	}

	@IBAction func openURL(_ sender: AnyObject?) {
		openURLPanel.center()
		openURLPanel.makeKeyAndOrderFront(sender)
	}

	@IBAction func enqueueFiles(_ sender: AnyObject?) {
		let openPanel = NSOpenPanel()

		openPanel.allowsMultipleSelection = false
		openPanel.canChooseDirectories = false
		openPanel.allowedFileTypes = playerWindowController.supportedPathExtensions

		if(openPanel.runModal() == .OK) {
			if let url = openPanel.urls.first {
				playerWindowController.enqueue(url)
			}
		}
	}

	@IBAction func openURLPanelOpenAction(_ sender: AnyObject?) {
		openURLPanel.orderOut(sender)
		if let url = URL(string: openURLPanelTextField.stringValue) {
			playerWindowController.play(url)
		}
	}

	@IBAction func openURLPanelCancelAction(_ sender: AnyObject?) {
		openURLPanel.orderOut(sender)
	}

	@IBAction func analyzeFiles(_ sender: AnyObject?) {
		let openPanel = NSOpenPanel()

		openPanel.allowsMultipleSelection = true
		openPanel.canChooseDirectories = false
		openPanel.allowedFileTypes = playerWindowController.supportedPathExtensions

		if(openPanel.runModal() == .OK) {
			do {
				let rg = try ReplayGainAnalyzer.analyzeAlbum(openPanel.urls)
				os_log("Album gain %.2f dB, peak %.8f; Tracks: [%{public}@]", rg.0.gain, rg.0.peak, rg.1.map({ (url, replayGain) in String(format: "\"%@\" gain %.2f dB, peak %.8f", FileManager.default.displayName(atPath: url.lastPathComponent), replayGain.gain, replayGain.peak) }).joined(separator: ", "))
				let alert = NSAlert()
				alert.messageText = "Replay Gain Analysis Complete"
				alert.informativeText = "Check log for details."
				alert.runModal()
			}
			catch let error {
				NSApp.presentError(error)
			}
		}
	}
}

extension AppDelegate: NSApplicationDelegate {
	func applicationDidFinishLaunching(_ aNotification: Notification) {
		playerWindowController.showWindow(self)
	}

	func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool {
		true
	}

	func application(_ application: NSApplication, open urls: [URL]) {
		if let url = urls.first {
			playerWindowController.play(url)
		}
	}
}
