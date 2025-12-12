CC = gcc
CFLAGS = -Wall -Wextra -std=c99
LIBS = -lwayland-client -lcairo
TARGET = wayland_text_app

# Source files
SOURCES = wayland_text_working.c xdg-shell-client-protocol.c
OBJECTS = $(SOURCES:.c=.o)

.PHONY: all clean install-deps

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

# Generate protocol files (run this if you update wayland protocols)
protocols:
	wayland-scanner client-header /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml xdg-shell-client-protocol.h
	wayland-scanner private-code /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml xdg-shell-client-protocol.c

install-deps:
	@echo "Installing dependencies..."
	@echo "On Ubuntu/Debian:"
	@echo "  sudo apt-get install libwayland-dev libwayland-cursor-dev libcairo2-dev"
	@echo "On Fedora:"
	@echo "  sudo dnf install wayland-devel wayland-protocols-devel cairo-devel"
	@echo "On Arch:"
	@echo "  sudo pacman -S wayland cairo"

run: $(TARGET)
	./$(TARGET)

help:
	@echo "Available targets:"
	@echo "  all         - Build the application"
	@echo "  clean       - Remove compiled files"
	@echo "  install-deps- Show dependency installation commands"
	@echo "  run         - Build and run the application"
	@echo "  help        - Show this help message"