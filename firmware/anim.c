#include <avr/io.h>      // this contains all the IO port definitions
#include <avr/interrupt.h>
#include <util/delay.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "util.h"
#include "ratt.h"
#include "ks0108.h"
#include "glcd.h"
#include "deathclock.h"

extern volatile uint8_t time_s, time_m, time_h;
extern volatile uint8_t old_m, old_h;
extern volatile uint8_t date_m, date_d, date_y;
extern volatile uint8_t death_m, death_d, death_y;
extern volatile uint8_t alarming, alarm_h, alarm_m;
extern volatile uint8_t time_format;
extern volatile uint8_t region;
extern volatile uint8_t score_mode;

extern volatile int32_t minutes_left, old_minutes_left;
extern volatile uint8_t dc_mode;

uint32_t left_score, right_score, results;

float ball_x, ball_y;
float oldball_x, oldball_y;
float ball_dx, ball_dy;

int8_t rightpaddle_y, leftpaddle_y;
uint8_t oldleftpaddle_y, oldrightpaddle_y;
int8_t rightpaddle_dy, leftpaddle_dy;

extern volatile uint8_t minute_changed, hour_changed;

volatile uint8_t redraw_time = 0;
volatile uint8_t last_score_mode = 0;

extern const uint8_t skull_0[];
extern const uint8_t skull_1[];
extern const uint8_t skull_2[];
extern const uint8_t reaper_0[];
extern const uint8_t reaper_1[];
extern const uint8_t rip_0[];
extern const uint8_t rip_1[];


// special pointer for reading from ROM memory
PGM_P skull0_p PROGMEM = skull_0;
PGM_P skull1_p PROGMEM = skull_1;
PGM_P skull2_p PROGMEM = skull_2;
PGM_P reaper0_p PROGMEM = reaper_0;
PGM_P reaper1_p PROGMEM = reaper_1;
PGM_P rip0_p PROGMEM = rip_0;
PGM_P rip1_p PROGMEM = rip_1;


//void blitsegs_rom(uint8_t x_origin, uint8_t y_origin, PGM_P bitmap_p, uint8_t height, uint8_t inverted)
void render_image (uint8_t image, uint8_t x)
{
	switch(image)
	{
		default:
		case 0:
		  blitsegs_rom(x+0,0,skull0_p, 64, 0);
		  blitsegs_rom(x+28,0,skull1_p, 64, 0);
		  blitsegs_rom(x+56,0,skull2_p, 64, 0);
		  break;
		case 1:
		  blitsegs_rom(x+0,0,reaper0_p, 64, 0);
		  blitsegs_rom(x+28,0,reaper1_p, 64, 0);
		  break;
		case 2:
		  blitsegs_rom(x+0,0,rip0_p, 64, 0);
		  blitsegs_rom(x+28,0,rip1_p, 64, 0);
		  break;
	}
}


void setscore(void)
{
  if(score_mode != last_score_mode) {
  	//Death Clock and Death Alarm requires 8 digits to be drawn, while the remaining modes, only require 4 digits.
  	if(((score_mode == SCORE_MODE_DEATH_TIME) || (score_mode == SCORE_MODE_DEATH_ALARM)) && ((last_score_mode != SCORE_MODE_DEATH_TIME) && (last_score_mode != SCORE_MODE_DEATH_ALARM)))
  	  redraw_time = 2;
  	else if(((last_score_mode == SCORE_MODE_DEATH_TIME) || (last_score_mode == SCORE_MODE_DEATH_ALARM)) && ((score_mode != SCORE_MODE_DEATH_TIME) && (score_mode != SCORE_MODE_DEATH_ALARM)))
  	  redraw_time = 2;
  	else
      redraw_time = 1;
    last_score_mode = score_mode;
  }
  switch(score_mode) {
    case SCORE_MODE_TIME:
      if(alarming && (minute_changed || hour_changed)) {
      	if(hour_changed) {
	      left_score = old_h;
	      right_score = old_m;
	    } else if (minute_changed) {
	      right_score = old_m;
	    }
      } else {
        left_score = time_h;
        right_score = time_m;
      }
      break;
    case SCORE_MODE_DATE:
      if(region == REGION_US) {
        left_score = date_m;
        right_score = date_d;
      } else {
        left_score = date_d;
        right_score = date_m;
      }
      break;
    case SCORE_MODE_YEAR:
      left_score = 20;
      right_score = date_y;
      break;
    case SCORE_MODE_DEATH_TIME:
      if(alarming && (minute_changed || hour_changed)) {
      	if(hour_changed) {
	      left_score = old_minutes_left/10000;
	      right_score = old_minutes_left%10000;
	    } else if (minute_changed) {
	      right_score = old_minutes_left%10000;
	    }
      } else {
        left_score = minutes_left/10000;
        right_score = minutes_left%10000;
      }
      break;
    case SCORE_MODE_DEATH_DATE:
      if(region == REGION_US) {
        left_score = death_m;
        right_score = death_d;
      } else {
        left_score = death_d;
        right_score = death_m;
      }
      break;
    case SCORE_MODE_DEATH_YEAR:
      left_score = 20;
      right_score = death_y;
      break;
    case SCORE_MODE_ALARM:
      left_score = alarm_h;
      right_score = alarm_m;
      break;
    case SCORE_MODE_DEATH_ALARM:
      results = minutes_left;
      if((time_h > alarm_h) || ((time_h == alarm_h) && (time_m > alarm_m)))
        results -= (((((alarm_h * 60) + alarm_m) + 1440) - ((time_h * 60) + time_m)) * ((dc_mode == DC_mode_sadistic)?4:1));
      else
        results -= ((((alarm_h * 60) + alarm_m) - ((time_h * 60) + time_m)) * ((dc_mode == DC_mode_sadistic)?4:1));
      left_score = results / 10000;
      right_score = results % 10000;
      break;
  }
}

void initanim(void) {
}

void initdisplay(uint8_t inverted) {

  glcdFillRectangle(0, 0, GLCD_XPIXELS, GLCD_YPIXELS, inverted);
  
	setscore();

  // time
  if((score_mode != SCORE_MODE_DEATH_TIME) && (score_mode != SCORE_MODE_DEATH_ALARM))
  {
	if ((time_format == TIME_12H) && ((score_mode == SCORE_MODE_TIME) || (score_mode == SCORE_MODE_ALARM)))
		drawbigdigit(DISPLAY_H10_X, DISPLAY_TIME_Y, ((left_score + 23)%12 + 1)/10, inverted);
    else 
      drawbigdigit(DISPLAY_H10_X, DISPLAY_TIME_Y, left_score/10, inverted);
  
	if ((time_format == TIME_12H) && ((score_mode == SCORE_MODE_TIME) || (score_mode == SCORE_MODE_ALARM)))
		drawbigdigit(DISPLAY_H1_X, DISPLAY_TIME_Y, ((left_score + 23)%12 + 1)%10, inverted);
    else
      drawbigdigit(DISPLAY_H1_X, DISPLAY_TIME_Y, left_score%10, inverted);
  
    drawbigdigit(DISPLAY_M10_X, DISPLAY_TIME_Y, right_score/10, inverted);
    drawbigdigit(DISPLAY_M1_X, DISPLAY_TIME_Y, right_score%10, inverted);
  }
  else
  {
    drawbigdigit(DISPLAY_DL1_X, DISPLAY_TIME_Y, left_score / 1000, inverted);
    drawbigdigit(DISPLAY_DL2_X, DISPLAY_TIME_Y, (left_score % 1000) / 100, inverted);
    drawbigdigit(DISPLAY_DL3_X, DISPLAY_TIME_Y, (left_score % 100) / 10, inverted);
    drawbigdigit(DISPLAY_DL4_X, DISPLAY_TIME_Y, left_score % 10, inverted);
    
	drawbigdigit(DISPLAY_DR1_X, DISPLAY_TIME_Y, right_score / 1000, inverted);
    drawbigdigit(DISPLAY_DR2_X, DISPLAY_TIME_Y, (right_score % 1000) / 100, inverted);
    drawbigdigit(DISPLAY_DR3_X, DISPLAY_TIME_Y, (right_score % 100) / 10, inverted);
    drawbigdigit(DISPLAY_DR4_X, DISPLAY_TIME_Y, right_score % 10, inverted);
  }

  //drawmidline(inverted);
}


void step(void) {
	if(minute_changed || hour_changed)
	{
	  redraw_time = 1;
      minute_changed = hour_changed = 0;
	  setscore();
    }
}


void draw(uint8_t inverted) {
   // draw time
   volatile uint8_t redraw_digits = 0;
   TIMSK2 = 0;	//Disable Timer 2 interrupt, to prevent a race condition.
   if(redraw_time)
   {
   	   if(redraw_time == 2)
   	     initdisplay(inverted);
   	   else
   	     redraw_digits = 1;
   	   redraw_time = 0;
   }
   TIMSK2 = _BV(TOIE2); //Race issue gone, renable.
    
    // redraw 10's of hours
  if((score_mode != SCORE_MODE_DEATH_TIME) && (score_mode != SCORE_MODE_DEATH_ALARM))
  {
    if (redraw_digits) {
      
			if ((time_format == TIME_12H) && ((score_mode == SCORE_MODE_TIME) || (score_mode == SCORE_MODE_ALARM)))
				drawbigdigit(DISPLAY_H10_X, DISPLAY_TIME_Y, ((left_score + 23)%12 + 1)/10, inverted);
      else 
	drawbigdigit(DISPLAY_H10_X, DISPLAY_TIME_Y, left_score/10, inverted);
    }
    
    // redraw 1's of hours
    if (redraw_digits) {
			if ((time_format == TIME_12H) && ((score_mode == SCORE_MODE_TIME) || (score_mode == SCORE_MODE_ALARM)))
				drawbigdigit(DISPLAY_H1_X, DISPLAY_TIME_Y, ((left_score + 23)%12 + 1)%10, inverted);
      else
	drawbigdigit(DISPLAY_H1_X, DISPLAY_TIME_Y, left_score%10, inverted);
    }

    if (redraw_digits) {
      drawbigdigit(DISPLAY_M10_X, DISPLAY_TIME_Y, right_score/10, inverted);
    }
    if (redraw_digits) {
      drawbigdigit(DISPLAY_M1_X, DISPLAY_TIME_Y, right_score%10, inverted);
    }
  }
  else
  {
  	
  	if (redraw_digits) {
  		drawbigdigit(DISPLAY_DL1_X, DISPLAY_TIME_Y, left_score / 1000, inverted);
  	}
  	
  	if (redraw_digits) {
  		drawbigdigit(DISPLAY_DL2_X, DISPLAY_TIME_Y, (left_score % 1000) / 100, inverted);
  	}
  	if (redraw_digits) {
  		drawbigdigit(DISPLAY_DL3_X, DISPLAY_TIME_Y, (left_score % 100) / 10, inverted);
  	}
  	if (redraw_digits) {
  		drawbigdigit(DISPLAY_DL4_X, DISPLAY_TIME_Y, left_score % 10, inverted);
  	}
  	
  	if (redraw_digits) {
  		drawbigdigit(DISPLAY_DR1_X, DISPLAY_TIME_Y, right_score / 1000, inverted);
  	}
  	if (redraw_digits) {
  		drawbigdigit(DISPLAY_DR2_X, DISPLAY_TIME_Y, (right_score % 1000) / 100, inverted);
  	}
  	if (redraw_digits) {
  		drawbigdigit(DISPLAY_DR3_X, DISPLAY_TIME_Y, (right_score % 100) / 10, inverted);
  	}
  	if (redraw_digits) {
  		drawbigdigit(DISPLAY_DR4_X, DISPLAY_TIME_Y, right_score % 10, inverted);
  	}
  }
    redraw_digits = 0;
    
}

uint8_t intersectrect(uint8_t x1, uint8_t y1, uint8_t w1, uint8_t h1,
		      uint8_t x2, uint8_t y2, uint8_t w2, uint8_t h2) {
  // yer everyday intersection tester
  // check x coord first
  if (x1+w1 < x2)
    return 0;
  if (x2+w2 < x1)
    return 0;

  // check the y coord second
  if (y1+h1 < y2)
    return 0;
  if (y2+h2 < y1)
    return 0;

  return 1;
}

// 8 pixels high
static unsigned char __attribute__ ((progmem)) BigFont[] = {
	0xFF, 0x81, 0x81, 0xFF,// 0
	0x00, 0x00, 0x00, 0xFF,// 1
	0x9F, 0x91, 0x91, 0xF1,// 2
	0x91, 0x91, 0x91, 0xFF,// 3
	0xF0, 0x10, 0x10, 0xFF,// 4
	0xF1, 0x91, 0x91, 0x9F,// 5
	0xFF, 0x91, 0x91, 0x9F,// 6
	0x80, 0x80, 0x80, 0xFF,// 7
	0xFF, 0x91, 0x91, 0xFF,// 8 
	0xF1, 0x91, 0x91, 0xFF,// 9
};


void drawbigdigit(uint8_t x, uint8_t y, uint8_t n, uint8_t inverted) {
  uint8_t i, j;
  
  for (i = 0; i < 4; i++) {
    uint8_t d = pgm_read_byte(BigFont+(n*4)+i);
    for (j=0; j<8; j++) {
      if (d & _BV(7-j)) {
	glcdFillRectangle(x+i*2, y+j*2, 2, 2, !inverted);
      } else {
	glcdFillRectangle(x+i*2, y+j*2, 2, 2, inverted);
      }
    }
  }
}

#define DIGIT_WIDTH 28
#define DIGIT_HEIGHT 64

void bitblit_ram(uint8_t x_origin, uint8_t y_origin, uint8_t *bitmap_p, uint8_t size, uint8_t inverted) {
  uint8_t x, y, p;

  for (uint8_t i = 0; i<size; i++) {
    p = bitmap_p[i];

    x = i % DIGIT_WIDTH;
    if (x == 0) {
      y = i / DIGIT_WIDTH;
      glcdSetAddress(x+x_origin, (y_origin/8)+y);
    }
    if (inverted) 
      glcdDataWrite(~p);  
    else 
      glcdDataWrite(p);  
  }
}

// number of segments to expect
#define SEGMENTS 2

void blitsegs_rom(uint8_t x_origin, uint8_t y_origin, PGM_P bitmap_p, uint8_t height, uint8_t inverted) {
  uint8_t bitmap[DIGIT_WIDTH * DIGIT_HEIGHT / 8] = {0}, i,j;

  uint16_t pointer=0;
  for (uint8_t line = 0; line<height; line++) {
    uint8_t count = pgm_read_byte(bitmap_p+pointer);
    pointer++;
    while(count--) {
      uint8_t start = pgm_read_byte(bitmap_p+pointer);pointer++;
      uint8_t stop = pgm_read_byte(bitmap_p+pointer);pointer++;
    
      while (start <= stop) {
	    bitmap[start + (line/8)*DIGIT_WIDTH ] |= _BV(line%8);
	    start++;
      }
    }
  }
  bitblit_ram(x_origin, y_origin, bitmap, DIGIT_HEIGHT*DIGIT_WIDTH/8, inverted);
}
      
