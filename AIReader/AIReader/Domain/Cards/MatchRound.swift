import Foundation

/// One round of the matching game: a few cards, their fronts in one column
/// and their backs in another, shuffled apart. The reader taps a front and a
/// back; a pair that belongs together is put aside, one that does not is a
/// miss. Pure state, so it can be checked without a screen.
struct MatchRound: Equatable, Sendable {
    /// A front and a back the reader put together, as indices into `cards`.
    struct Pick: Equatable, Sendable {
        let front: Int
        let back: Int
        let matched: Bool
    }

    let cards: [Card]
    /// The order the fronts and backs are shown in, as indices into `cards`.
    /// No back sits in the same row as its front.
    private(set) var fronts: [Int]
    private(set) var backs: [Int]
    private var matched: [Bool]
    private(set) var selectedFront: Int?
    private(set) var selectedBack: Int?
    /// The pair most recently put together, until the next tap.
    private(set) var lastPick: Pick?
    private(set) var misses = 0

    init(cards: [Card], using generator: inout some RandomNumberGenerator) {
        self.cards = cards
        matched = Array(repeating: false, count: cards.count)
        fronts = Array(cards.indices).shuffled(using: &generator)
        backs = fronts
        // A row that lines up would give its answer away.
        repeat {
            backs.shuffle(using: &generator)
        } while cards.count > 1 && zip(fronts, backs).contains { $0 == $1 }
    }

    func isMatched(_ card: Int) -> Bool { matched[card] }
    var matchedCount: Int { matched.filter { $0 }.count }
    var isComplete: Bool { matchedCount == cards.count }

    /// A tap on a front or a back. Tapping the chosen one again lets go of
    /// it; once one of each is chosen the pair resolves and is returned.
    @discardableResult
    mutating func pickFront(_ card: Int) -> Pick? {
        lastPick = nil
        guard canPick(card) else { return nil }
        selectedFront = selectedFront == card ? nil : card
        return resolve()
    }

    @discardableResult
    mutating func pickBack(_ card: Int) -> Pick? {
        lastPick = nil
        guard canPick(card) else { return nil }
        selectedBack = selectedBack == card ? nil : card
        return resolve()
    }

    private func canPick(_ card: Int) -> Bool {
        cards.indices.contains(card) && !matched[card]
    }

    private mutating func resolve() -> Pick? {
        guard let front = selectedFront, let back = selectedBack else { return nil }
        let pick = Pick(front: front, back: back, matched: front == back)
        if pick.matched { matched[front] = true } else { misses += 1 }
        selectedFront = nil
        selectedBack = nil
        lastPick = pick
        return pick
    }
}
