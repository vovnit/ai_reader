// A word or a sentence spoken in the book's language, by the system's own
// voices: on the device, free.

export const canSpeak = () => typeof speechSynthesis !== "undefined";

export function speak(text, language) {
  if (!canSpeak() || !text) return;
  speechSynthesis.cancel();
  const utterance = new SpeechSynthesisUtterance(text);
  if (language) utterance.lang = language;
  speechSynthesis.speak(utterance);
}
