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
  {screenWidth/2 - 4, screenHeight/2 - 4}
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

Layer ballLayer = {		/** Layer with a violet ball */
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
  &ballLayer
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

MovLayer ml3 = { &ballLayer, {-1,2}, 0 }; /**< not all layers move */
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
void detectCollisions(MovLayer *ml, Region *fence, Layer *rightPad, Layer *leftPad, Layer *ballLayer, Layer *middleDiv )
{
  int radius = (WIDTH/2);
  Vec2 newPos;
  u_char axis;
  Layer *Pad1 = rightPad;
  Layer *Pad2 = ballLayer;
  Layer *Ball = middleDiv;

  Region shapeBoundary;
  for (; ml; ml = ml->next) {
    vec2Add(&newPos, &ml->layer->posNext, &ml->velocity);
    abShapeGetBounds(ml->layer->abShape, &newPos, &shapeBoundary);
    for (axis = 0; axis < 2; axis ++) {
      if ((shapeBoundary.topLeft.axes[0] < fence->topLeft.axes[0]) ||
	  (shapeBoundary.botRight.axes[0] > fence->botRight.axes[0]) ) {
	int velocity = ml->velocity.axes[axis] = -ml->velocity.axes[axis];
	newPos.axes[axis] += (2*velocity);
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

  shapeInit();

  layerInit(&rightPad);
  layerDraw(&rightPad);


  layerGetBounds(&fieldLayer, &fieldFence);


  enableWDTInterrupts();      /**< enable periodic interrupt */
  or_sr(0x8);                  /**< GIE (enable interrupts) */


  //definitions for score
  char score[3];
  int j;
  for (j = 0; j < 3; j++)
    score[j] = '0';



  for (;;) {
    while (!redrawScreen) { /**< Pause CPU if screen doesn't need updating */
      P1OUT &= ~GREEN_LED;    /**< Green led off witHo CPU */
      or_sr(0x10);          /**< CPU OFF */
    }
    P1OUT |= GREEN_LED;       /**< Green led on when CPU on */
    redrawScreen = 0;
    movLayerDraw(&ml0, &rightPad);

    score[3] = 0;

    drawString5x7(90, 15, score, COLOR_RED, COLOR_BLACK);
    drawString5x7(45, 5, "YOUR SCORE: ", COLOR_GOLD, COLOR_BLACK);

    if ( onesPlace<9 && increment == 1 ) {
      increment = 0;
      onesPlace++;
      score[2] = onesPlace;

    }else if(increment == 1 && onesPlace == 9 && tensPlace < 9) {
      increment = 0;
      onesPlace = 0;
      tensPlace++;
      score[2] = onesPlace;
      score[1] = tensPlace;

    }else if( onesPlace == 9 && tensPlace == 9 && hundredsPlace < 9 && increment == 1) {
      increment = 0;
      onesPlace = 0;
      tensPlace = 0;
      hundredsPlace++;
      score[2] = onesPlace;
      score[1] = tensPlace;
      score[0] = hundredsPlace;
    }
  }

 }
/** Watchdog timer interrupt handler. 15 interrupts/sec */
void wdt_c_handler()
{
  static short count = 0;
  P1OUT |= GREEN_LED;		      /**< Green LED on when cpu on */
  count ++;
  if (count == 15) {
    detectCollisions(&rightPad, &leftPad, &ballLayer, &middleDiv, &ml0, &fieldFence);
    if (p2sw_read())
      redrawScreen = 1;
    count = 0;
  } 
  P1OUT &= ~GREEN_LED;		    /**< Green LED off when cpu off */
}
