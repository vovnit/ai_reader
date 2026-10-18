// Zooming a PDF's page view: with a pinch, kept from the browser, which would
// zoom the whole page; with a trackpad's pinch, which arrives as a scroll
// with the control key held; or a step at a time, from the buttons and keys.
// The page is stretched at once and drawn sharp again once the zoom
// settles, the point zoomed about staying where it was on screen.
import { clampedZoom } from "./PagesFeature.js";

export class PageZoom {
  /** A zoom shown stretched before it is drawn: `{ zoom, x, y, fx, fy }`, the point on screen it is about and where that is on the page. */
  #zooming = null;
  /** The zoom being drawn, whose point stays where it was on screen. */
  #focus = null;
  #settle = null;

  /** `sheet` is the page, `body` what scrolls it, `feature` the `PagesFeature` that keeps the zoom. */
  constructor(sheet, body, feature) {
    this.sheet = sheet;
    this.body = body;
    this.feature = feature;
    this.#listen();
  }

  /** Zooms by `factor` about the middle of the screen. */
  step(factor) {
    const box = this.body.getBoundingClientRect();
    this.#stretch(this.feature.zoom * factor, box.left + box.width / 2, box.top + box.height / 2);
    this.#draw();
  }

  /** Scrolls the sheet, just sized for a new zoom, to keep the point zoomed about in its place; false when the zoom was not this one's. */
  keepPlace() {
    const focus = this.#focus;
    if (!focus) return false;
    const box = this.sheet.getBoundingClientRect();
    this.body.scrollLeft += box.left + focus.fx * box.width - focus.x;
    this.body.scrollTop += box.top + focus.fy * box.height - focus.y;
    return true;
  }

  stop() {
    clearTimeout(this.#settle);
  }

  /** Shows the page at `zoom` at once, stretched about the point (x, y) on screen, until it is drawn so. */
  #stretch(zoom, x, y) {
    if (this.sheet.hidden) return;
    if (!this.#zooming) {
      const box = this.sheet.getBoundingClientRect();
      this.#zooming = { x, y, fx: (x - box.left) / box.width, fy: (y - box.top) / box.height };
      this.sheet.style.transformOrigin = `${x - box.left}px ${y - box.top}px`;
    }
    this.#zooming.zoom = clampedZoom(zoom);
    this.sheet.style.transform = `scale(${this.#zooming.zoom / this.feature.zoom})`;
  }

  /** Draws the page at the zoom shown stretched; the view sizes the sheet before this returns. */
  #draw() {
    const zooming = this.#zooming;
    if (!zooming) return;
    this.#zooming = null;
    this.sheet.style.transform = "";
    this.#focus = zooming;
    this.feature.zoomTo(zooming.zoom);
    this.#focus = null;
  }

  #listen() {
    let pinch = null;
    const spread = (touches) => Math.hypot(touches[0].clientX - touches[1].clientX, touches[0].clientY - touches[1].clientY);
    this.sheet.addEventListener("touchstart", (event) => {
      if (event.touches.length !== 2) return;
      const [a, b] = event.touches;
      pinch = { spread: spread(event.touches), zoom: this.feature.zoom };
      this.#stretch(this.feature.zoom, (a.clientX + b.clientX) / 2, (a.clientY + b.clientY) / 2);
    }, { passive: true });
    this.sheet.addEventListener("touchmove", (event) => {
      if (!pinch || event.touches.length !== 2) return;
      event.preventDefault();
      this.#stretch(pinch.zoom * (spread(event.touches) / pinch.spread));
    }, { passive: false });
    for (const name of ["touchend", "touchcancel"]) {
      this.sheet.addEventListener(name, (event) => {
        if (!pinch || event.touches.length >= 2) return;
        pinch = null;
        this.#draw();
      });
    }

    this.body.addEventListener("wheel", (event) => {
      if (!event.ctrlKey || this.sheet.hidden) return;
      event.preventDefault();
      // A mouse wheel's notch is a hundred or so; a trackpad's pinch moves a few at a time.
      const delta = Math.min(Math.max(event.deltaY, -100), 100);
      this.#stretch((this.#zooming?.zoom ?? this.feature.zoom) * Math.exp(-delta / 200), event.clientX, event.clientY);
      clearTimeout(this.#settle);
      this.#settle = setTimeout(() => this.#draw(), 200);
    }, { passive: false });
  }
}
