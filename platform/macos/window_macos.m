#import <AppKit/AppKit.h>
#import <CoreGraphics/CoreGraphics.h>
#include "../../include/busto/window.h"
#include "../../include/busto/renderer.h"
#include "../../include/busto/key_repeat.h"
#include <cairo/cairo.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *mac_key_string(NSEvent *event);
static const char *mac_keycode_string(struct busto_window *window, int keycode);

struct busto_window {
    NSWindow *ns_window;
    NSView *ns_view;
    id window_delegate;

    void *pixel_data;
    cairo_surface_t *cairo_surface;
    cairo_t *cr;
    int buffer_w;
    int buffer_h;

    int width;
    int height;
    int running;
    int needs_redraw;

    busto_key_handler_t key_handler;
    void *key_handler_data;

    char key_names[256][32];
    struct busto_repeat_state repeat;
};

@interface BustoWindowDelegate : NSObject <NSWindowDelegate>
@property (nonatomic, assign) struct busto_window *busto;
@end

@implementation BustoWindowDelegate

- (BOOL)windowShouldClose:(id)sender {
    return YES;
}

- (void)windowWillClose:(NSNotification *)notification {
    if (self.busto) {
        self.busto->running = 0;
    }
}

@end

@interface BustoView : NSView
@property (nonatomic, assign) struct busto_window *busto;
@end

@implementation BustoView

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (BOOL)acceptsFirstMouse:(NSEvent *)event {
    return YES;
}

- (BOOL)isFlipped {
    return YES;
}

- (BOOL)isOpaque {
    return YES;
}

- (void)setFrameSize:(NSSize)newSize {
    [super setFrameSize:newSize];
    if (self.busto) {
        int w = (int)newSize.width;
        int h = (int)newSize.height;
        if (w > 0 && h > 0 &&
            (w != self.busto->width || h != self.busto->height)) {
            self.busto->width = w;
            self.busto->height = h;
            self.busto->needs_redraw = 1;
        }
    }
}

- (void)drawRect:(NSRect)dirtyRect {
    struct busto_window *w = self.busto;
    CGContextRef ctx = [[NSGraphicsContext currentContext] CGContext];

    if (!w || !w->cr ||
        w->buffer_w != w->width || w->buffer_h != w->height) {
        CGContextSetRGBFillColor(ctx, 0.2, 0.2, 0.2, 1.0);
        CGContextFillRect(ctx, dirtyRect);
        return;
    }

    int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, w->width);
    size_t data_size = (size_t)stride * (size_t)w->height;

    CGDataProviderRef provider =
        CGDataProviderCreateWithData(NULL, w->pixel_data, data_size, NULL);
    CGColorSpaceRef color_space = CGColorSpaceCreateDeviceRGB();
    CGImageRef image = CGImageCreate(
        (size_t)w->width, (size_t)w->height,
        8, 32, (size_t)stride,
        color_space,
        kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host,
        provider, NULL, false, kCGRenderingIntentDefault);

    if (image) {
		//for some reason on mac the CG context has a flipped Y axis
        //flip the context around the midline
        CGContextSaveGState(ctx);
        CGContextTranslateCTM(ctx, 0, w->height);
        CGContextScaleCTM(ctx, 1, -1);
        CGContextDrawImage(ctx, CGRectMake(0, 0, w->width, w->height), image);
        CGContextRestoreGState(ctx);
        CGImageRelease(image);
    }

    CGColorSpaceRelease(color_space);
    CGDataProviderRelease(provider);
}

- (void)keyDown:(NSEvent *)event {
    [BustoView handleKey:event view:self pressed:1 repeat:[event isARepeat]];
}

- (void)keyUp:(NSEvent *)event {
    [BustoView handleKey:event view:self pressed:0 repeat:NO];
}

+ (void)handleKey:(NSEvent *)event view:(BustoView *)view pressed:(int)pressed repeat:(BOOL)repeat {
    struct busto_window *w = view.busto;
    if (!w){
		return;
	}

    int code = (int)[event keyCode];
    if (code < 0 || code >= 256) {
		return;
	}

    if (pressed) {
        if (repeat) {
			return;
		}

        w->repeat.key_down[code] = 1;
        if (w->repeat.rate > 0) {
            long long t = busto_now_ms();
            w->repeat.key_next_repeat_ms[code] = t + w->repeat.delay;
        } else {
            w->repeat.key_next_repeat_ms[code] = 0;
        }

        const char *key_str = mac_key_string(event);
        if (key_str) {
            snprintf(w->key_names[code], sizeof(w->key_names[code]), "%s", key_str);
            if (w->key_handler) {
                w->key_handler(w, key_str, w->key_handler_data);
            }
        }
    } else {
        w->repeat.key_down[code] = 0;
        w->repeat.key_next_repeat_ms[code] = 0;
        w->key_names[code][0] = '\0';
    }
}

- (void)flagsChanged:(NSEvent *)event {
    return;
}

@end

static const char *mac_key_string(NSEvent *event) {
    static char buf[32];
    NSEventModifierFlags mods = [event modifierFlags];

    switch ([event keyCode]) {
        case 36: return "Return";
        case 76: return "Return";
        case 53: return "Escape";
        case 126: return "Up";
        case 125: return "Down";
        case 123: return "Left";
        case 124: return "Right";
        case 96: return "F5";
        case 122: return "F1";
        case 120: return "F2";
        case 99: return "F3";
        case 118: return "F4";
        case 97: return "F6";
        case 98: return "F7";
        case 100: return "F8";
        case 101: return "F9";
        case 109: return "F10";
        case 103: return "F11";
        case 111: return "F12";
        case 51: return "BackSpace";
        case 115: return "Home";
        case 119: return "End";
        case 48: return "Tab";
        default: break;
    }

    NSString *chars = (mods & NSEventModifierFlagControl)
        ? [event charactersIgnoringModifiers]
        : [event characters];

    if (chars.length == 0) {
		return NULL;
	}

    unichar c = [chars characterAtIndex:0];
    if (mods & NSEventModifierFlagControl) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
            char upper = (c >= 'a' && c <= 'z') ? (char)(c - 'a' + 'A') : (char)c;
            snprintf(buf, sizeof(buf), "Ctrl+%c", upper);
            return buf;
        }
    }

    snprintf(buf, sizeof(buf), "%C", c);
    return buf;
}

static const char *mac_keycode_string(struct busto_window *window, int keycode) {
    if (!window || keycode < 0 || keycode >= 256) {
		return NULL;
	}

    if (window->key_names[keycode][0] == '\0') {
		return NULL;
	}

    return window->key_names[keycode];
}

static void macos_destroy_buffer(struct busto_window *w) {
    if (w->cr) {
        cairo_destroy(w->cr);
        w->cr = NULL;
    }
    if (w->cairo_surface) {
        cairo_surface_destroy(w->cairo_surface);
        w->cairo_surface = NULL;
    }
    if (w->pixel_data) {
        free(w->pixel_data);
        w->pixel_data = NULL;
    }
    w->buffer_w = 0;
    w->buffer_h = 0;
}

static int macos_create_buffer(struct busto_window *w) {
    int stride;
    size_t size;
    void *data;

    macos_destroy_buffer(w);

    stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, w->width);
    if (stride < 0) {
		return 0;
	}

    size = (size_t)stride * (size_t)w->height;

    data = calloc(1, size);
    if (!data) {
		return 0;
	}

    w->pixel_data = data;
    w->cairo_surface = cairo_image_surface_create_for_data(
        data, CAIRO_FORMAT_ARGB32, w->width, w->height, stride);

    if (cairo_surface_status(w->cairo_surface) != CAIRO_STATUS_SUCCESS) {
        macos_destroy_buffer(w);
        return 0;
    }
    w->cr = cairo_create(w->cairo_surface);
    w->buffer_w = w->width;
    w->buffer_h = w->height;
    return 1;
}

static int macos_buffer_matches(struct busto_window *w) {
    return w->cr && w->buffer_w == w->width && w->buffer_h == w->height;
}

struct busto_window *busto_window_create(int width, int height) {
    @autoreleasepool {
        NSApplication *app = [NSApplication sharedApplication];
        struct busto_window *w;
        BustoWindowDelegate *delegate;
        NSWindow *nswindow;
        BustoView *view;
        NSRect frame;

        [app setActivationPolicy:NSApplicationActivationPolicyRegular];

        w = calloc(1, sizeof(struct busto_window));
        if (!w) {
			return NULL;
		}

        w->width = width;
        w->height = height;
        w->running = 1;
        w->repeat.rate = 25;
        w->repeat.delay = 300;

        if (!macos_create_buffer(w)) {
            free(w);
            return NULL;
        }

        delegate = [[BustoWindowDelegate alloc] init];
        delegate.busto = w;

        frame = NSMakeRect(200, 200, (CGFloat)width, (CGFloat)height);
        nswindow = [[NSWindow alloc] initWithContentRect:frame
                                               styleMask:(NSWindowStyleMaskTitled |
                                                         NSWindowStyleMaskClosable |
                                                         NSWindowStyleMaskMiniaturizable |
                                                         NSWindowStyleMaskResizable)
                                                 backing:NSBackingStoreBuffered
                                                   defer:NO];
        nswindow.title = @"Busto Browser";
        nswindow.delegate = delegate;

        view = [[BustoView alloc] initWithFrame:frame];
        view.busto = w;
        nswindow.contentView = view;

        w->ns_window = nswindow;
        w->ns_view = view;
        w->window_delegate = delegate;

        [nswindow makeFirstResponder:view];
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        [app activateIgnoringOtherApps:YES];
#pragma clang diagnostic pop
        [nswindow makeKeyAndOrderFront:nil];

        busto_window_redraw(w);

        return w;
    }
}

void busto_window_destroy(struct busto_window *window) {
    if (!window) {
		return;
	}

    @autoreleasepool {
        window->running = 0;

        window->ns_window.delegate = nil;
        [window->ns_window close];
        window->ns_view = nil;
        window->ns_window = nil;
        window->window_delegate = nil;

        macos_destroy_buffer(window);
    }

    free(window);
}

void busto_window_set_title(struct busto_window *window, const char *title) {
    if (!window || !title) {
		return;
	}

    @autoreleasepool {
        window->ns_window.title = [NSString stringWithUTF8String:title];
    }
}

int busto_window_is_running(struct busto_window *window) {
    return window ? window->running : 0;
}

void busto_window_dispatch(struct busto_window *window) {
    if (!window) {
		return;
	}

    @autoreleasepool {
        NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                            untilDate:nil
                                               inMode:NSDefaultRunLoopMode
                                              dequeue:YES];
        if (event) {
            [NSApp sendEvent:event];
        }
    }
}

void busto_window_poll(struct busto_window *window, int timeout_ms) {
    if (!window) {
		return;
	}

    @autoreleasepool {
        NSTimeInterval wait = (timeout_ms > 0) ? (NSTimeInterval)timeout_ms / 1000.0 : 0.0;
        NSDate *until = [NSDate dateWithTimeIntervalSinceNow:wait];
        NSEvent *event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                            untilDate:until
                                               inMode:NSDefaultRunLoopMode
                                              dequeue:YES];

        while (event) {
            [NSApp sendEvent:event];
            event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                       untilDate:nil
                                          inMode:NSDefaultRunLoopMode
                                         dequeue:YES];
        }
    }
}

void busto_window_redraw(struct busto_window *window) {
    if (!window) {
		return;
	}

    if (!macos_buffer_matches(window)) {
        if (!macos_create_buffer(window)) {
			return;
		}
    }

    if (!window->cr) {
		return;
	}

    busto_renderer_render(window->cr, window->width, window->height);
    cairo_surface_flush(window->cairo_surface);
    [window->ns_view setNeedsDisplay:YES];
    [window->ns_view displayIfNeeded];
}

void busto_window_set_key_handler(struct busto_window *window, busto_key_handler_t handler, void *data) {
    if (window) {
        window->key_handler = handler;
        window->key_handler_data = data;
    }
}

void busto_window_request_redraw(struct busto_window *window) {
    if (window) {
		window->needs_redraw =1;
	}
}

int busto_window_needs_redraw(struct busto_window *window) {
    if (!window) {
		return 0;
	}

    int needs = window->needs_redraw;
    window->needs_redraw = 0;
    return needs;
}

void busto_window_update_repeats(struct busto_window *window) {
    if (!window) {
		return;
	}

    busto_repeat_update(
        &window->repeat,
        window->key_handler,
        window->key_handler_data,
        window,
        mac_keycode_string
    );
}
