// Adopted by Bodmer to support TFT_eSPI library.

// Font structures for newer Adafruit_GFX (1.1 and later).
// Example fonts are included in 'Fonts' directory.
// To use a font in your Arduino sketch, #include the corresponding .h
// file and pass address of GFXfont struct to setFont().  Pass NULL to
// revert to 'classic' fixed-space bitmap font.

#ifndef _GFXFONT_H_
#define _GFXFONT_H_

#ifdef LOAD_GFXFF

typedef struct { // Data stored PER GLYPH
	uint32_t bitmapOffset;     // Pointer into GFXfont->bitmap
	uint8_t  width, height;    // Bitmap dimensions in pixels
	uint8_t  xAdvance;         // Distance to advance cursor (x axis)
	int8_t   xOffset, yOffset; // Dist from cursor pos to UL corner
} GFXglyph;

typedef struct { // Data stored for FONT AS A WHOLE:
	uint8_t  *bitmap;      // Glyph bitmaps, concatenated
	GFXglyph *glyph;       // Glyph array
	uint16_t  first, last; // ASCII extents
	uint8_t   yAdvance;    // Newline distance (y axis)
} GFXfont;

  // Original Adafruit_GFX "Free Fonts"

  #include <Fonts/GFXFF/FreeMono9pt7b.h>  // FF1 or FM9
  #include <Fonts/GFXFF/FreeMono12pt7b.h> // FF2 or FM12
  #include <Fonts/GFXFF/FreeMono18pt7b.h> // FF3 or FM18
  #include <Fonts/GFXFF/FreeMono24pt7b.h> // FF4 or FM24

  
  #include <Fonts/GFXFF/FreeMonoBold9pt7b.h>  // FF9  or FMB9
  #include <Fonts/GFXFF/FreeMonoBold12pt7b.h> // FF10 or FMB12
  #include <Fonts/GFXFF/FreeMonoBold18pt7b.h> // FF11 or FMB18
  #include <Fonts/GFXFF/FreeMonoBold24pt7b.h> // FF12 or FMB24
  
  
  // Sans serif fonts
  #include <Fonts/GFXFF/FreeSans9pt7b.h>  // FF17 or FSS9
  #include <Fonts/GFXFF/FreeSans12pt7b.h> // FF18 or FSS12
  #include <Fonts/GFXFF/FreeSans18pt7b.h> // FF19 or FSS18
  #include <Fonts/GFXFF/FreeSans24pt7b.h> // FF20 or FSS24
  
  
  #include <Fonts/GFXFF/FreeSansBold9pt7b.h>  // FF25 or FSSB9
  #include <Fonts/GFXFF/FreeSansBold12pt7b.h> // FF26 or FSSB12
  #include <Fonts/GFXFF/FreeSansBold18pt7b.h> // FF27 or FSSB18
  #include <Fonts/GFXFF/FreeSansBold24pt7b.h> // FF28 or FSSB24
  
  
  // Serif fonts
  #include <Fonts/GFXFF/FreeSerif9pt7b.h>  // FF33 or FS9
  #include <Fonts/GFXFF/FreeSerif12pt7b.h> // FF34 or FS12
  #include <Fonts/GFXFF/FreeSerif18pt7b.h> // FF35 or FS18
  #include <Fonts/GFXFF/FreeSerif24pt7b.h> // FF36 or FS24
  
  
  #include <Fonts/GFXFF/FreeSerifBold9pt7b.h>  // FF41 or FSB9
  #include <Fonts/GFXFF/FreeSerifBold12pt7b.h> // FF42 or FSB12
  #include <Fonts/GFXFF/FreeSerifBold18pt7b.h> // FF43 or FSB18
  #include <Fonts/GFXFF/FreeSerifBold24pt7b.h> // FF44 or FSB24
  

#endif // LOAD_GFXFF

#endif // _GFXFONT_H_
