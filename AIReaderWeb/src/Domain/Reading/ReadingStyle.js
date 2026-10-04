// How the book is rendered: the reader's own preference.
// `{ scale, fontName, lineSpacing, margin }`; an empty font name is the
// default serif.

export const defaultStyle = { scale: 1.4, fontName: "", lineSpacing: 2, margin: 24 };

/** Text size at scale 1, in CSS pixels. */
export const basePixelSize = 16;

export const ranges = {
  scale: { min: 0.8, max: 2.6, step: 0.1 },
  lineSpacing: { min: 0, max: 16, step: 1 },
  margin: { min: 8, max: 64, step: 4 },
};

/** Families worth offering: the default, plus faces that read well at length where the system has them. */
export const fontNames = ["Serif", "Sans", "Georgia", "Palatino", "Charter", "Iowan Old Style", "Helvetica Neue", "Verdana"];

/** One step up or down, rounded to the step so repeated taps do not drift. */
export function stepped(style, field, direction) {
  const { min, max, step } = ranges[field];
  const next = Math.round((style[field] + direction * step) / step) * step;
  return { ...style, [field]: Math.min(Math.max(Number(next.toFixed(2)), min), max) };
}

/** The CSS font stack for a style's face. */
export function fontFamily(style) {
  if (!style.fontName || style.fontName === "Serif") return "serif";
  if (style.fontName === "Sans") return "sans-serif";
  return `"${style.fontName}", serif`;
}

/**
 * The text area of a page in a container: the container less the margins,
 * and no wider than a line the eye can follow back — about 65 characters.
 */
export function pageSize(style, width, height) {
  const measure = 32 * 17 * style.scale;
  return {
    width: Math.floor(Math.max(Math.min(width - style.margin * 2, measure), 1)),
    height: Math.floor(Math.max(height - style.margin * 2, 1)),
  };
}

export function sanitizeStyle(value) {
  const style = { ...defaultStyle };
  for (const field of Object.keys(ranges)) {
    if (typeof value?.[field] === "number") style[field] = Math.min(Math.max(value[field], ranges[field].min), ranges[field].max);
  }
  if (typeof value?.fontName === "string") style.fontName = value.fontName === "Serif" ? "" : value.fontName;
  return style;
}
