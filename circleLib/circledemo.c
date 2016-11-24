#include <libTimer.h>
#include <lcdutils.h>
#include <lcddraw.h>
#include "abCircle.h"

AbRect rect10 = {abRectGetBounds, abRectCheck, {15,15}};; /**< 10x10 rectangle */
AbRect rect15 = {abRectGetBounds, abRectCheck, {30,15}};; /**< 30x15 rectangle */


u_int bgColor = COLOR_BLUE;

/*
  Layer 0 in this example has higher priority. Then layer 1 and so on.
  We create a layer 2 rectangle and place it under construct for layer 1.
  Analogous to what they did with layer 1 under layer0.
 */

Layer layer2 = {		/**< Layer with a red square */
  (AbShape *)&rect15,
  {(screenWidth/2)-15, (screenHeight/2)-10}, /**< center */
  {0,0}, {0,0},				    /* next & last pos */
  COLOR_GREEN,
  0
};

Layer layer1 = {		/**< Layer with a red square */
  (AbShape *)&rect10,
  {screenWidth/2, screenHeight/2}, /**< center */
  {0,0}, {0,0},				    /* next & last pos */
  COLOR_RED,
  &layer2,
};

Layer layer0 = {		/**< Layer with an orange circle */
  (AbShape *)&circle15,
  {(screenWidth/2)+10, (screenHeight/2)+5}, /**< bit below & right of center */
  {0,0}, {0,0},				    /* next & last pos */
  COLOR_ORANGE,
  &layer1,
};

main()
{
  configureClocks();
  lcd_init();

  /*
    BUG: the screen background draws red, writes hello, but it gets fully
         repainted when the layers are drawn. Why!??
   */
  clearScreen(COLOR_RED);
  drawString5x7(20,20, "hello", COLOR_GREEN, COLOR_RED);


  layerDraw(&layer0);


}
