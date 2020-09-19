
#pragma once

#include <cstdlib>
#include "../core/Shape.h"

namespace msdfgen {

typedef unsigned unicode_t;

class FreetypeHandle;
class FontHandle;

/// Global metrics of a typeface (in font units).
struct FontMetrics {
    /// The size of one EM.
    double emSize;
    /// The vertical position of the ascender and descender relative to the baseline.
    double ascenderY, descenderY;
    /// The vertical difference between consecutive baselines.
    double lineHeight;
    /// The vertical position and thickness of the underline.
    double underlineY, underlineThickness;
};

/// Initializes the FreeType library.
FreetypeHandle * initializeFreetype();
/// Deinitializes the FreeType library.
void deinitializeFreetype(FreetypeHandle *library);
/// Loads a font from memory and returns its handle.
FontHandle * loadFont(FreetypeHandle *library, const void *data, long size, int index);
/// Unloads a font.
void destroyFont(FontHandle *font);
/// Outputs the metrics of a font.
bool getFontMetrics(FontMetrics &metrics, FontHandle *font);
/// Outputs the width of the space and tab characters.
bool getFontWhitespaceWidth(double &spaceAdvance, double &tabAdvance, FontHandle *font);
/// Loads the geometry of a glyph from a font.
bool loadGlyph(Shape &output, FontHandle *font, unsigned int index, double *advance = NULL);
/// Outputs the kerning distance adjustment between two specific glyphs.
bool getKerning(double &output, FontHandle *font, unicode_t unicode1, unicode_t unicode2);

unsigned int getCharIndex(FontHandle *font, int chr);
int getNumFaces(FontHandle *font);
const char *getStyleName(FontHandle *font);
const char *getFamilyName(FontHandle *font);

}
