//
// Copyright (c) 2011 - 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

import UIKit
import SwiftUI

class SceneDelegate: UIResponder, UIWindowSceneDelegate {
	var window: UIWindow?

	func scene(_ scene: UIScene, willConnectTo session: UISceneSession, options connectionOptions: UIScene.ConnectionOptions) {
		let delegate = UIApplication.shared.delegate as! AppDelegate
		let contentView = ContentView(delegate.playerController)

		// Use a UIHostingController as window root view controller.
		if let windowScene = scene as? UIWindowScene {
		    let window = UIWindow(windowScene: windowScene)
			window.rootViewController = UIHostingController(rootView: contentView)
		    self.window = window
		    window.makeKeyAndVisible()
		}
	}
}
