/*
 * LCD_GFX.c
 *
 * Created: 9/20/2021 6:54:25 PM
 *  Author: You
 */ 

#include "LCD_GFX.h"
#include "ST7735.h"

/******************************************************************************
* Local Functions
******************************************************************************/



/******************************************************************************
* Global Functions
******************************************************************************/

/**************************************************************************//**
* @fn			uint16_t rgb565(uint8_t red, uint8_t green, uint8_t blue)
* @brief		Convert RGB888 value to RGB565 16-bit color data
* @note
*****************************************************************************/
uint16_t rgb565(uint8_t red, uint8_t green, uint8_t blue)
{
	return ((((31*(red+4))/255)<<11) | (((63*(green+2))/255)<<5) | ((31*(blue+4))/255));
}

/**************************************************************************//**
* @fn			void LCD_drawPixel(uint8_t x, uint8_t y, uint16_t color)
* @brief		Draw a single pixel of 16-bit rgb565 color to the x & y coordinate
* @note
*****************************************************************************/
void LCD_drawPixel(uint8_t x, uint8_t y, uint16_t color) {
	LCD_setAddr(x,y,x,y);
	SPI_ControllerTx_16bit(color);
}

/**************************************************************************//**
* @fn			void LCD_drawChar(uint8_t x, uint8_t y, uint16_t character, uint16_t fColor, uint16_t bColor)
* @brief		Draw a character starting at the point with foreground and background colors
* @note
*****************************************************************************/
void LCD_drawChar(uint8_t x, uint8_t y, uint16_t character, uint16_t fColor, uint16_t bColor){
	uint16_t row = character - 0x20;		// Determine row of ASCII table starting at space
	uint8_t scale = 2;
    int i, j, sx, sy;

	if ((LCD_WIDTH - x > 7 * scale) && (LCD_HEIGHT - y > 7 * scale)){
		for (i = 0; i < 5; i++) {
			uint8_t pixels = ASCII[row][i]; // Go through the list of pixels
			for (j = 0; j < 8; j++) {
				uint16_t color = ((pixels >> j) & 1) ? fColor : bColor;
				// Draw each pixel as a block of size `scale x scale`
				for (sx = 0; sx < scale; sx++) {
					for (sy = 0; sy < scale; sy++) {
						LCD_drawPixel(x + i * scale + sx, y + j * scale + sy, color);
					}
				}
			}
		}
	}
}


/******************************************************************************
* LAB 4 TO DO. COMPLETE THE FUNCTIONS BELOW.
* You are free to create and add any additional files, libraries, and/or
*  helper function. All code must be authentically yours.
******************************************************************************/

/**************************************************************************//**
* @fn			void LCD_drawCircle(uint8_t x0, uint8_t y0, uint8_t radius,uint16_t color)
* @brief		Draw a colored circle of set radius at coordinates
* @note
*****************************************************************************/
void LCD_drawCircle(uint8_t x0, uint8_t y0, uint8_t radius,uint16_t color)
{
	    int x;
        int y;
    
    // Iterate over a square region around the circle's center (x0, y0)
    for (x = x0 - radius; x <= x0 + radius; x++) {
        for (y = y0 - radius; y <= y0 + radius; y++) {
            if ((x - x0) * (x - x0) + (y - y0) * (y - y0) <= radius * radius) {
                LCD_drawPixel(x, y, color); // Draw pixel within the circle
            }
        }
    }
}


/**************************************************************************//**
* @fn			void LCD_drawLine(short x0,short y0,short x1,short y1,uint16_t c)
* @brief		Draw a line from and to a point with a color
* @note
*****************************************************************************/
void LCD_drawLine(short x0,short y0,short x1,short y1,uint16_t c)
	{
    short dx = abs(x1 - x0); 
    short dy = abs(y1 - y0);

    short starting_x;
    if (x0 < x1) {
        starting_x = 1;   // Move right if starting point is to the left
    } else {
        starting_x = -1;  // Move left if starting point is to the right
    }

    short starting_y;
    if (y0 < y1) {
        starting_y = 1; 
    } else {
        starting_y = -1; 
    }

    short err = dx - dy;
    while (1) {
        LCD_drawPixel(x0, y0, c);
        if (x0 == x1 && y0 == y1) {
            break;
        }
        short e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += starting_x;
        }
        if (e2 < dx) {
            err += dx;
            y0 += starting_y;
        }
    }
}



/**************************************************************************//**
* @fn			void LCD_drawBlock(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1,uint16_t color)
* @brief		Draw a colored block at coordinates
* @note
*****************************************************************************/
void LCD_drawBlock(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1,uint16_t color)
{
	uint8_t x;
    uint8_t y;
    for (x = x0; x <= x1; x++) {
        for (y = y0; y <= y1; y++) {
            // Draw pixel at (x, y)
            LCD_drawPixel(x, y, color);
        }
    }
}

/**************************************************************************//**
* @fn			void LCD_setScreen(uint16_t color)
* @brief		Draw the entire screen to a color
* @note
*****************************************************************************/
void LCD_setScreen(uint16_t color) 
{
    for (int j = 0; j < 128; j++) {
        for (int i = 0; i < 160; i++) {
            LCD_drawPixel(i, j, color);
        }
    }
}

/**************************************************************************//**
* @fn			void LCD_drawString(uint8_t x, uint8_t y, char* str, uint16_t fg, uint16_t bg)
* @brief		Draw a string starting at the point with foreground and background colors
* @note
*****************************************************************************/
void LCD_drawString(uint8_t x, uint8_t y, char* str, uint16_t fg, uint16_t bg)
{
    uint8_t scale = 2;
	while (*str) {
		if (x + 5 * scale > LCD_WIDTH) {  // Check if string exceeds screen width
			break;
		}
		if (y + 8 * scale > LCD_HEIGHT) { // Check if string exceeds screen height
			break;
		}
		LCD_drawChar(x, y, *str, fg, bg);
		x += 6 * scale;  // Move to the next character position (5 for character width + 1 for spacing)
		str++;
	}
}