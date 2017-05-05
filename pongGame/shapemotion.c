
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
 u_int bgColor = COLOR_BLUE;     /**< The background color */




AbRect rectanglePanel = {
        abRectGetBounds, abRectCheck, {WIDTH, LENGTH}
};


AbRectOutline fieldOutline = {	/* playing field */
  abRectOutlineGetBounds, abRectOutlineCheck,
  {screenWidth/2-10, screenHeight/2-10}
};

 Layer BallLayerL2 = {		/** Layer with a violet Ball */
         (AbShape *)&circle8,
         {(screenWidth/2)+10, (screenHeight/2)+5}, /**< bit below & right of center */
         {0,0}, {0,0},				    /* last & next pos */
         COLOR_VIOLET,
         0,
 };
 Layer fieldLayerL3 = {		/* playing field as a layer */
         (AbShape *) &fieldOutline,
         {screenWidth/2, screenHeight/2},/**< center */
         {0,0}, {0,0},				    /* last & next pos */
         COLOR_BLACK,
         &BallLayerL2,
 };
 

Layer leftPadL1 = {		/**< Layer with left pad */
  (AbShape *)&rectanglePanel,
  {(screenWidth/2)-49, (screenHeight/2)+8}, /**< left */
  {0,0}, {0,0},				    /* last & next pos */
  COLOR_BLACK,
  &fieldLayerL3,
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

MovLayer ml3 = { &BallLayerL2, {1,1}, 0 };//layer for ball
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

//score point func
void scorePoint(int player) {

    if (onesPlace < 9 && increment > 0) {
        onesPlace++;

        if (player == 1) {
            score1[0] = '0' + 1;
        } else {
            score2[2] = '0' + 1;
        }
        increment = 0;

    }
}


//Region fence = {{10,30}, {SHORT_EDGE_PIXELS-10, LONG_EDGE_PIXELS-10}}; /**< Create a fence region */

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
                (shapeBoundary.botRight.axes[axis] > fence->botRight.axes[axis])) {
                int velocity = ml->velocity.axes[axis] = -ml->velocity.axes[axis];
                newPos.axes[axis] += (2 * velocity);
            }    /**< if outside of fence */

            if (ml->layer->abShape == ml3.layer->abShape) {

            if (abShape(rightPadL0->layer->abShape, rightPadL0->layer->posNext, &newPos)) {
                int velocity = ml3.velocity.axes[axis] = -ml3.velocity.axes[axis];
                newPos.axes[axis] += (2 * velocity);
                hitBuzz();
                increment = '0' + 1; //player one score
                scorePoint(increment);
            }

            if (shapeBoundary.botRight.axes[axis] > fence->botRight.axes[axis]) {
                 int velocity = ml->velocity.axes[axis] = -ml->velocity.axes[axis];
                newPos.axes[axis] += (2 * velocity);
                hitBuzz();
                increment = '0' + 2; //p2 score
                scorePoint(increment);

            }
        }

        } /**< for axis */
        ml->layer->posNext = newPos;
    } /**< for ml */
}





void switchHandler(u_int switches){

        if (!(switches & (1 << 0))) {

                upBuzz();
                ml1.velocity.axes[1] = 4;
            }
            else if (!(switches & (1 << 1))) {
                downBuzz();
                ml1.velocity.axes[1] = -4;
            }
            else if (!(switches & (1 << 2))) {
                upBuzz();
                ml0.velocity.axes[1] = 4;
            }
            else if (!(switches & (1 << 3))) {
                downBuzz();
                ml0.velocity.axes[1] = -4;
            }
            else{
                ml0.velocity.axes[1] = 0;
                ml1.velocity.axes[1] = 0;
            }
        }


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
  p2sw_init(15);
  shapeInit();
    buzzer_init();


  layerInit(&rightPadL0);
  layerDraw(&rightPadL0);


  layerGetBounds(&fieldLayerL3, &fieldFence);



  enableWDTInterrupts();      /**< enable periodic interrupt */
  or_sr(0x8);                  /**< GIE (enable interrupts) */

  u_int j;

  for (j = 0; j < 3; j++) {//fill in 000 for score
      score1[j] = '0';
      score2[j] = '0';
  }


  score1[3] = 0;
  score2[3] = 0;

    u_int switches;
  for (;;) {
      switches = p2sw_read();
    while (!redrawScreen) { /**< Pause CPU if screen doesn't need updating */
      P1OUT &= ~GREEN_LED;    /**< Green led off witHo CPU */
     or_sr(0x10);          /**< CPU OFF */
    }


    P1OUT |= GREEN_LED;       // Green led on when CPU on


      switchHandler(switches);

      drawString5x7(45, 0, "SCORE", COLOR_GOLD, COLOR_BLACK); //shows score
      drawString5x7(50,3,score1,COLOR_BLACK, COLOR_WHITE);

      redrawScreen =0;
      movLayerDraw(&ml0, &rightPadL0); // Move ball

  }//end for

 }




/** Watchdog timer interrupt handler. 15 interrupts/sec */
void wdt_c_handler() {

   static short count = 0;
   P1OUT |= GREEN_LED;              /**< Green LED on when cpu on */
   count++;
   if (count == 15) {
     mlAdvance(&ml0, &fieldFence); //detect any collisions

       if (p2sw_read()){
           redrawScreen = 1;
       }
       count = 0;
   }
    P1OUT &= ~GREEN_LED;    /**< Green LED off when cpu off */
 }




