#include <FastLED.h>
#include <time.h>

#define BUTTON_PIN 4
#define NUM_LEDS 7
#define DATA_PIN 5
#define MAX_BRIGHTNESS 100
#define CTREM_VAL 7
#define VTREM_VAL 20

// LED Status Variables
CRGB leds[NUM_LEDS];
int16_t ledValDiffs[NUM_LEDS][3];
const char numColors = 4;
uint8_t colorHSVVals[numColors] = {235, 83, 129, 0};
const volatile int myColorIndex = 4;


// Current State Variables
uint8_t current_color_hue = 129;
uint8_t default_saturation = 255;
uint8_t default_brightness = MAX_BRIGHTNESS;

volatile int colorIndex = 0;
int animNum = 0;



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
    //no animation. stay what it is already.
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
    
    if (done){
      return 1; //when complete
    }
  }
  return 0;
}

//interrupt stuff
volatile unsigned long lastPressTS = 0; //timestamp of last button press
volatile bool btnISRFlag = false; //flag that stays on until cleared by ISR. 
ICACHE_RAM_ATTR void pin_ISR(){
  // can't use millis()

  if (millis() - lastPressTS > 10){ 
    // 10 is debounce time in mS: if our difference is > debounce, it's legit. 
    lastPressTS = millis();
    btnISRFlag = true;
    
    //changing color here.
    ++colorIndex;
    if(colorIndex == myColorIndex){
      ++colorIndex;
    }
    if(colorIndex >= numColors){
      colorIndex = 0;
    }
    current_color_hue = colorHSVVals[colorIndex];
    
    resetLED(true);
    //toggle = !toggle;
  }
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
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), pin_ISR, RISING);
  resetLED(false);
  animNum = 0;
}


unsigned long lastLoop = 0;
unsigned long lastSentMsg = 0;
volatile bool msgSent = false;
unsigned long pollInterval = 250;
unsigned long sendInterval = 4 * 1000;  // how long before we send a message after recipient selection
unsigned long longInterval = 10 * 1000; // how long in seconds the lantern stays on

void loop() { 
  unsigned long rn = millis();
  if (rn - lastLoop > pollInterval){
    lastLoop = rn;
    /*
    if(btnISRFlag){
      lastMsg = rn;
      
    }*/
    
    if(btnISRFlag && rn - lastPressTS >= sendInterval){
      //do a bunch of stuff (send message, set long TS, etc)
      msgSent = true;
      lastSentMsg = millis();
      btnISRFlag = false;
      
      //flash message confirmation
      animNum = 1;
      resetLED(false);
      delay(250);
      resetLED(true);

    }

    else if (rn - lastSentMsg > longInterval && msgSent){
      // this is too jumpy. triggers every single time when out of interval.
      // sometimes cuts off interrupt and forces reset to false.
      //turning off after long interval
      Serial.print(lastSentMsg);
      Serial.print(" : ");
      Serial.print(rn);
      Serial.print("\n");
      animNum = 0;
      resetLED(false);
      msgSent = false;
    }
    animate(animNum);
  }
}
