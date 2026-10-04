/*
 * GNARL - host_keys.cpp
 *
 * Copyright (C) 2026 Gnarl Audio
 *
 * GNARL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

// A key the web panel does not use, handed to the host's window - FL Studio's
// F5 (playlist), F6 (channel rack), Space (play). Vital's own editor does
// this for every key it ignores (JUCE posts it to the parent window); the
// web view keeps the keys for itself, so the panel sends them here instead.
// In a file of its own: <windows.h> beside JuceHeader.h clashes on names
// (Rectangle among them).

#include "host_keys.h"

#if defined(_WIN32)

#ifndef NOMINMAX
  #define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

bool forwardKeyToHost(void* plugin_window, int key_code, bool down, bool alt) {
  HWND window = static_cast<HWND>(plugin_window);
  HWND parent = window != nullptr ? GetParent(window) : nullptr;
  if (parent == nullptr || key_code <= 0 || key_code > 0xfe)
    return false;

  // The lParam a real key press carries: repeat count 1, the scan code, and
  // for a release the previous-state and transition bits.
  UINT scan_code = MapVirtualKeyW(static_cast<UINT>(key_code), MAPVK_VK_TO_VSC);
  LPARAM l_param = 1 | (static_cast<LPARAM>(scan_code) << 16);
  if (alt)
    l_param |= static_cast<LPARAM>(1) << 29;
  if (!down)
    l_param |= (static_cast<LPARAM>(1) << 30) | (static_cast<LPARAM>(1) << 31);

  UINT message = alt ? (down ? WM_SYSKEYDOWN : WM_SYSKEYUP) : (down ? WM_KEYDOWN : WM_KEYUP);
  return PostMessageW(parent, message, static_cast<WPARAM>(key_code), l_param) != 0;
}

#else

// macOS and Linux: not yet. Hosts there have not been tested with the panel.
bool forwardKeyToHost(void*, int, bool, bool) {
  return false;
}

#endif
