/**
 * 8x8 Bitmap Font Implementation
 */

#include <stdint.h>
#include <string.h>
#include "font8x8.h"

// Draw a character from the bitmap font to the buffer
void draw_char(uint32_t *pixels, int buf_width, int x, int y, char c, uint32_t color) {
    // Only handle printable ASCII characters
    if (c < 32 || c > 127) {
        return;
    }
    
    // Get the font data for this character
    const unsigned char *char_data = font8x8[c - 32];
    
    // Draw each pixel of the character
    for (int dy = 0; dy < 8; dy++) {
        for (int dx = 0; dx < 8; dx++) {
            // Check if this pixel should be drawn (1 in the bitmap)
            if (char_data[dy] & (1 << dx)) {
                // Calculate buffer position (if in bounds)
                int pos_x = x + dx;
                int pos_y = y + dy;
                if (pos_x >= 0 && pos_x < buf_width) {
                    pixels[pos_y * buf_width + pos_x] = color;
                }
            }
        }
    }
}

// Draw a string of text to the buffer
void draw_text(uint32_t *pixels, int buf_width, int x, int y, const char *text, uint32_t color) {
    int text_x = x;
    
    for (int i = 0; text[i] != '\0'; i++) {
        draw_char(pixels, buf_width, text_x, y, text[i], color);
        text_x += 8; // Move to the next character position (8 pixels)
    }
}
