/*
 * Copyright (c) 2009 - 2020 Stephen F. Booth <me@sbooth.org>
 * See https://github.com/sbooth/SFBAudioEngine/blob/master/LICENSE.txt for license information
 */

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
