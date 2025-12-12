CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -I include/ -I build/protocol/
LIBS = -lwayland-client -lcairo -lcurl -lpthread
TARGET = busto-browser

# Source files
SOURCES = src/browser.c src/window.c src/renderer.c src/http.c src/html.c src/input.c src/utils.c build/protocol/xdg-shell-client-protocol.c
OBJECTS = $(SOURCES:.c=.o)

# Header files for dependency tracking
HEADERS = include/busto/window.h include/busto/renderer.h include/busto/http.h include/busto/html.h include/busto/input.h include/busto/utils.h

.PHONY: all clean install-deps protocols test

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LIBS)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

# Generate protocol files (run this if you update wayland protocols)
protocols:
	wayland-scanner client-header /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml build/protocol/xdg-shell-client-protocol.h
	wayland-scanner private-code /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml build/protocol/xdg-shell-client-protocol.c

install-deps:
	@echo "Installing dependencies..."
	@echo "On Ubuntu/Debian:"
	@echo "  sudo apt-get install libwayland-dev libwayland-cursor-dev libcairo2-dev"
	@echo "On Fedora:"
	@echo "  sudo dnf install wayland-devel wayland-protocols-devel cairo-devel"
	@echo "On Arch:"
	@echo "  sudo pacman -S wayland cairo"
	@echo "For HTTP support:"
	@echo "  sudo apt-get install libcurl4-openssl-dev"
	@echo "For HTML parsing:"
	@echo "  sudo apt-get install libgumbo-dev"

run: $(TARGET)
	./run.sh

direct-run: $(TARGET)
	./$(TARGET)

test:
	@echo "No tests implemented yet"

help:
	@echo "Available targets:"
	@echo "  all         - Build the browser"
	@echo "  clean       - Remove compiled files"
	@echo "  protocols   - Regenerate Wayland protocol files"
	@echo "  install-deps- Show dependency installation commands"
	@echo "  run         - Build and run with helper script"
	@echo "  direct-run  - Build and run directly (no helper)"
	@echo "  test        - Run tests (when implemented)"
	@echo "  help        - Show this help message"