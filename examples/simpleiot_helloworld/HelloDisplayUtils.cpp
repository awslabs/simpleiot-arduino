/*
 * © 2022 Amazon Web Services, Inc. or its affiliates. All Rights Reserved.
 * 
 * These are utility routines for showing the display. We're keeping them separate
 * to let the hello world focus on functionality instead of display.
 * NOTE: If using the Arduino IDE, make sure the Board setting is for ESP32 -> M5Stack-Core2. 
*/

#include "HelloDisplayUtils.hpp"

extern const unsigned char HelloWorldM5_Connecting[];
extern const unsigned char HelloWorldM5[];

PlanetColor defaultColor;
PlanetColor currentColor;

int planet_offset_x = 190;
int planet_offset_y = 105;

TFT_eSprite connectingScreen = TFT_eSprite(&M5.Lcd);
TFT_eSprite helloScreen = TFT_eSprite(&M5.Lcd);


// This is what is shown when the device first boots up.
//
void showStartupScreen()
{
  defaultColor = PLANET_ORIGINAL;
  currentColor = defaultColor;

  // Per: https://community.m5stack.com/topic/2676/m5-lcd-setbrightness-not-working/2
  // On Core2, the setBrightness is capped. To get it full brightness you have to set 
  // the display voltage level from 2500 to 3300
  
  //  M5.Lcd.setBrightness(128); // doesn't do anything
  //
  M5.Axp.SetLcdVoltage(3300); // 

  connectingScreen.createSprite(320, 240);
  helloScreen.createSprite(320, 240);

  connectingScreen.fillSprite(BLACK);
  connectingScreen.setTextColor(TFT_WHITE, TFT_BLACK);
  connectingScreen.setFreeFont(&Poppins_Regular20pt7b);
  connectingScreen.drawJpg(HelloWorldM5_Connecting, 23478); // Get the size from the included file declaration-sizeof doesn't work.
  connectingScreen.pushSprite(0, 0);
}

// Once a connection has been established, this is what we show.
//
void showHelloWorldBackground()
{
  helloScreen.fillSprite(BLACK);
  helloScreen.setTextColor(TFT_WHITE, TFT_BLACK);
  helloScreen.setFreeFont(&Poppins_Regular20pt7b);
  helloScreen.drawJpg(HelloWorldM5_Base, 14277);
  updateDisplay(0);
}

// When we get a remote command to change the color, this changes it so on screen refresh
// we always use the last text color. 

void setCurrentColor(PlanetColor color)
{
  currentColor = color;
}


// M5.Lcd doesn't have a getTextBounds method, so we have to guess how wide and high
// to erase the background before drawing again.
// This value is roughly the width and height of each field. We have to offset
// backward to make sure it draws in the right place
//
int xOffset = -2;
int yOffset = -35;
int eraseBlockHeight = 50;

void eprint(char* txt, int x, int y, int width)
{
//  M5.Lcd.getTextBounds(string, x, y, &x1, &y1, &w, &h);
  helloScreen.fillRect(x+xOffset, y+yOffset, width, eraseBlockHeight, TFT_BLACK);
  helloScreen.setCursor(x, y);
  helloScreen.printf(txt);
}

// Update the display, given current settings (button number, last set color, etc.)
// If we want to show anything else that can be updated. This is where it could go.
//
void updateDisplay(int currentButton)
{
  char buttonString[20];

  if (currentButton > 0) {
    sprintf(buttonString, "Button %d", currentButton);
    eprint(buttonString, 20, 220, 180);
  }

  switch(currentColor) {
    case PLANET_ORIGINAL:
        helloScreen.drawJpg(Planet_Original, 6923, planet_offset_x, planet_offset_y);
        break;
    case PLANET_RED: 
        helloScreen.drawJpg(Planet_Red, 7399, planet_offset_x, planet_offset_y);
        break;
    case PLANET_GREEN: 
        helloScreen.drawJpg(Planet_Green, 7323, planet_offset_x, planet_offset_y);
        break;
    case PLANET_BLUE: 
        helloScreen.drawJpg(Planet_Blue, 7289, planet_offset_x, planet_offset_y);
        break;

  }

// In case we want to show the current firmware version.
//
//  helloScreen.setTextSize(3);
//  helloScreen.setCursor(115, 140);
//  helloScreen.printf(IOT_FW_VERSION);
//  helloScreen.setTextSize(2);
//  helloScreen.setCursor(65, 170);
//  helloScreen.printf("firmware version");

  helloScreen.pushSprite(0, 0);
}
