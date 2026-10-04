// The one assertion the checks use: a line per check, and a count of
// failures for the exit code.

export let failures = 0;

export function check(name, ok, detail = "") {
  if (!ok) failures++;
  console.log(`${ok ? "ok  " : "FAIL"}  ${name}${detail && !ok ? ` — ${detail}` : ""}`);
}

export function skip(name, why) {
  console.log(`skip  ${name} (${why})`);
}
