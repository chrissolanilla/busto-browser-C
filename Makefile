TARGET = busto-browser

UNAME_S := $(shell uname -s)

COMMON_SRC = src/browser.c src/renderer.c src/http.c src/html.c src/input.c src/utils.c src/key_repeat.c
COMMON_HEADERS = include/busto/window.h include/busto/renderer.h include/busto/http.h include/busto/html.h include/busto/input.h include/busto/utils.h include/busto/key_repeat.h

PKG_CONFIG ?= pkg-config

CAIRO_INCDIR := $(shell $(PKG_CONFIG) --variable=includedir cairo 2>/dev/null)

ifeq ($(UNAME_S),Darwin)
    CC = clang
    CFLAGS = -Wall -Wextra -std=c11 -I include/ -I$(CAIRO_INCDIR) $(shell $(PKG_CONFIG) --cflags cairo libcurl 2>/dev/null) -fobjc-arc
    PLATFORM_SRC = platform/macos/window_macos.m
    LIBS = -framework Cocoa -framework CoreGraphics $(shell $(PKG_CONFIG) --libs cairo libcurl 2>/dev/null)
else
    CC ?= gcc
    PROTO_DIR = build/protocol
    PROTO_H   = $(PROTO_DIR)/xdg-shell-client-protocol.h
    PROTO_C   = $(PROTO_DIR)/xdg-shell-client-protocol.c
    PROTO_XML = /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml
    CFLAGS = -Wall -Wextra -std=c99 -I include/ -I $(PROTO_DIR) -I$(CAIRO_INCDIR) $(shell $(PKG_CONFIG) --cflags cairo libcurl wayland-client 2>/dev/null)
    PLATFORM_SRC = platform/wayland/window_wayland.c $(PROTO_C)
    LIBS = $(shell $(PKG_CONFIG) --libs cairo libcurl wayland-client 2>/dev/null) -lpthread
endif

SOURCES = $(COMMON_SRC) $(PLATFORM_SRC)
OBJECTS = $(SOURCES:.c=.o)
OBJECTS := $(OBJECTS:.m=.o)

.PHONY: all clean install-deps protocols run direct-run test help

ifeq ($(UNAME_S),Darwin)
all: $(TARGET)
else
all: $(PROTO_H) $(TARGET)
endif

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LIBS)

%.o: %.c $(COMMON_HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.m $(COMMON_HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

ifeq ($(UNAME_S),Darwin)
.SILENT: protocols
protocols:
	@echo "wayland protocol generation is linux-only"
else
protocols: $(PROTO_H) $(PROTO_C)

$(PROTO_H) $(PROTO_C):
	mkdir -p $(PROTO_DIR)
	wayland-scanner client-header $(PROTO_XML) $(PROTO_H)
	wayland-scanner private-code  $(PROTO_XML) $(PROTO_C)
endif

install-deps:
	@echo "Installing dependencies..."
	@echo ""
	@echo "On Ubuntu/Debian:"
	@echo "  sudo apt-get install libwayland-dev libcairo2-dev libcurl4-openssl-dev pkg-config"
	@echo ""
	@echo "On Fedora:"
	@echo "  sudo dnf install wayland-devel wayland-protocols-devel cairo-devel libcurl-devel pkgconf"
	@echo ""
	@echo "On Arch:"
	@echo "  sudo pacman -S wayland wayland-protocols cairo libcurl pkgconf"
	@echo ""
	@echo "On macOS (Homebrew):"
	@echo "  brew install cairo curl pkg-config"
	@echo ""
	@echo "HTML parsing is built-in (no libgumbo needed)"

run: $(TARGET)
	./$(TARGET)

direct-run: $(TARGET)
	./$(TARGET)

test:
	@echo "No tests implemented yet"

help:
	@echo "Available targets:"
	@echo "  all         - Build the browser"
	@echo "  clean       - Remove compiled files"
	@echo "  protocols   - Regenerate Wayland protocol files (Linux only)"
	@echo "  install-deps- Show dependency installation commands"
	@echo "  run         - Build and run the browser"
	@echo "  direct-run  - Build and run directly (no helper)"
	@echo "  test        - Run tests (when implemented)"
	@echo "  help        - Show this help message"