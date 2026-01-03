# Busto Browser



## How to Use This Browser: DONT

1. **Run the browser:**
   ```bash
   ./busto-browser
   ```

## Building

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

### Dependencies


uses wayland, cairo, libcurl, and custom HTML parsing

cooked

COOKED
