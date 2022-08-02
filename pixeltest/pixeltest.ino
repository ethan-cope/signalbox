
#include <FastLED.h>
#include <time.h>

#define BUTTON_PIN 4

#define NUM_LEDS 7
#define DATA_PIN 5
#define MAX_BRIGHTNESS 100
#define CTREM_VAL 7
#define VTREM_VAL 20

CRGB leds[NUM_LEDS];

//set value of LEDs will always be the same
//need a 1d array for default value and a 2d array for diffs.
//should be unsigned char, so rolls over after 255. 

uint8_t current_color_hue = 83;
uint8_t default_saturation = 255;
uint8_t default_brightness = MAX_BRIGHTNESS;
int animNum = 0;

int16_t ledValDiffs[NUM_LEDS][3];

void status(){
  //Serial.print(NUM_LEDS);
  Serial.print("[animNum: ");
  Serial.print(animNum);
  Serial.print("]\n");
  Serial.print("Defaults:\n");
  Serial.print(current_color_hue); Serial.print(" | ");
  Serial.print(default_saturation); Serial.print(" | ");
  Serial.print(default_brightness); Serial.print("\n\n");

  for(int i = 0; i< NUM_LEDS; i++){
    Serial.print("(");
    Serial.print(ledValDiffs[i][0]);
    Serial.print(",");
    Serial.print(ledValDiffs[i][1]);
    Serial.print(",");
    Serial.print(ledValDiffs[i][2]);
    Serial.print(")\n");
  }
}

void resetLED(bool lightsOn){
  //lightsOn = false: starts low to animate up
  //lightsOn = true: starts high 
  for (int i = 0; i<NUM_LEDS; i++){
    ledValDiffs[i][0] = 0;
    ledValDiffs[i][1] = 0;
    if (lightsOn) {
      leds[i] = CHSV( current_color_hue, default_saturation, MAX_BRIGHTNESS);
      ledValDiffs[i][2] = 0;
    }
    else {
      leds[i] = CHSV( current_color_hue, default_saturation, 0);
      ledValDiffs[i][2] = -1*MAX_BRIGHTNESS;
    }
  }
  //status();
  FastLED.show();
}

// here are the animation-specific global variables 

int8_t an1CtremSigns[NUM_LEDS];
int8_t an1VtremSign = 1;

int8_t an5LedNum = 0;
int8_t an6LedNum = -1;

int hnum;
int snum;
int vnum; 

int animate(int selection){
  hnum = 0; snum = 0; vnum = 0;
  FastLED.show();
  if(selection == 0){
    //no animation. constantly set to nothing.
    return 0;
  }
  else if (selection == 1){
    //default color / value tremolo animation. 
    //only call this if colors are already on. if they aren't, you'll need to resetLED.

    //value trem
    if (ledValDiffs[0][2] > VTREM_VAL){
      an1VtremSign = -1;
    }
    else if(ledValDiffs[0][2] < -1*VTREM_VAL){
      an1VtremSign = 1;
    }
    vnum = an1VtremSign*2;
    
    for (int i = 0; i < NUM_LEDS; i++){
      //color tremolo
      if (ledValDiffs[i][0] > CTREM_VAL){
        an1CtremSigns[i] = -1;
      }
      else if(ledValDiffs[i][0] < -1*CTREM_VAL){
        an1CtremSigns[i] = 1;
      }
      hnum = an1CtremSigns[i]*random(3);
      ledValDiffs[i][0]+= hnum;
      ledValDiffs[i][2]+=vnum;
      leds[i] = CHSV(current_color_hue + ledValDiffs[i][0], default_saturation, default_brightness+ledValDiffs[i][2]);   
    }
    //status();
  }
  else if (selection == 5){
    //this fade in is hella broken. ignore for now, since animation 1 fades in.
    //maybe it's broken cause I never show the changes :/
    int actval = 0;
    int expval = 0;
    int increment = default_brightness / NUM_LEDS; 
    bool done = true;
    //assumes off. 
    //fade in. use log2 in a row.
    //an5LedNum
    for(int i = NUM_LEDS - 1; i > 0; i++){
      if(i >= an5LedNum){
        if (ledValDiffs[i][2] == -1*default_brightness){
          ledValDiffs[i][2] = -1*default_brightness +2;
          done = false;
        }
        else if(-1*ledValDiffs[i][2] < default_brightness){
          ledValDiffs[i][2] += increment; 
          done = false;
        }
        else if(ledValDiffs[i][2] > 0){
          ledValDiffs[i][2] = 0;
        }
      }
    }
    if(done)
    {
      return 1;
    }
    //status();

    //this one had a part where I parsed backwards for some reason.
  }
    else if (selection == 6){
    //fade out. random.
    //if the pin's value isn't off, fade that pin out incrementally and randomly.
    //once faded out / pin invalid, you need to pick another random.
    //YES this is the bogosort method, but screw you it works ok for small values
    bool done = true;
    if (an6LedNum < 0){
      //pick a new an6LedNum
      an6LedNum = random(NUM_LEDS);
    }
    else if(-1*ledValDiffs[an6LedNum][2] >= default_brightness){
      //pick a new an6LedNum
      ledValDiffs[an6LedNum][2] = -1*default_brightness;
      an6LedNum = random(NUM_LEDS);
    }
    else{
      ledValDiffs[an6LedNum][2] -= 7;
      //when it goes over it flashes. kinda cool tbh
    }

    for (int i = 0; i < NUM_LEDS; i++){  
      if (ledValDiffs[i][2] != -1*default_brightness){
        done = false;
      }
      leds[i] = CHSV(current_color_hue + ledValDiffs[i][0], default_saturation, default_brightness+ledValDiffs[i][2]);
    }
    
    //status();

    if (done){
      return 1; //when complete
    }

  }
  return 0;
}

void setup() { 
  delay(500);
  Serial.begin(9600); //setting up debug output
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  delay(500);

  //seeding for the color tremolo
  for(int u = 0; u< NUM_LEDS; u++){
    an1CtremSigns[u]=1;
  }

  pinMode(BUTTON_PIN, INPUT_PULLUP);
  resetLED(false);
}

unsigned long lastMsg = 0;
unsigned long minterval = 250;
int butval = 0;
unsigned long onTime = 0; //in milliseconds
unsigned long onInterval = 10 * 1000; // provide left number in seconds for ease of use.

void loop() { 
  unsigned long rn = millis();
  if (rn - lastMsg > minterval){
    butval = digitalRead(BUTTON_PIN);

    if(butval == 0){
      //sets to fade in.
      animNum = 1;
      onTime = millis();
      Serial.print("\n\nPRESSED!!!!!\n\n");
    }
    
    if(animate(animNum) == 1){
      //this only returns 1 if we're done fading out. 
      resetLED(false);
      animNum = 0;
    }
    
    else if(rn - onTime > onInterval && animNum == 1)
    {
      //start fading out. 
      animNum = 6;
    }
    
    else if(animNum != 0){
      status();
    }
    
    //status();
    lastMsg = rn;

    /*
    if(animate(animNum) == 1){
      //now all that's left is finding a way to program in lengths of time before fadeout.
      //status();
      animNum = 0;
      resetLED(false);
    }
    */
  }

}
