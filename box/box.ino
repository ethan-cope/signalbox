#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <time.h>
#include <TZ.h>
#include <FS.h>
#include <LittleFS.h>
#include <CertStoreBearSSL.h>
#include <FastLED.h>
#include <time.h>
#include "info.h" // REMOVE THIS LINE, UNCOMMENT LINES BELOW AFTER ADDING YOUR INFO 

// const char* mqtt_server = "YOUR_MQTT_SERVER_HERE";
// const char* pw = "YOUR_MQTT_PASSWORD_HERE";
// const char* user_defined_ssid = "YOUR_WIFI_NETWORK_HERE";
// const char* user_defined_wifipass = "YOUR_WIFI_PASSWORD_HERE";

//[CONSTANTS]
#define BUTTON_PIN 4
#define NUM_LEDS 7
#define DATA_PIN 5
#define MAX_BRIGHTNESS 100
#define CTREM_VAL 7
#define VTREM_VAL 20

// [DEBUG MODE]
bool debugActive = true;

// [LED STATUS VARS] 
CRGB leds[NUM_LEDS];
const char numColors = 4;
char colors[numColors][4] = {"ltr", "grn", "blu", "dkr"};
uint8_t colorHSVVals[numColors] = {235, 83, 129, 0};
const volatile int myColorIndex = 1;

// [LED STATE VARS]
volatile int colorIndex = 0;    // The current color of the box 
volatile int lastRecvIndex = -1; // the last received color (used to determine two-tone animations)
uint8_t current_color_hue = 129;
uint8_t default_saturation = 255;
uint8_t default_brightness = MAX_BRIGHTNESS;
int16_t ledValDiffs[NUM_LEDS][3];
int animNum = 0;

//[WIFI STUFF]

const char* ssid = user_defined_ssid;         // PUT NETWORK NAME HERE
const char* password = user_defined_wifipass; // PUT NETWORK PW HERE

// A single, global CertStore which can be used by all connections.
// Needs to stay live the entire time any of the WiFiClientBearSSLs
// are present.
BearSSL::CertStore certStore;

WiFiClientSecure espClient;
PubSubClient * client;
#define MSG_BUFFER_SIZE (500)
char msg[MSG_BUFFER_SIZE];
char connmsg[MSG_BUFFER_SIZE];
int value = 0;

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void setDateTime() {
  // You can use your own timezone, but the exact time is not used at all.
  // Only the date is needed for validating the certificates.
  configTime(TZ_Europe_Berlin, "pool.ntp.org", "time.nist.gov");

  Serial.print("Waiting for NTP time sync: ");
  time_t now = time(nullptr);
  while (now < 8 * 3600 * 2) {
    delay(100);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println();

  struct tm timeinfo;
  gmtime_r(&now, &timeinfo);
  Serial.printf("%s %s", tzname[0], asctime(&timeinfo));
}

// [ANIMATION FUNCTIONS]

void status(){
  //Serial.print(NUM_LEDS);
  sprintf(msg, "### STATUS ###\n[animNum: %d ]\nDefaults:\n%d | %d | %d\n\n", animNum, current_color_hue, default_saturation, default_brightness);
  Serial.print(msg);

  for(int i = 0; i< NUM_LEDS; i++){
    sprintf(msg, "( %d, %d, %d )\n", ledValDiffs[i][0], ledValDiffs[i][1], ledValDiffs[i][2]);
    Serial.print(msg);
  }
  Serial.println();
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
  FastLED.show();
}

// [ANIMATE BLOCK] 
// It's easier to leave them globals then try to keep track of passing them around.

int8_t an1CtremSigns[NUM_LEDS];
int8_t an1VtremSign = 1;

int8_t an5LedNum = 0;
int8_t an6LedNum = -1;

int animate(int selection){
  int hnum;
  int snum;
  int vnum; 

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
  }
  else if (selection == 2){
    // this is the "alternate between two colors" setting
    
    // ways to access: 
    // - if we've sent a color, and gotten it back.
    //   - how to tell if this happened - our animation ISN'T BLACK and we RECEIVE the same color we sent. 
    //   - in this case, our box already knows the right color to alternate between
    // - if we've recieved a color, and sent one out. 

    uint8_t gradientLength;
    uint8_t startpoint;

    if(colorHSVVals[myColorIndex] - current_color_hue == 0){
      Serial.print("[ERR.] Cannot form gradient: sent and recv color are the same.");
      return 0;
    }
    else if(current_color_hue < colorHSVVals[myColorIndex]){
      startpoint = current_color_hue;
      gradientLength = colorHSVVals[myColorIndex] - current_color_hue;
    }
    else{
      startpoint = colorHSVVals[myColorIndex];
      gradientLength = current_color_hue - colorHSVVals[myColorIndex]; 
    } 

    // how much the gradient changes before reaching the next color.
    // formula is i**2 * scalefactor
    uint8_t scalefactor = 3;
    
    for(int i = 0; i < NUM_LEDS/2 + 1; i++){
      // BTW, we use the CTREM_SIGNS array for the VALUE Tremolo here. Because I don't want more global variables.
      // If someone comes up with a better way to do tremolo, please make a PR!
          
      //value tremolo (bottom)
      if (ledValDiffs[i][2] > VTREM_VAL){
        an1CtremSigns[i] = -1;
      }
      else if(ledValDiffs[i][2] < -1*VTREM_VAL){
        an1CtremSigns[i] = 1;
      }
      vnum = an1CtremSigns[i]*random(3);
      ledValDiffs[i][2]+=vnum;

      //value tremolo (top)
      if (ledValDiffs[NUM_LEDS-1-i][2] > VTREM_VAL){
        an1CtremSigns[NUM_LEDS-1-i] = -1;
      }
      else if(ledValDiffs[NUM_LEDS-1-i][2] < -1*VTREM_VAL){
        an1CtremSigns[NUM_LEDS-1-i] = 1;
      }
      // USES HNUM - I don't want to make a new variable. 
      hnum = an1CtremSigns[NUM_LEDS-1-i]*random(3);
      ledValDiffs[NUM_LEDS-1-i][2]+=hnum;
      
      // populate gradient here
      // gradient moves towards i**2. 
      leds[0+i]          = CHSV(startpoint + scalefactor * i*i, default_saturation, default_brightness + ledValDiffs[i][2]);
      //if odd number, above overwritten by below. 
      leds[NUM_LEDS-1-i] = CHSV(startpoint+gradientLength - scalefactor * i*i, default_saturation, default_brightness + ledValDiffs[NUM_LEDS-1-i][2]);
    }
  }
  
  else if (selection == 6){
    // currently unused.
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

// [INTERRUPT BLOCK]

volatile unsigned long lastPressTS = 0;
volatile bool btnISRFlag = false;
ICACHE_RAM_ATTR void pin_ISR(){
  // interrupt that handles button presses
  // can't use millis()

  if (millis() - lastPressTS > 10){ 
    // 10 is debounce time in mS: if our difference is > debounce, it's legit. 
    lastPressTS = millis();
    animNum = 1; // reset animnum, so we don't have double colors when clicking through
    btnISRFlag = true;
    
    //changing color here.
    ++colorIndex;
    //beginning overflow
    if(colorIndex >= numColors){
      colorIndex = 0;
    }
    if(colorIndex == myColorIndex){
      ++colorIndex;
    }
    //ending overflow
    if(colorIndex >= numColors){
      colorIndex = 0;
    }
    current_color_hue = colorHSVVals[colorIndex];
    
    resetLED(true);
  }
}

// [RECONNECTION HELPER FUNCTION]

void reconnect() {
  // Loop until we’re reconnected
  snprintf (connmsg, MSG_BUFFER_SIZE, "[RCON] [%s] device online.", colors[myColorIndex]);
  while (!client->connected()) {
    Serial.print("Attempting MQTT connection…");
    // Attempt to connect
    // Insert your password
    // change this for when other people connect their devices!

    char uName[] = "";
    sprintf(uName, "%sbox", colors[myColorIndex]);
    // Working!
    
    // this should be colors[myColorIndex]box
    String clientId = "ESP8266Client - ";
    clientId += colors[myColorIndex];
    clientId += "box";

    //Serial.println(clientId);
    
    // bluebox, dkrbox, greenbox, ltredbox
    Serial.println(uName);
    Serial.println(clientId);
    
    if (client->connect(clientId.c_str(), uName , pw)) {
      Serial.println("connected");
      // Once connected, publish an announcement…
      debugPrintMQTT(connmsg);
      // … and resubscribe
      client->subscribe(colors[myColorIndex]);
    } else {
      Serial.print("failed, rc = ");
      Serial.print(client->state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

// [DEBUG MODE MQTT POSTING]

void debugPrintMQTT(char* msg){
  if (debugActive){
    client->publish("debug", msg);
  }
}

// [PROGRAM RUN]

void setup() {
  delay(500);
  // When opening the Serial Monitor, select 9600 Baud
  Serial.begin(9600);
  delay(500);

  //lighting setup
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  for(int u = 0; u< NUM_LEDS; u++){
    an1CtremSigns[u]=1;
  }
  resetLED(false);
  animNum = 0;

  // interrupt setup
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(BUTTON_PIN), pin_ISR, RISING);

  //wifi setup
  LittleFS.begin();
  setup_wifi();
  setDateTime();

  //MQTT certificate setup
  int numCerts = certStore.initCertStore(LittleFS, PSTR("/certs.idx"), PSTR("/certs.ar"));
  Serial.printf("Number of CA certs read: %d\n", numCerts);
  if (numCerts == 0) {
    Serial.printf("No certs found. Did you run certs-from-mozilla.py and upload the LittleFS directory before running?\n");
    return; // Can't connect to anything w/o certs!
  }

  BearSSL::WiFiClientSecure *bear = new BearSSL::WiFiClientSecure();
  // Integrate the cert store with this connection
  bear->setCertStore(&certStore);
  client = new PubSubClient(*bear);
  client->setServer(mqtt_server, 8883);
  client->setCallback(callback);
}

// [HOW TO SEND A MESSAGE] 
    //snprintf (msg, MSG_BUFFER_SIZE, "%s->%s", colors[myColorIndex], colors[colorIndex]);
    //client->publish(colors[colorIndex], msg);
    
    //snprintf (msg, MSG_BUFFER_SIZE, "hello world #%ld", value);
    //Serial.print("Publish message: ");
    //Serial.println(msg);
    //client->publish("testTopic", msg);

unsigned long lastLoop = 0;             // timestamp for last time we looped
unsigned long lastMsg = 0;              // timestamp for last message
volatile bool msgOut = false;           // message sent flag
unsigned long minterval = 250;          // polling interval
unsigned long sendInterval = 4 * 1000;  // How long after select before send
unsigned long longInterval = 2 * 60 * 60 * 1000; // How long the Lamp is on
//unsigned long longInterval = 15 * 1000; // DEBUG LAMP ON INTERVAL

void callback(char* topic, byte* payload, unsigned int length) {

  // output message to serial

  // we can't store this for now because I don't want to dive into string handling, i want to make something cool
  Serial.print("[RECV] Message arrived:");
  for (int i = 0; i < length; i++) {
    //payloadString[i] = (char)payload[i];
    Serial.print((char)payload[i]);
  }
  Serial.print("\n");

  // Switch on the LED if the first character is present
  if ((char)payload[0] != NULL) {
    digitalWrite(LED_BUILTIN, LOW); // Turn the LED on (Note that LOW is the voltage level
    // but actually the LED is on; this is because
    // it is active low on the ESP-01)
    delay(500);
    digitalWrite(LED_BUILTIN, HIGH); // Turn the LED off by making the voltage HIGH
  } else {
    digitalWrite(LED_BUILTIN, HIGH); // Turn the LED off by making the voltage HIGH
  }

  //we need to get this junk and find out what color to make the leds
  //sketchy conversion from ascii here
  
  if (((char)payload[4]-48) >=0 && ((char)payload[4]-48) < numColors){
    msgOut = true;
    lastMsg = millis();
    
    colorIndex = (char)payload[4]-48;


    // [gradient ANIMATION 2] 
    // if we've already sent a message to the box 
    // and we receive a message from the same one we sent to...

    // note: first message if lastRecvIndex isn't the same as the current color. 
    if((animNum == 1 || animNum ==2) && current_color_hue == colorHSVVals[colorIndex] && lastRecvIndex != colorIndex){
      Serial.print("[RECV] box responded!!\n");
      animNum = 2;
    }
    else{
      animNum = 1;
    }

    lastRecvIndex = colorIndex;    
    current_color_hue = colorHSVVals[colorIndex];

    //output message to serial...
    sprintf(msg, "[RECV] [%s] Setting Color to %d (%s) \n", colors[myColorIndex], (char)payload[4]-48, colors[colorIndex]);
    Serial.print(msg);
    // and to debug
    debugPrintMQTT(msg);
  }
  else{
    sprintf(msg, "[RECV] Invalid Color Index %d \n", (char)payload[4]-48);
    Serial.print(msg); 
  }
}

void loop() {
  if (!client->connected()) {
    reconnect();
  }
  client->loop();
  unsigned long rn = millis();
  if (rn - lastLoop > minterval){
    lastLoop = rn;
    
    if(btnISRFlag && rn - lastPressTS >= sendInterval){
      // if we get a button interrupt AND we've waited sendInterval seconds 
      //(AKA message is ready to be sent)...
      
      //send message, set long timestamp, etc
      msgOut = true;
      lastMsg = millis();
      btnISRFlag = false;

      // post message to the recipient box's client 
      snprintf (msg, MSG_BUFFER_SIZE, "%s(%d)->%s", colors[myColorIndex], myColorIndex, colors[colorIndex]);
      client->publish(colors[colorIndex], msg);
      
      //debug output on serial monitor...
      sprintf(msg, "[SEND] [%s] Sending Message (to %s)\n", colors[myColorIndex], colors[colorIndex]);
      Serial.print(msg);

      //and to MQTT
      debugPrintMQTT(msg);

      if((animNum == 1 || animNum == 2) && colorIndex == lastRecvIndex){
        // If we are sending a message, but we've received a message from that box already... 
        Serial.print("[SEND] in response to sent message!!!\n");

        animNum = 2;
      }
      else{
        animNum = 1;
      }
      
      //flash to indicate message has been sent
      resetLED(false);
      delay(250);
      resetLED(true);
    }

    else if (rn - lastMsg > longInterval && msgOut){
      //turning off after long interval, whether we sent or received 
      sprintf (msg, "[TOFF] Turning off after %d seconds. (%d : %d) \n", (rn-lastMsg)/1000, lastMsg/1000, rn/1000);
      Serial.print(msg);

      // no two-tone unless we send out again. 
      lastRecvIndex = -1;
      
      animNum = 0;
      resetLED(false);
      msgOut = false;
    }

    //status();
    animate(animNum);
  }
}
