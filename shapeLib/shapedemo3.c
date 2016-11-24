#include <libTimer.h>
#include "lcdutils.h"
#include "lcddraw.h"
#include "shape.h"

// like abRectCheck, but excludes a triangle
int 
abSlicedRectCheck(const AbRect *rect, const Vec2 *centerPos, const Vec2 *pixel)
{
  Vec2 relPos;
  vec2Sub(&relPos, pixel, centerPos); /* vector from center to pixel */

  /* reject pixels in slice */
  if (relPos.axes[0] >= 0 && relPos.axes[0]/2 < relPos.axes[1]) 
    return 0;
  else
    return abRectCheck(rect, centerPos, pixel);
}

AbRect rect10 = {abRectGetBounds, abSlicedRectCheck, 10,10};;


Region fence = {{10,30}, {SHORT_EDGE_PIXELS-10, LONG_EDGE_PIXELS-10}};


#define numLayers 2
Layer layer1 = {
  (AbShape *)&rect10,
  {screenWidth/2, screenHeight/2}, /* position */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_RED,
  0,
};
Layer layer0 = {
  (AbShape *)&rect10,
  {(screenWidth/2)+15, (screenHeight/2)+10}, /* position */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_ORANGE,
  &layer1,
};


    

u_int bgColor = COLOR_WHITE;

main()
{
  configureClocks();
  lcd_init();
  shapeInit();
  Vec2 rectPos = screenCenter, circlePos = {30,screenHeight - 30};

  /*
    While loop that shows how to change the background rapidly 
    between colors. layerDraw uses bgColor's targeted color
    for the background
   */
  /* int i = 0; */
  /* while(i++ < 10){ */
  /*   clearScreen(COLOR_BLUE); */
  /*   clearScreen(COLOR_RED); */
  /*   clearScreen(COLOR_GREEN); */
  /* } */
 
  drawString5x7(20,20, "hello", COLOR_GREEN, COLOR_RED);
  //shapeInit();

  /*
    While loop that shows how bgColor sets the background
    for the layerDraw function
   */
  int i = 0;
  while(i++ < 10){
    bgColor = COLOR_WHITE;
    layerDraw(&layer0);
    bgColor = COLOR_BLACK;
    layerDraw(&layer0);
    bgColor = COLOR_GREEN;
    layerDraw(&layer0);
  }
   
  
  layerDraw(&layer0);
  
}


