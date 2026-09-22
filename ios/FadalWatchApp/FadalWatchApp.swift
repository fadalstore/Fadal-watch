import SwiftUI

@main
struct FadalWatchApp: App {
    var body: some Scene {
        WindowGroup {
            ContentView()
        }
    }
}

struct ContentView: View {
    var body: some View {
        NavigationStack {
            VStack(alignment: .leading, spacing: 16) {
                Text("FadalWatch")
                    .font(.largeTitle.bold())
                Text("Fadal Kernel UEFI and security development")
                    .font(.headline)
                Text("The x86_64 UEFI image is provided as a UTM QEMU package. Import the FadalWatch UEFI bundle into UTM and choose Emulate.")
                    .foregroundStyle(.secondary)
                Link("Open UTM documentation", destination: URL(string: "https://docs.getutm.app/")!)
                Spacer()
            }
            .padding()
            .navigationTitle("FadalWatch")
        }
    }
}
