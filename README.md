# Busto Browser

My own custom web browser and text editor written in C

## How to Use This Browser:

1. **Run the browser:**
   ```bash
   ./busto-browser
   ```

## Building

### Linux (Wayland)

if you got arch
```bash
sudo pacman -S wayland wayland-protocols cairo libcurl pkgconf
make

./busto-browser
```
gentoo
```
sudo emerge --ask dev-libs/wayland dev-libs/wayland-protocols x11-libs/cairo net-misc/curl pkgconf
```

Debian/Ubuntu:
```bash
sudo apt-get install libwayland-dev libcairo2-dev libcurl4-openssl-dev pkg-config
make
./busto-browser
```

### macOS

```bash
brew install cairo curl pkg-config
make

./busto-browser
```

The macOS backend uses native Cocoa AppKit for the window and input.

### Dependencies

uses wayland for linux or appkit for macos, cairo for rendering, libcurl for
networking, and custom HTML parsing
