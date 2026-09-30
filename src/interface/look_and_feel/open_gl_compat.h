/* Copyright 2026 Gnarl Audio
 *
 * GNARL is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version. See LICENSE.
 */

// One source for two JUCE versions while the build moves from JUCE 6.0.5
// (the Projucer projects) to JUCE 8 (CMakeLists.txt) -
// docs/design/phase2-03-juce8.md.
//
// JUCE 6.1 moved every OpenGL function and constant into namespace juce::gl
// and dropped most of OpenGLExtensionFunctions. Upstream calls those through
// `context.extensions.glX(...)`; written as
//
//     GNARL_GL_EXT(open_gl.context) glBindBuffer(...)
//
// the same line is `open_gl.context.extensions.glBindBuffer(...)` on 6.0.5 and
// the free function juce::gl::glBindBuffer(...) on 6.1 and later. Remove this
// file, and the macro, when the Projucer projects are gone.

#pragma once

#include "JuceHeader.h"

#if JUCE_MAJOR_VERSION > 6 || (JUCE_MAJOR_VERSION == 6 && JUCE_MINOR_VERSION >= 1)
  using namespace juce::gl;
  #define GNARL_GL_VIA(extensions)
#else
  #define GNARL_GL_VIA(extensions) extensions.
#endif

// `GNARL_GL_VIA(ext) glX(...)` for an OpenGLExtensionFunctions object itself.
#define GNARL_GL_EXT(context) GNARL_GL_VIA(context.extensions)
