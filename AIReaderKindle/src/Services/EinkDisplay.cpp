#include "EinkDisplay.hpp"

#include <cstdio>

#ifdef __linux__

#include <fcntl.h>
#include <linux/fb.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstring>

namespace EinkDisplay {

namespace {

// The update request of the MediaTek Kindle kernel
// (drivers/misc/mediatek/hwtcon_v2), as FBInk and KOReader carry it. The
// swipe fields are what animate; the rest is what every update sends.
struct Rect {
    uint32_t top, left, width, height;
};
struct AltBuffer {
    uint32_t physicalAddress, width, height;
    Rect region;
};
struct Swipe {
    uint32_t direction, steps;
};
struct Update {
    Rect region;
    uint32_t waveform, mode, marker;
    int temperature;
    unsigned flags;
    int dither, quantBits;
    AltBuffer altBuffer;
    Swipe swipe;
    uint32_t histBwWaveform, histGrayWaveform, tsPxp, tsEpdc;
};
static_assert(sizeof(Update) == 96, "the ioctl number encodes the size the kernel expects");

constexpr unsigned long sendUpdate = _IOW('F', 0x2E, Update);
constexpr uint32_t waveformAuto = 257, waveformDU = 1, waveformGC16 = 2;
constexpr uint32_t updatePartial = 0;
constexpr int temperatureAmbient = 0x1000;
constexpr unsigned flagSwipe = 0x10000;
constexpr uint32_t swipeLeft = 2, swipeRight = 3;
/// What the Kindle's reader uses; the driver takes 1 to 60.
constexpr uint32_t swipeSteps = 12;

/// The framebuffer, mapped once. Only an 8-bit grayscale one is used: that
/// is every e-ink Kindle but the colour ones, and X draws it byte for byte.
struct Framebuffer {
    int fd = -1;
    uint8_t* memory = nullptr;
    size_t length = 0;
    fb_var_screeninfo var{};
    fb_fix_screeninfo fix{};
    uint32_t marker = 0;
    bool usable = false;

    Framebuffer() {
        fd = ::open("/dev/fb0", O_RDWR);
        if (fd < 0) return;
        if (ioctl(fd, FBIOGET_VSCREENINFO, &var) < 0 || ioctl(fd, FBIOGET_FSCREENINFO, &fix) < 0) return;
        if (var.bits_per_pixel != 8) {
            std::fprintf(stderr, "eink: %u-bit framebuffer, page turns not animated\n", var.bits_per_pixel);
            return;
        }
        length = fix.smem_len;
        void* mapped = mmap(nullptr, length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if (mapped == MAP_FAILED) return;
        memory = static_cast<uint8_t*>(mapped);
        usable = true;
    }

    ~Framebuffer() {
        if (memory) munmap(memory, length);
        if (fd >= 0) ::close(fd);
    }
};

Framebuffer& framebuffer() {
    static Framebuffer instance;
    return instance;
}

}  // namespace

bool slide(cairo_surface_t* page, int x, int y, Slide direction) {
    Framebuffer& fb = framebuffer();
    if (!fb.usable || cairo_image_surface_get_format(page) != CAIRO_FORMAT_RGB24) return false;
    int width = cairo_image_surface_get_width(page);
    int height = cairo_image_surface_get_height(page);
    // Inside the screen, and wide enough for the driver, which crashes on a
    // region narrower than its steps.
    if (x < 0 || y < 0 || width < static_cast<int>(swipeSteps) || height < 1
        || x + width > static_cast<int>(fb.var.xres) || y + height > static_cast<int>(fb.var.yres)) return false;

    // The page, as gray, straight into the framebuffer.
    const uint8_t* source = cairo_image_surface_get_data(page);
    int stride = cairo_image_surface_get_stride(page);
    for (int row = 0; row < height; ++row) {
        const uint32_t* pixels = reinterpret_cast<const uint32_t*>(source + row * stride);
        uint8_t* line = fb.memory + (fb.var.yoffset + y + row) * fb.fix.line_length + fb.var.xoffset + x;
        for (int column = 0; column < width; ++column) {
            uint32_t pixel = pixels[column];
            uint32_t r = (pixel >> 16) & 0xff, g = (pixel >> 8) & 0xff, b = pixel & 0xff;
            line[column] = static_cast<uint8_t>((r * 77 + g * 151 + b * 28) >> 8);
        }
    }

    Update update{};
    update.region = {static_cast<uint32_t>(y), static_cast<uint32_t>(x), static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
    // AUTO and partial: the kernel picks the waveforms a swipe wants.
    update.waveform = waveformAuto;
    update.mode = updatePartial;
    update.marker = ++fb.marker;
    update.temperature = temperatureAmbient;
    update.flags = flagSwipe;
    update.swipe = {direction == Slide::Left ? swipeLeft : swipeRight, swipeSteps};
    update.histBwWaveform = waveformDU;
    update.histGrayWaveform = waveformGC16;
    if (ioctl(fb.fd, sendUpdate, &update) < 0) {
        // Not a MediaTek Kindle: X shows the page the ordinary way from here on.
        std::fprintf(stderr, "eink: the driver takes no swipe update (%s), page turns not animated\n", std::strerror(errno));
        fb.usable = false;
        return false;
    }
    return true;
}

}  // namespace EinkDisplay

#else

namespace EinkDisplay {

bool slide(cairo_surface_t*, int, int, Slide) {
    return false;
}

}  // namespace EinkDisplay

#endif
