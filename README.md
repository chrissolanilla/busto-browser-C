# Wayland Text Rendering App

A simple C application that opens a Wayland window and renders text using Cairo.

## Dependencies

Install the required development libraries:

**Ubuntu/Debian:**
```bash
sudo apt-get install libwayland-dev libwayland-cursor-dev libcairo2-dev
```

**Fedora:**
```bash
sudo dnf install wayland-devel wayland-protocols-devel cairo-devel
```

**Arch:**
```bash
sudo pacman -S wayland cairo
```

## Building

```bash
make
```

## Running

```bash
make run
# or
./wayland_text_app
```

The application will create a 400x300 window displaying text rendered using Cairo on Wayland.

## Features

- Opens a Wayland window
- Renders text using Cairo
- Clean window title
- Proper resource cleanup

## Code Structure

- `wayland_text_app.c` - Main application source
- `Makefile` - Build configuration

The application uses:
- Wayland client protocols for window management
- Cairo for 2D graphics and text rendering
- Standard C libraries