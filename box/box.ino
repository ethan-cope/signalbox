#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <time.h>
#include <TZ.h>
#include <FS.h>
#include <LittleFS.h>
#include <CertStoreBearSSL.h>
#include <FastLED.h>
#include <time.h>

//[CONSTANTS]
#define BUTTON_PIN 4
#define NUM_LEDS 7
#define DATA_PIN 5
#define MAX_BRIGHTNESS 100
#define CTREM_VAL 7
#define VTREM_VAL 20
const char* mqtt_server = "SERVER_ID";

// [LED STATUS VARS] 
CRGB leds[NUM_LEDS];
const char numColors = 4;
char colors[numColors][4] = {"ltr", "grn", "blu", "dkr"};
uint8_t colorHSVVals[numColors] = {235, 83, 129, 0};
const volatile int myColorIndex = 2;

// [LED STATE VARS]
volatile int colorIndex = 0;
uint8_t current_color_hue = 129;
uint8_t default_saturation = 255;
uint8_t default_brightness = MAX_BRIGHTNESS;
int16_t ledValDiffs[NUM_LEDS][3];
int animNum = 0;

//[WIFI STUFF]

const char* ssid = "WIFI_SSID";
const char* password = "WIFI_PW";

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

// [ANIMATE BLOCK] 

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

// [INTERRUPT BLOCK]

volatile unsigned long lastPressTS = 0;
volatile bool btnISRFlag = false;
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

// [RECONNECTION HELPER FUNCTION]

void reconnect() {
  // Loop until we’re reconnected
  snprintf (connmsg, MSG_BUFFER_SIZE, "%s device online!", colors[myColorIndex]);
  while (!client->connected()) {
    Serial.print("Attempting MQTT connection…");
    String clientId = "ESP8266Client - MyClient";
    // Attempt to connect
    // Insert your password
    // change this for when other people connect their devices!
    if (client->connect(clientId.c_str(), "BOX_NAME", "SERVER_PW")) {
      Serial.println("connected");
      // Once connected, publish an announcement…
      client->publish("general", connmsg);
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

// [PROGRAM RUN]

void setup() {
  delay(500);
  // When opening the Serial Monitor, select 9600 Baud
  Serial.begin(9600);
  delay(500);

  //wifi setup
  LittleFS.begin();
  setup_wifi();
  setDateTime();

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
    /*
    snprintf (msg, MSG_BUFFER_SIZE, "hello world #%ld", value);
    Serial.print("Publish message: ");
    Serial.println(msg);
    client->publish("testTopic", msg);
    */

unsigned long lastLoop = 0;             // timestamp for last time we looped
unsigned long lastMsg = 0;              // timestamp for last message
volatile bool msgOut = false;           // message sent flag
unsigned long minterval = 250;          // polling interval
unsigned long sendInterval = 4 * 1000;  // How long after select before send
unsigned long longInterval = 10 * 1000; // How long the Lamp is on

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

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

  msgOut = true;
  lastMsg = millis();

  //we need to get this junk and find out what color to make the leds
  colorIndex = (char)payload[4];

  animNum = 1;
  resetLED(true);
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
      //send message, set long timestamp, etc
      msgOut = true;
      lastMsg = millis();
      btnISRFlag = false;

      Serial.print(colors[myColorIndex]);
      snprintf (msg, MSG_BUFFER_SIZE, "%s(%d)->%s", colors[myColorIndex], myColorIndex, colors[colorIndex]);
      client->publish(colors[colorIndex], msg);
      
      //flash to indicate message has been sent
      animNum = 1;
      resetLED(false);
      delay(250);
      resetLED(true);
    }

    else if (rn - lastMsg > longInterval && msgOut){
      //turning off after long interval, whether we sent or recvd
      Serial.print(lastMsg);
      Serial.print(" : ");
      Serial.print(rn);
      Serial.print("\n");
      animNum = 0;
      resetLED(false);
      msgOut = false;
    }
    animate(animNum);
  }
}
