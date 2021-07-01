//
// Copyright (c) 2011 - 2021 Stephen F. Booth <me@sbooth.org>
// Part of https://github.com/sbooth/SFBAudioEngine
// MIT license
//

import SwiftUI

@main
struct SimplePlayerApp: App {
	@StateObject private var model = DataModel()

	var body: some Scene {
		WindowGroup {
			ContentView()
				.environmentObject(model)
				.onAppear {
					model.load()
				}
		}
	}
}
