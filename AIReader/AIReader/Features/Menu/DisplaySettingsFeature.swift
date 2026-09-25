import ComposableArchitecture
import SwiftUI

/// Type size, face, leading and margins. Edits land in the shared reading
/// style, so the page behind re-renders as they are made.
@Reducer
struct DisplaySettingsFeature {
    @ObservableState
    struct State: Equatable {
        @Shared(.readingStyle) var stored
        /// The bindings edit this copy; every change is written back to the
        /// shared value, which is what the reader renders from.
        var style: ReadingStyle

        init() {
            style = _stored.wrappedValue
        }
    }

    enum Action: BindableAction {
        case binding(BindingAction<State>)
    }

    var body: some ReducerOf<Self> {
        BindingReducer()
            .onChange(of: \.style) { _, style in
                Reduce { state, _ in
                    state.$stored.withLock { $0 = style }
                    return .none
                }
            }
    }
}

struct DisplaySettingsView: View {
    @Bindable var store: StoreOf<DisplaySettingsFeature>

    var body: some View {
        Form {
            Section("Text") {
                LabeledContent("Size") {
                    Slider(
                        value: $store.style.scale,
                        in: ReadingStyle.scaleRange,
                        step: 0.1
                    )
                }
                Picker("Face", selection: $store.style.fontName) {
                    Text("As published").tag(String?.none)
                    ForEach(ReadingStyle.fontNames, id: \.self) { name in
                        Text(name).tag(String?.some(name))
                    }
                }
            }

            Section("Layout") {
                LabeledContent("Line spacing") {
                    Slider(
                        value: $store.style.lineSpacing,
                        in: ReadingStyle.lineSpacingRange,
                        step: 1
                    )
                }
                LabeledContent("Margins") {
                    Slider(
                        value: $store.style.margin,
                        in: ReadingStyle.marginRange,
                        step: 4
                    )
                }
            }

            Section {
                Text("The quick brown fox jumps over the lazy dog.")
                    .font(preview)
                    .lineSpacing(store.style.lineSpacing)
            } header: {
                Text("Preview")
            }
        }
        .navigationTitle("Display")
    }

    private var preview: Font {
        let size = 12 * store.style.scale
        return store.style.fontName.map { Font.custom($0, size: size) }
            ?? .system(size: size)
    }
}
