#!/bin/bash

echo "=========================================="
echo "       BUSTO BROWSER - TERMINAL MODE      "
echo "=========================================="
echo ""
echo "This browser uses terminal input for control."
echo "The browser window will open, but you type commands HERE."
echo ""
echo "CONTROLS (type these in this terminal):"
echo "  l          - Focus URL bar for input"
echo "  q          - Quit browser"
echo "  j          - Scroll down"
echo "  k          - Scroll up"
echo "  Enter      - Load URL (when URL bar is focused)"
echo "  Backspace  - Delete character (when URL bar is focused)"
echo ""
echo "To test, type: l"
echo "Then type: https://example.com"
echo "Then press Enter"
echo ""
echo "Starting browser..."
echo "=========================================="

# Make stdin work properly
stty -icanon -echo

# Run the browser
./busto-browser

# Restore terminal settings
stty icanon echo

echo ""
echo "Browser closed."