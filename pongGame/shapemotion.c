/** \file shapemotion.c
 *  \brief This is a simple shape motion demo.
 *  This demo creates two layers containing shapes.
 *  One layer contains a rectangle and the other a circle.
 *  While the CPU is running the green LED is on, and
 *  when the screen does not need to be redrawn the CPU
 *  is turned off along with the green LED.
 */
#include <stdio.h>
#include <msp430.h>
#include <libTimer.h>
#include <lcdutils.h>
#include <lcddraw.h>
#include <p2switches.h>
#include <shape.h>
#include <abCircle.h>
#include "buzzer.h"

#define GREEN_LED BIT6
#define WIDTH 2
#define LENGTH 10
static int increment;
static int onesPlace = 0;
static int tensPlace = 0;
static int hundredsPlace = 0;
//definitions for score
static char score1[3];
static char score2[3];



AbRect rectanglePanel = {
        abRectGetBounds, abRectCheck, {WIDTH, LENGTH}
};



AbRectOutline fieldOutline = {	/* playing field */
  abRectOutlineGetBounds, abRectOutlineCheck,   
  {screenWidth/2-5, screenHeight/2-5}
};



Layer BallLayerL3 = {		/** Layer with a violet Ball */
  (AbShape *) &circle8,
  {(screenWidth/2)+10, (screenHeight/2)+5}, /**< bit below & right of center */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_VIOLET,
  0,
};


Layer fieldLayerL2 = {		/* playing field as a layer */
  (AbShape *) &fieldOutline,
  {screenWidth/2, screenHeight/2},/**< center */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_BLACK,
  &BallLayerL3,
};

Layer leftPadL1 = {		/**< Layer with left pad */
  (AbShape *)&rectanglePanel,
  {(screenWidth/2)-49, (screenHeight/2)+8}, /**< left */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_BLACK,
  &fieldLayerL2,
};

Layer rightPadL0 = {		/**< Layer with right pad */
  (AbShape *)&rectanglePanel,
  {(screenWidth/2)+50, (screenHeight/2)+5}, /**< right */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_BLACK,
  &leftPadL1,
};

/** Moving Layer
 *  Linked list of layer references
 *  Velocity represents one iteration of change (direction & magnitude)
 */
typedef struct MovLayer_s {
  Layer *layer;
  Vec2 velocity;
  struct MovLayer_s *next;
} MovLayer;

MovLayer ml3 = { &BallLayerL3, {-1,2}, 0 };//layer for ball
MovLayer ml1 = { &leftPadL1, {0,1}, &ml3 };//left paddle
MovLayer ml0 = { &rightPadL0, {0,1}, &ml1 };//right paddle


//moves layers in game
void movLayerDraw(MovLayer *movLayers, Layer *layers)
{
  int row, col;
  MovLayer *movLayer;

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
	} // for checking all layes at col, row
	lcd_writeColor(color); 
      } // for col
    } // for row
  } // for moving layer being updated
}	  



//Region fence = {{10,30}, {SHORT_EDGE_PIXELS-10, LONG_EDGE_PIXELS-10}}; /**< Create a fence region */

/** Advances a moving shape within a fence
 *  
 *  \param ml The moving shape to be advanced
 *  \param fence The region which will serve as a boundary for ml
 */
void detectCollisions( Layer *rightPadL0, Layer *leftPadL1, Layer *BallLayerL3, MovLayer *ml, Region *fence )
{
  Vec2 newPos;
  u_char axis;
  Layer *Pad1 = rightPadL0;
  Layer *Pad2 = leftPadL1;
  Layer *Ball = BallLayerL3;

  Region shapeBoundary;
    vec2Add(&newPos, &ml->layer->posNext, &ml->velocity);
    abShapeGetBounds(ml->layer->abShape, &newPos, &shapeBoundary);

    if (((shapeBoundary.topLeft.axes[0] <= fence->botRight.axes[0]) &&  //ball bouncing
         (shapeBoundary.topLeft.axes[1] > fence->topLeft.axes[1]) &&
         (shapeBoundary.topLeft.axes[1] < fence->botRight.axes[1]))||
        ((shapeBoundary.botRight.axes[0] >= fence->topLeft.axes[0]) &&
         (shapeBoundary.botRight.axes[1] > fence->topLeft.axes[1]) &&
         (shapeBoundary.botRight.axes[1] < fence->botRight.axes[1]))) {
        int velocity = ml->velocity.axes[0] = -ml->velocity.axes[0];
        newPos.axes[0] += (2*velocity);
       }	/**< if outside of fence */

    //hori wall
    if((shapeBoundary.topLeft.axes[1] <= fence->topLeft.axes[1]) ||
       (shapeBoundary.botRight.axes[1] >= fence->botRight.axes[1])){
          int velocity = ml->velocity.axes[1] = -ml->velocity.axes[1];
           newPos.axes[1] += (2*velocity);
    }

/* Manages collisions between ball and vertical walls */
    if(shapeBoundary.topLeft.axes[0] < fence->topLeft.axes[0]){
        increment = 1;
    }

    if(shapeBoundary.botRight.axes[0] > fence->botRight.axes[0]){
        increment = 2;

    }

    ml->layer->posNext = newPos; // UPDATE POSNEXT

}

/** Function receives an int and determines which player to update the
 *  the score for. If zero update player 1, else update player 2 */
void updateScore(int player){

  if (onesPlace<9 && increment > 0 ) {
    onesPlace++;

    if(player == 1){
      score1[2] = onesPlace;
    }
    else{
      score2[2] = onesPlace;
    }
    increment = 0;

  }else if(increment > 0 && onesPlace == 9 && tensPlace < 9) {
    onesPlace = 0;
    tensPlace++;

    if(player == 1){
      score1[2] = onesPlace;
      score1[1] = tensPlace;
    }
    else{
      score2[2] = onesPlace;
      score2[1] = tensPlace;
    }
    increment = 0;

  }else if( onesPlace == 9 && tensPlace == 9 && hundredsPlace < 9 && increment > 0) {
    onesPlace = 0;
    tensPlace = 0;
    hundredsPlace++;

    if(player == 1){
      score1[2] = onesPlace;
      score1[1] = tensPlace;
      score1[0] = hundredsPlace;
    }
    else{
      score2[2] = onesPlace;
      score2[1] = tensPlace;
      score2[0] = hundredsPlace;
    }
    increment = 0;
  }

}


u_int bgColor = COLOR_BLUE;     /**< The background color */
int redrawScreen = 1;           /**< Boolean for whether screen needs to be redrawn */

Region fieldFence;		/**< fence around playing field  */


/** Initializes everything, enables interrupts and green LED, 
 *  and handles the rendering for the screen
 */
void main() {
  P1DIR |= GREEN_LED;        /**< Green led on when CPU on */
  P1OUT |= GREEN_LED;

  configureClocks();
  lcd_init();
  shapeInit();
  p2sw_init(15);
  buzzer_init();

  shapeInit();

  layerInit(&rightPadL0);
  layerDraw(&rightPadL0);

  layerGetBounds(&fieldLayerL2, &fieldFence);

  enableWDTInterrupts();      /**< enable periodic interrupt */
  or_sr(0x8);                  /**< GIE (enable interrupts) */

  u_int j;

  for (j = 0; j < 3; j++)
    score1[j] = '0';
    score2[j] = '0';

  drawString5x7(45, 5, "SCORE", COLOR_GOLD, COLOR_BLACK);
  drawString5x7(1,3,score1,COLOR_GOLD, COLOR_BLACK);
  drawString5x7(90,3,score2,COLOR_GOLD, COLOR_BLACK);


  score1[3] = 0;
  score2[3] = 0;

  for (;;) {

    while (!redrawScreen) { /**< Pause CPU if screen doesn't need updating */
      P1OUT &= ~GREEN_LED;    /**< Green led off witHo CPU */
      or_sr(0x10);          /**< CPU OFF */
    }

    P1OUT |= GREEN_LED;       /**< Green led on when CPU on */
    redrawScreen = 0;
    movLayerDraw(&ml0, &rightPadL0);


    drawString5x7(45, 0, "SCORE", COLOR_GOLD, COLOR_BLACK);
    drawString5x7(1,3,score1,COLOR_GOLD, COLOR_BLACK);
    drawString5x7(100,3,score2,COLOR_GREEN, COLOR_BLACK);



  }

 }

void buttonSense(u_int sw, MovLayer *left, MovLayer *right) {
  int b1 = 0;
  int b2 = 1;
  int b3 = 2;
  int b4 = 3;


  Vec2 lPadUpdate;
  Vec2 rPadUpdate;

  vec2Add(&lPadUpdate, &left->layer->posNext, &left->velocity);
  vec2Add(&rPadUpdate, &right->layer->posNext, &right->velocity);

  int velocity = left->velocity.axes[1];


      if (i == b1) {
        upBuzz(); //sound for up
        lPadUpdate.axes[1] += (velocity + 10);
        left->layer->posNext = lPadUpdate;

      } else if (i == b2) {
        downBuzz();
        lPadUpdate.axes[1] += (velocity - 10);
        left->layer->posNext = lPadUpdate;

      } else if (i == b3) {
        upBuzz();
        rPadUpdate.axes[1] += (velocity + 10);
        right->layer->posNext = rPadUpdate;

      } else if (i == b4) {
        downBuzz();
        rPadUpdate.axes[1] += (velocity - 10);
        right->layer->posNext = rPadUpdate;

      }

    }



/** Watchdog timer interrupt handler. 15 interrupts/sec */
void wdt_c_handler()
{
  static short count = 0;
  P1OUT |= GREEN_LED;		      /**< Green LED on when cpu on */
  count ++;
  //if (count == 15) {
    detectCollisions(&leftPadL1, &rightPadL0, &BallLayerL3, &ml0, &fieldFence);
    u_int sw = p2sw_read();
    buttonSense(sw, &ml1, &ml0);

    if (increment > 0) {
      updateScore(1);
    } else {
      updateScore(2);
    }

}