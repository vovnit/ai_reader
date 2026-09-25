import ComposableArchitecture
import SwiftUI

struct ChatView: View {
    @Bindable var store: StoreOf<ChatFeature>
    /// The field takes the keyboard as the screen opens: it is what the
    /// screen is for.
    @FocusState private var isComposing: Bool

    var body: some View {
        ScrollViewReader { proxy in
            ScrollView {
                LazyVStack(alignment: .leading, spacing: 10) {
                    if store.turns.isEmpty {
                        Text(store.hint)
                            .font(.callout)
                            .foregroundStyle(.secondary)
                            .padding(.top, 8)
                    }
                    ForEach(store.turns) { turn in
                        ChatBubble(turn: turn).id(turn.id)
                    }
                    if store.isAnswering {
                        ProgressView()
                            .padding(12)
                            .glassEffect(in: .circle)
                            .id("progress")
                    }
                    if let message = store.errorMessage {
                        Text(message)
                            .font(.footnote)
                            .foregroundStyle(.red)
                    }
                }
                .frame(maxWidth: .infinity, alignment: .leading)
                .padding(.horizontal)
                .padding(.vertical, 12)
            }
            .scrollDismissesKeyboard(.interactively)
            .onChange(of: store.turns.count) { _, _ in
                withAnimation { proxy.scrollTo(store.turns.last?.id, anchor: .bottom) }
            }
        }
        .safeAreaInset(edge: .bottom) { composer }
        .navigationTitle("Chat")
        .onAppear { isComposing = true }
    }

    /// The question field and its send button, floating over the thread.
    private var composer: some View {
        GlassEffectContainer(spacing: 10) {
            HStack(spacing: 10) {
                TextField("Ask", text: $store.draft, axis: .vertical)
                    .lineLimit(1...5)
                    .textFieldStyle(.plain)
                    .focused($isComposing)
                    .padding(.horizontal, 16)
                    .padding(.vertical, 11)
                    .glassEffect(.regular.interactive(), in: .capsule)
                    .onSubmit { store.send(.sendTapped) }

                // Glass applied by hand rather than through `.glassProminent`,
                // whose own padding sets the height and would not match the field.
                Button { store.send(.sendTapped) } label: {
                    Image(systemName: "arrow.up")
                        .font(.headline)
                        .foregroundStyle(store.canSend ? Color.white : Color.secondary)
                        .frame(width: 48)
                        .frame(maxHeight: .infinity)
                        .contentShape(.capsule)
                }
                .buttonStyle(.plain)
                .glassEffect(
                    .regular.tint(store.canSend ? .accentColor : nil).interactive(),
                    in: .capsule
                )
                .disabled(!store.canSend)
            }
            // Holds the row to the field's height, so the button fills exactly that.
            .fixedSize(horizontal: false, vertical: true)
            .padding(.horizontal)
            .padding(.bottom, 8)
        }
    }
}

/// One turn of the conversation: the reader's question on the right, the
/// model's answer on the left.
private struct ChatBubble: View {
    let turn: ChatTurn

    var body: some View {
        HStack {
            if turn.isReader { Spacer(minLength: 44) }

            // The reader's own words stay as typed; the model answers in Markdown.
            Text(turn.isReader ? AttributedString(turn.text) : ChatMarkdown.render(turn.text))
                .textSelection(.enabled)
                .padding(.horizontal, 14)
                .padding(.vertical, 10)
                .glassEffect(
                    turn.isReader ? .regular.tint(.accentColor.opacity(0.4)) : .regular,
                    in: .rect(cornerRadius: 20)
                )

            if !turn.isReader { Spacer(minLength: 44) }
        }
    }
}
