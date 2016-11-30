#include <msp430.h>
#include <libTimer.h>
#include <lcdutils.h>
#include <lcddraw.h>
#include <shape.h>
#include <abCircle.h>
#include "music.h"
#include "buzzer.h"
#include "p2switches.h"

#define GREEN_LED BIT6

short score[2]; // score[0] for player 1, and score[1] for player 2
u_int reset = 0;

u_int bgColor = COLOR_YELLOW;     /**< The background color */
int redrawScreen = 1;           /**< Boolean for whether screen needs to be redrawn */

Region fieldFence;		/**< fence around playing field  */
Region fencePaddle1;
Region fencePaddle2;


AbRect paddle = {abRectGetBounds, abRectCheck, {3,15}};   /**< 3x15 paddles*/

AbRectOutline fieldOutline = {	                      /* playing field */
  abRectOutlineGetBounds, abRectOutlineCheck,   
  {screenWidth/2 - 5, screenHeight/2 - 11}
};

/* playing field as a layer */

Layer fieldLayer = {		
  (AbShape *) &fieldOutline,
  {screenWidth/2, screenHeight/2},/**< center */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_BLACK,
  0,
};

/* Ball moving */

Layer layer2 = {		
  (AbShape *)&circle4,
  {(screenWidth/2)+10, (screenHeight/2)+5}, /**< bit below & right of center */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_PURPLE,
  &fieldLayer,
};

/* paddle player 2 */

Layer layer1 = {	       
  (AbShape *)&paddle,
  {118, screenHeight/2}, /**< center */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_BLACK,
  &layer2,
};

/* paddle player 1 */

Layer layer0 = {		
  (AbShape *)&paddle,
  {10, screenHeight/2}, /**< bit below & right of center */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_BLACK,
  &layer1,
};

typedef struct MovLayer_s {
  Layer *layer;
  Vec2 velocity;
  struct MovLayer_s *next;
} MovLayer;

MovLayer mlball = { &layer2, {2,2}, 0};
MovLayer ml1 = { &layer1, {0,3}, 0};  /**< not all layers move */
MovLayer ml0 = { &layer0, {0,3}, 0}; 


movLayerDraw(MovLayer *movLayers, Layer *layers)
{
  int row, col;
  MovLayer *movLayer; // This is a dummy to first in LL

  and_sr(~8);			/**< disable interrupts (GIE off) */
  for (movLayer = movLayers; movLayer; movLayer = movLayer->next) { /* for each moving layer */
    Layer *l = movLayer->layer;
    l->posLast = l->pos;
    l->pos = l->posNext;
  }
  or_sr(8);			/**< disable interrupts (GIE on) */
 
  for (movLayer = movLayers; movLayer; movLayer = movLayer->next) { /* for each moving layer */
    Region bounds;
    layerGetBounds(movLayer->layer, &bounds);
    lcd_setArea(bounds.topLeft.axes[0], bounds.topLeft.axes[1], 
		bounds.botRight.axes[0], bounds.botRight.axes[1]);
    for (row = bounds.topLeft.axes[1]; row <= bounds.botRight.axes[1]; row++) {
      for (col = bounds.topLeft.axes[0]; col <= bounds.botRight.axes[0]; col++) {
	Vec2 pixelPos = {col, row};
	u_int color = bgColor;
	Layer *probeLayer;
	for (probeLayer = layers; probeLayer; 
	     probeLayer = probeLayer->next) { /* probe all layers, in order */
	  if (abShapeCheck(probeLayer->abShape, &probeLayer->pos, &pixelPos)) {
	    color = probeLayer->color;
	    break; 
	  } /* if probe check */
	} // for checking all layers at col, row
	lcd_writeColor(color); 
      } // for col
    } // for row
  } // for moving layer being updated
}	  

/** Advances a moving shape within a fence
 *  
 *  \param ml The moving shape to be advanced
 *  \param fence The region which will serve as a boundary for ml
 */
void mlAdvance(MovLayer *ml, Region *fence)
{
  Vec2 newPos;
  u_char axis;
  Region shapeBoundary;
  for (; ml; ml = ml->next) {
    vec2Add(&newPos, &ml->layer->posNext, &ml->velocity);
    abShapeGetBounds(ml->layer->abShape, &newPos, &shapeBoundary);
    for (axis = 0; axis < 2; axis ++) {
      if ((shapeBoundary.topLeft.axes[axis] < fence->topLeft.axes[axis]) ||
	  (shapeBoundary.botRight.axes[axis] > fence->botRight.axes[axis]) ) {
	int velocity = ml->velocity.axes[axis] = -ml->velocity.axes[axis];
	newPos.axes[axis] += 1*velocity;
      }	/**< if outside of fence */
    } /**< for axis */
    ml->layer->posNext = newPos;
  } /**< for ml */
}


void resetPositions(MovLayer *ml, MovLayer *p1, MovLayer *p2){
  Vec2 newPos;
  newPos.axes[0] = screenWidth/2;
  newPos.axes[1] = screenHeight/2;
  ml->layer->posNext = newPos;
  newPos.axes[0] = 10;
  p1->layer->posNext = newPos;
  newPos.axes[0] = 118;
  p2->layer->posNext = newPos;
}

/** Advances a moving shape within a set of boundaries
 *  
 *  \param ml The moving shape to be advanced
 *  \param fenceP1 the region that contains paddle 1
 *  \param fenceP2 the region that contains paddle 2
 */
u_int game(MovLayer *ml, MovLayer *p1, MovLayer *p2, Region *fenceP1, Region *fenceP2, Region *fence)
{
  Vec2 newPos;
  u_char axis;
  Region shapeBoundary;

  /* Manages collisions between ball and paddles */
  vec2Add(&newPos, &ml->layer->posNext, &ml->velocity); // Gets new position (adds position vec to vel vec)
  abShapeGetBounds(ml->layer->abShape, &newPos, &shapeBoundary); // Uses new position to set new boundaries
  if (((shapeBoundary.topLeft.axes[0] <= fenceP1->botRight.axes[0]) &&
       (shapeBoundary.topLeft.axes[1] > fenceP1->topLeft.axes[1]) &&
       (shapeBoundary.topLeft.axes[1] < fenceP1->botRight.axes[1]))||
      ((shapeBoundary.botRight.axes[0] >= fenceP2->topLeft.axes[0]) &&
       (shapeBoundary.botRight.axes[1] > fenceP2->topLeft.axes[1]) &&
       (shapeBoundary.botRight.axes[1] < fenceP2->botRight.axes[1]))) {
    int velocity = ml->velocity.axes[0] = -ml->velocity.axes[0];
    newPos.axes[0] += (1*velocity); 
  }

  /* Manages collisions between ball and horizontal walls */
  if((shapeBoundary.topLeft.axes[1] <= fence->topLeft.axes[1]) ||
     (shapeBoundary.botRight.axes[1] >= fence->botRight.axes[1])){
    int velocity = ml->velocity.axes[1] = -ml->velocity.axes[1];
    newPos.axes[1] += (1*velocity); 
  }

  /* Manages collisions between ball and  vertical walls */
  if(shapeBoundary.topLeft.axes[0] < fence->topLeft.axes[0]){
    return 1;
  }

  if(shapeBoundary.botRight.axes[0] > fence->botRight.axes[0]){
    return 1;
  }

  ml->layer->posNext = newPos; // UPDATE POSNEXT
  return 0;
}



void updateScore(short score[]){
  /* drawString5x7(1,3, "P1:", COLOR_BLACK, COLOR_GREEN); */
  /* drawChar5x7(19,3, score, COLOR_BLACK, COLOR_GREEN); */
  /* drawString5x7(100,3, "P2:", COLOR_BLACK, COLOR_PINK); */
  /* drawChar5x7(121,3, score, COLOR_BLACK, COLOR_PINK); */
}


/** Initializes everything, enables interrupts and green LED, 
 *  and handles the rendering for the screen
 */

void main()
{
  P1DIR |= GREEN_LED;		/**< Green led on when CPU on */		
  P1OUT |= GREEN_LED;

  configureClocks();
  lcd_init();
  shapeInit();
  p2sw_init(15);
  shapeInit();

  buzzer_init(); //added buzzer_init() to program

  layerInit(&layer0);
  layerDraw(&layer0);
  layerGetBounds(&fieldLayer, &fieldFence);
  
  enableWDTInterrupts();      /**< enable periodic interrupt */
  or_sr(0x8);	              /**< GIE (enable interrupts) */

  drawString5x7(50,3, "Score", COLOR_BLACK, COLOR_GREEN);
  scoreP1('0');
  scoreP2('0');
  
  for(;;) {    
    while (!redrawScreen) { /**< Pause CPU if screen doesn't need updating */
      P1OUT &= ~GREEN_LED;    /**< Green led off witHo CPU */
      //or_sr(0x10);	      /**< CPU OFF */
    }
    
    P1OUT |= GREEN_LED;       /**< Green led on when CPU on */
    redrawScreen = 0;
  }
}


/** Watchdog timer interrupt handler. 15 interrupts/sec */
void wdt_c_handler()
{
  static short count = 0;
  static short sound = 0;
  static u_int point = 0;
  static long ya = 0;
  P1OUT |= GREEN_LED;	      

  //song(sound);

    if(++sound > 225) 
      sound = 0; 
    if (count++ == 15) {
      
      /* Update paddle region for collisions */
      layerGetBounds(&layer0, &fencePaddle1);
      layerGetBounds(&layer1, &fencePaddle2);
      
      movLayerDraw(&mlball,&layer2); // Move ball around
      
	point = game(&mlball, &ml0, &ml1 ,&fencePaddle1, &fencePaddle2, &fieldFence);

	if(point){
	  resetPositions(&mlball, &ml0, &ml1);
	  movLayerDraw(&mlball,&layer2); // Move ball around
	  movLayerDraw(&ml0,&layer0);
	  movLayerDraw(&ml1,&layer1);
	  drawChar5x7(screenWidth/2,50, '3', COLOR_BLACK, COLOR_GREEN);
	  while(++ya < 1000000){}
	  drawChar5x7(screenWidth/2,50, '2', COLOR_BLACK, COLOR_GREEN);
	  while(++ya < 2000000){}
	  drawChar5x7(screenWidth/2,50, '1', COLOR_BLACK, COLOR_GREEN);
	  while(++ya < 3000000){}
	  drawChar5x7(screenWidth/2,50, '1', COLOR_YELLOW, COLOR_YELLOW);
	  point = 0;
	  ya = 0;
	}
	
	// SWITCHES //
	u_int switches = p2sw_read(), i;
	for (i = 0; i < 4; i++){
	  if(!(switches & (1<<i))){	
	    if(i == 0){ 
	    ml0.velocity.axes[1] = -4;
	    movLayerDraw(&ml0,&layer0);
	    mlAdvance(&ml0, &fieldFence);
	    redrawScreen = 1;
	    }
	    if(i == 1){
	      ml0.velocity.axes[1] = 4;
	      movLayerDraw(&ml0,&layer0);
	      mlAdvance(&ml0, &fieldFence);
	      redrawScreen = 1;
	    }
	    if(i == 2){
	      ml1.velocity.axes[1] = -4;
	      movLayerDraw(&ml1,&layer1);
	      mlAdvance(&ml1, &fieldFence);
	      redrawScreen = 1;
	    }
	  if(i == 3){
	    ml1.velocity.axes[1] = 4;
	    movLayerDraw(&ml1,&layer1);
	    mlAdvance(&ml1, &fieldFence);
	    redrawScreen = 1;
	  }
	}
	count = 0;
      } 
      P1OUT &= ~GREEN_LED;    /**< Green LED off when cpu off */
    
  }
}

