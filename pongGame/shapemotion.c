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

int abSlicedRectCheck(const AbRect *rect, const Vec2 *centerPos, const Vec2 *pixel){
  Vec2 relPos;
  //vector from cener of pixel
  vec2Sub(&relPos, pixel, centerPos);
  //reject pixels in specified sclice
  if( relPos.axes[0] == 0 && relPos.axes[0]/2 == relPos.axes[0] &&
      relPos.axes[1]/2 != relPos.axes[1]){
    return 0;
  }else{
    return abRectCheck(rect,centerPos,pixel);
  }
}


AbRect rectanglePanel = {
        abRectGetBounds, abRectCheck, {WIDTH, LENGTH}
};

AbRect rectangleLine = {
        abRectGetBounds, abSlicedRectCheck, {5,70}
};


AbRectOutline fieldOutline = {	/* playing field */
  abRectOutlineGetBounds, abRectOutlineCheck,   
  {screenWidth/2-5, screenHeight/2-5}
};

Layer middleDiv = {
        //middle line division
        (AbShape *)&rectangleLine,
        //bit below & right of center
        {(screenWidth/2), (screenHeight/2)},
        //last & next pos
        {0,0}, {0,0},
        COLOR_WHITE,
        0
};

Layer BallLayer = {		/** Layer with a violet Ball */
  (AbShape *) &circle8,
  {(screenWidth/2)+10, (screenHeight/2)+5}, /**< bit below & right of center */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_VIOLET,
  &middleDiv,
};


Layer fieldLayer = {		/* playing field as a layer */
  (AbShape *) &fieldOutline,
  {screenWidth/2, screenHeight/2},/**< center */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_BLACK,
  &BallLayer,
};

Layer leftPad = {		/**< Layer with left pad */
  (AbShape *)&rectanglePanel,
  {(screenWidth/2)-49, (screenHeight/2)+8}, /**< left */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_BLACK,
  &fieldLayer,
};

Layer rightPad = {		/**< Layer with right pad */
  (AbShape *)&rectanglePanel,
  {(screenWidth/2)+50, (screenHeight/2)+5}, /**< right */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_BLACK,
  &leftPad,
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

MovLayer ml3 = { &BallLayer, {-1,2}, 0 };
MovLayer ml1 = { &leftPad, {0,1}, &ml3 };
MovLayer ml0 = { &rightPad, {0,1}, &ml1 };

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
void detectCollisions( Layer *rightPad, Layer *leftPad, Layer *BallLayer, MovLayer *ml, Region *fence )
{
  int radius = (WIDTH/2);
  Vec2 newPos;
  u_char axis;
  Layer *Pad1 = rightPad;
  Layer *Pad2 = leftPad;
  Layer *Ball = BallLayer;

  Region shapeBoundary;
  for (; ml; ml = ml->next) {
    vec2Add(&newPos, &ml->layer->posNext, &ml->velocity);
    abShapeGetBounds(ml->layer->abShape, &newPos, &shapeBoundary);
    for (axis = 0; axis < 2; axis ++) {
      if ((shapeBoundary.topLeft.axes[0] < fence->topLeft.axes[0]) ||
	  (shapeBoundary.botRight.axes[0] > fence->botRight.axes[0]) ) {

        clearScreen(COLOR_STEEL_BLUE);
        drawString5x7(20, 60, "you lost", COLOR_GREEN, COLOR_BLACK);

        or_sr(0x10);
        P1OUT &= ~GREEN_LED;
      }else if((shapeBoundary.topLeft.axes[1] < fence->topLeft.axes[1]) ||
               (shapeBoundary.botRight.axes[1] > fence->botRight.axes[1]) ) {

        int velocity = ml->velocity.axes[axis] = -ml->velocity.axes[axis];
        newPos.axes[axis] += (2 * velocity);

      }else if(

        (Ball->pos.axes[0]-radius <= leftPad->pos.axes[0] + WIDTH) &&
        (Ball->pos.axes[1] >= leftPad->pos.axes[1] - LENGTH) &&
        (Ball->pos.axes[1] <= leftPad->pos.axes[1] + LENGTH)  ||
        (Ball->pos.axes[0]+radius >= rightPad->pos.axes[0] - WIDTH) &&
        (Ball->pos.axes[1] <= rightPad->pos.axes[1] + LENGTH) &&
        (Ball->pos.axes[1] >= rightPad->pos.axes[1] - LENGTH) ){

        //set flag to give points
        increment = 1;
        //change velocity and direction
        int velocity = ml->velocity.axes[axis] = -ml->velocity.axes[axis];
        newPos.axes[axis] += (8*velocity);


      }	/**< if outside of fence */
    } /**< for axis */
    ml->layer->posNext = newPos;
  } /**< for ml */
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

  layerInit(&rightPad);
  layerDraw(&rightPad);


  layerGetBounds(&fieldLayer, &fieldFence);


  enableWDTInterrupts();      /**< enable periodic interrupt */
  or_sr(0x8);                  /**< GIE (enable interrupts) */


  //definitions for score
  char score1[3];
  char score2[3];
  u_int j;
  for (j = 0; j < 3; j++)
    score1[j] = '0';
    score2[j] = '0';



  drawString5x7(20, 15, score1, COLOR_RED, COLOR_BLACK);
  drawString5x7(10, 5, "P1 SCORE", COLOR_GOLD, COLOR_BLACK);

  drawString5x7(25, 15, score2, COLOR_RED, COLOR_BLACK);
  drawString5x7(45, 5, "P2 SCORE", COLOR_GOLD, COLOR_BLACK);

  for (;;) {

    while (!redrawScreen) { /**< Pause CPU if screen doesn't need updating */
      P1OUT &= ~GREEN_LED;    /**< Green led off witHo CPU */
      or_sr(0x10);          /**< CPU OFF */
    }

    P1OUT |= GREEN_LED;       /**< Green led on when CPU on */
    redrawScreen = 0;
    movLayerDraw(&ml0, &rightPad);

    score1[3] = 0;
    score2[3] = 0;


    if ( onesPlace<9 && increment == 1 ) {
      increment = 0;
      onesPlace++;
      score1[2] = onesPlace;
        score2[2] = onesPlace;


    }else if(increment == 1 && onesPlace == 9 && tensPlace < 9) {
      increment = 0;
      onesPlace = 0;
      tensPlace++;
      score1[2] = onesPlace;
      score1[1] = tensPlace;
        score2[2] = onesPlace;
        score2[1] = tensPlace;

    }else if( onesPlace == 9 && tensPlace == 9 && hundredsPlace < 9 && increment == 1) {
      increment = 0;
      onesPlace = 0;
      tensPlace = 0;
      hundredsPlace++;
      score1[2] = onesPlace;
      score1[1] = tensPlace;
      score1[0] = hundredsPlace;
        score2[2] = onesPlace;
        score2[1] = tensPlace;
        score2[0] = hundredsPlace;
    }
  }

 }

void buttonSense(int j, MovLayer *left, MovLayer *right) {
  int b1=0;
  int b2=0;
  int b3=0;
  int b4=0;

  Vec2 lPadUpdate;
  Vec2 rPadUpdate;

  vec2Add(&lPadUpdate, &left->layer->posNext, &left->velocity);
  vec2Add(&rPadUpdate, &right->layer->posNext, &right->velocity);

  int velocity = left->velocity.axes[1];

  if(j==b1){
    upBuzz(); //sound for up
    lPadUpdate.axes[1] += (velocity+10);
    left->layer->posNext = lPadUpdate;

  }

  else if(j==b2){
    downBuzz();
    lPadUpdate.axes[1] += (velocity-10);
    left->layer->posNext = lPadUpdate;

  }
  else if(j==b3){
    upBuzz();
    rPadUpdate.axes[1] += (velocity+10);
    right->layer->posNext = rPadUpdate;

  }
  else if(j==b4){
    downBuzz();
    rPadUpdate.axes[1] += (velocity-10);
    right->layer->posNext = rPadUpdate;

  }

}


/** Watchdog timer interrupt handler. 15 interrupts/sec */
void wdt_c_handler()
{
  static short count = 0;
  P1OUT |= GREEN_LED;		      /**< Green LED on when cpu on */
  count ++;
  if (count == 15) {
    detectCollisions( &leftPad, &rightPad, &BallLayer, &ml0, &fieldFence);
    u_int sw ;
    sw = p2sw_read();

    buttonSense(i,&ml1,&ml0);
      }


  P1OUT &= ~GREEN_LED;		    /**< Green LED off when cpu off */
}
