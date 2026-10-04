/*
 * GNARL - host_keys.h
 *
 * Copyright (C) 2026 Gnarl Audio
 *
 * GNARL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#pragma once

// Posts a key press or release to the host window that holds the plugin's
// window (`plugin_window`: the editor peer's native handle). `key_code` is a
// Windows virtual-key code, which a browser's KeyboardEvent.keyCode is on
// Windows. Returns false where it cannot (no parent, or not Windows).
bool forwardKeyToHost(void* plugin_window, int key_code, bool down, bool alt);
