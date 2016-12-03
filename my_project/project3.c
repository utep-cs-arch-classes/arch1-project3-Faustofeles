#include <msp430.h>
#include <libTimer.h>
#include <lcdutils.h>
#include <lcddraw.h>
#include <shape.h>
#include <abCircle.h>
#include "music.h" // Provides the music notes for buzzer
#include "buzzer.h" // Enables buzzer
#include "p2switches.h"

#define GREEN_LED BIT6

char p1Stats[6] = "P1:0"; // Score stats for player 1
char p2Stats[6] = "P2:0"; // Score stats for player 2
u_int scoreAt = 3;        // Index to update p1Stats & p2Stats
char p1Score = 0;         // Score player 1
char p2Score = 0;         // Score player 2
char playGame = 0;        // Set when game starts
char gameOver = 0;        // Set when game is over

u_int bgColor = COLOR_BLACK;    /**< Initial background color */
int redrawScreen = 1;           /**< Boolean for whether screen needs to be redrawn */
Region fieldFence;		/**< Fence around playing field  */
Region fencePaddle1;            /**< Ball constraints with respect to paddle 1 */
Region fencePaddle2;            /**< Ball constraints with respect to paddle 2 */

AbRect paddle = {abRectGetBounds, abRectCheck, {3,15}};   /**< 3x15 paddle*/

AbRectOutline fieldOutline = {	                      /* Playing Field */
  abRectOutlineGetBounds, abRectOutlineCheck,   
  {screenWidth/2 - 5, screenHeight/2 - 11}
};


/* playing field as a layer */

Layer fieldLayer = {		
  (AbShape *) &fieldOutline,
  {screenWidth/2, screenHeight/2},/**< center */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_YELLOW,
  0,
};


/* Ball moving */
Layer layer2 = {		
  (AbShape *)&circle4,
  {(screenWidth/2), (screenHeight/2)}, /**< bit below & right of center */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_BLACK,
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

/** < Moving layer, LList containing a layer and its velocity */
typedef struct MovLayer_s {
  Layer *layer;
  Vec2 velocity;
  struct MovLayer_s *next;
} MovLayer;

MovLayer mlball = { &layer2, {2,2}, 0}; /**< moving layer for ball */
MovLayer ml1 = { &layer1, {0,3}, 0};  /**< moving layer for paddle player 1 */
MovLayer ml0 = { &layer0, {0,3}, 0}; /**< moving layer for paddle player 2 */


/*
  Updates current, previous and next position of a moving layer. Gets bounds and
  redraws layers.
 */

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

/** Advances a moving shape within taking into account its boundary
 *  constrinsts.
 *  
 *  \param ml The moving shape to be advanced
 *  \param fence The region which will serve as a boundary for ml
 *  
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

/** Thid function prints the full score of a player */
void printScore(char *scoreBoard, char width){
    drawString5x7(width,3, scoreBoard, COLOR_BLACK, COLOR_YELLOW);
}

/** Resets the position of the two paddles and the ball. This function
 *  is used each time a player scores a point to allow time for players
 *  to get ready */
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

/** Function receives an int and determines which player to update the 
 *  the score for. If zero update player 1, else update player 2 */
void updateScore(int player){ 
  if(player == 0){
    p1Score++;
    p1Stats[scoreAt] = '0'+p1Score;
    printScore(p1Stats, 1); // Draws score player 1
  }
  else{
    p2Score++;
    p2Stats[scoreAt] = '0'+p2Score;
    printScore(p2Stats, 104); // Draws score player 2
  }
}


/** Runs most of the game within this function. Ball advances within boundaries
 *  defined here. Collision between ball and paddles are handled within the first
 *  loop. The second loop manages the balls interactions with the upper and lower 
 *  field bounds (bounces), and finally, the third and fourth loop manage the balls
 *  interactions with the left and right bounds (scores).
 *  
 *  \param ml The moving shape to be advanced
 *  \param fenceP1 the region that contains paddle 1
 *  \param fenceP2 the region that contains paddle 2
 */
char game(MovLayer *ml, MovLayer *p1, MovLayer *p2, Region *fenceP1, Region *fenceP2, Region *fence)
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

  /* Manages collisions between ball and vertical walls */
  if(shapeBoundary.topLeft.axes[0] < fence->topLeft.axes[0]){
    updateScore(1);
    return 1;
  }

  if(shapeBoundary.botRight.axes[0] > fence->botRight.axes[0]){
    updateScore(0);
    return 1;
  }

  ml->layer->posNext = newPos; // UPDATE POSNEXT
  return 0;
}


/** Initializes everything, enables interrupts and green LED, 
 *  and handles the rendering for the screen
 */


void main()
{
  P1DIR |= GREEN_LED;
  P1OUT |= GREEN_LED;

  configureClocks();
  lcd_init();
  shapeInit();
  layerInit(&layer0);
  layerDraw(&layer0);
  layerGetBounds(&fieldLayer, &fieldFence);
  
  enableWDTInterrupts();      /**< enable periodic interrupt */
  or_sr(0x8);	              /**< GIE (enable interrupts) */
  
  
  for(;;) {    
    while (!redrawScreen) { /**< Pause CPU if screen doesn't need updating */
      P1OUT &= ~GREEN_LED;    /**< Green led off witHo CPU */
      or_sr(0x10);	      /**< CPU OFF */
    }
    
    P1OUT |= GREEN_LED;       /**< Green led on when CPU on */
    redrawScreen = 0;
  }
}


/** Watchdog timer interrupt handler. 15 interrupts/sec */
void wdt_c_handler()
{
  /* static short score[2]; // score[0] for player 1, and score[1] for player 2 */
  /* score[0] = 0; score[1] = 0; // Both players start with score of 0 */

  static short count = 0;
  static short sound = 0;
  static char point = 0;
  static long wait = 0;
  u_char width = 60;
  
  P1OUT |= GREEN_LED;	      

  /** This loop shows the players the game's instructions before game starts */
  while(!playGame){
    drawString5x7(screenWidth/2-30,20, "PAD MANIA", COLOR_GOLD, COLOR_BLACK); // Title
    drawString5x7(10,50, "S1: Player 1 UP", COLOR_WHITE, COLOR_BLACK); // Instruction
    drawString5x7(10,65, "S2: Player 1 DOWN", COLOR_GREEN, COLOR_BLACK); // Instruction
    drawString5x7(10,80, "S3: Player 2 UP", COLOR_RED, COLOR_BLACK); // Instruction
    drawString5x7(10,95, "S4: Player 2 DOWN", COLOR_ORANGE, COLOR_BLACK); // Instruction
    drawString5x7(10,115, "  ~Press to play!", COLOR_HOT_PINK, COLOR_BLACK); // Instruction
    if(++wait == 150){ // Wait for player to read instructions
      wait = 0; // Reset wait
      playGame = 1; // Ready to start game
      bgColor = COLOR_YELLOW; // Change of screen Color

      /* Paints Game Screen */
      lcd_init();
      p2sw_init(15); // Initializes switches for paddle control
      buzzer_init(); // Initializes buzzer for music 
      layerDraw(&layer0); // Paints new screen

      /* Draws Scores */
      drawString5x7(50,3, "Score", COLOR_BLACK, COLOR_YELLOW);
      printScore(p1Stats, 1); // Draws score player 1
      printScore(p2Stats, 104); // Draws score player 2
    }
  }

  if(!gameOver)
    song(sound); // Play tone while playing
  if(++sound > 225) 
    sound = 0;   // Reset counter

  if (count++ == 15) {
    
    /* Update paddle region for collisions */
    layerGetBounds(&layer0, &fencePaddle1);
    layerGetBounds(&layer1, &fencePaddle2);
    
    movLayerDraw(&mlball,&layer2); // Move ball around
    
    point = game(&mlball, &ml0, &ml1 ,&fencePaddle1, &fencePaddle2, &fieldFence);
    
    /* Manages wait time between scored points */
    if(point){
      resetPositions(&mlball, &ml0, &ml1); //Resets shape's pos to original position
      movLayerDraw(&mlball,&layer2); // Repaints ball
      movLayerDraw(&ml0,&layer0);    // Repaints paddle 1
      movLayerDraw(&ml1,&layer1);    // Repaints paddle 2
      drawChar5x7(screenWidth/2,50, '3', COLOR_BLACK, COLOR_YELLOW);  // Starts countdown
      while(++wait < 1000000){}                                       // Waits
      drawChar5x7(screenWidth/2,50, '2', COLOR_BLACK, COLOR_YELLOW);
      while(++wait < 2000000){}                                       // Waits
      drawChar5x7(screenWidth/2,50, '1', COLOR_BLACK, COLOR_YELLOW);
      while(++wait < 3000000){}                                       // Waits
      drawChar5x7(screenWidth/2,50, '1', COLOR_YELLOW, COLOR_YELLOW); // Clears countdown
      point = 0; // Reset point offset
      wait = 0; // Reset wait
    }

    /* Finishes game if a player reaches a score of 7 */
    if(p1Score == 7 || p2Score == 7){
      gameOver = 1; // Game Over
      char *winner;
      (p1Score == 7)?(winner = "Player 1!!!"):(winner = "Player 2!!!");
      bgColor = COLOR_GOLD; // Set new color
      
      /* Paints Game Over Screen */
      layerDraw(&layer0);
      and_sr(~8);	              /**< GIE (unable interrupts) */
      buzzer_set_note(1);

      /* Prints Game Over message and WINNER */
      drawString5x7(screenWidth/2-30,20, "PAD MANIA", COLOR_BLUE, COLOR_GOLD);
      drawString5x7(screenWidth/2-30,50, "GAME OVER!", COLOR_RED, COLOR_GOLD);
      drawString5x7(screenWidth/2-30,100, "WINNER:", COLOR_BLACK, COLOR_GOLD);
      drawString5x7(screenWidth/2-30,115, winner, COLOR_BLACK, COLOR_GOLD);
      
      while(1){} // stays here forever (temporary fix)
    }
    
    /* SWITCHES */
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

