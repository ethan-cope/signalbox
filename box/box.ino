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

const char* mqtt_server = "a3c34c8bb30e4eb6964060e9263f41ff.s2.eu.hivemq.cloud";

//colors is an array of strings
const char numColors = 4;
//[number of strings][length of strings]
char colors[numColors][4] = {"ltr", "grn", "blu", "dkr"};
uint8_t colorHSVVals[numColors] = {322, 83, 129, 0};
int colorIndex = 0;

//TODO paralell array of neopixel colors goes here

CRGB leds[NUM_LEDS];

//set value of LEDs will always be the same
//need a 1d array for default value and a 2d array for diffs.
//should be unsigned char, so rolls over after 255. 

uint8_t current_color_hue = 83;
uint8_t default_saturation = 255;
uint8_t default_brightness = MAX_BRIGHTNESS;
int animNum = 0;
int16_t ledValDiffs[NUM_LEDS][3];

//[NON-CONSTANTS]
const char* ssid = "90125";
const char* password = "Baylortamu19";
const int myColorIndex = 1;

//[WIFI STUFF]

// A single, global CertStore which can be used by all connections.
// Needs to stay live the entire time any of the WiFiClientBearSSLs
// are present.
BearSSL::CertStore certStore;

WiFiClientSecure espClient;
PubSubClient * client;
unsigned long lastMsg = 0;
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
  //this is where we turn on the color for a little bit.
}


void reconnect() {
  // Loop until we’re reconnected
  snprintf (connmsg, MSG_BUFFER_SIZE, "%s device online!", colors[myColorIndex]);
  while (!client->connected()) {
    Serial.print("Attempting MQTT connection…");
    String clientId = "ESP8266Client - MyClient";
    // Attempt to connect
    // Insert your password
    // change this for when other people connect their devices!
    if (client->connect(clientId.c_str(), "boxname", "Chicken-Fajita")) {
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

//[LED STUFF]

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

    //this one had a part where I parsed backwards to stop rollover.
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
  // When opening the Serial Monitor, select 9600 Baud
  Serial.begin(9600);
  delay(500);
  LittleFS.begin();
  setup_wifi();
  setDateTime();
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_BUILTIN, OUTPUT); // Initialize the LED_BUILTIN pin as an output

  //lighting setup
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  for(int u = 0; u< NUM_LEDS; u++){
    an1CtremSigns[u]=1;
  }
  resetLED(false);

  //certs stuff

  // you can use the insecure mode, when you want to avoid the certificates
  // espclient->setInsecure();
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

bool clicked = false;
int butval = 0;
int minterval = 250;
int pressTimeout = 0;
unsigned long onTime = 0; //in milliseconds
unsigned long onInterval = 10 * 1000; // provide left number in seconds for ease of use.

void loop() {
  if (!client->connected()) {
    reconnect();
  }
  client->loop();

  unsigned long rn = millis();
  if (rn - lastMsg > minterval) {
    lastMsg = rn;
    ++value;

    butval = digitalRead(BUTTON_PIN);

    //when button is let go of after click. this latches to an actual click.
    if(butval == 1 && clicked){
      clicked = false;
      //increment colorindex. 
      ++colorIndex;

      // when button pressed, better polling  resolution.
      minterval = 100;
      pressTimeout = 30;

      // if it's your color, increment again
      if(colorIndex == myColorIndex)
        ++colorIndex;

      // if it's past end, go to 0
      if(colorIndex >= numColors)
        colorIndex = 0;

      
	    current_color_hue = colorHSVVals[colorIndex];
      resetLED(true);
      Serial.println(colors[colorIndex]);
    }
    else if(butval == 0){
      clicked = true;

    }

    //pressTimeout will just hang at -2 
    if (pressTimeout > -2){
      --pressTimeout;
    }

    if (pressTimeout == 0){
      //snprintf (info, MSG_BUFFER_SIZE, "Publishing %s to %s", msg, colors[colorIndex]);
      //Serial.println(info);
      snprintf (msg, MSG_BUFFER_SIZE, "%s->%s", colors[myColorIndex], colors[colorIndex]);
      client->publish(colors[colorIndex], msg);
      minterval = 500;
    }

    /*
    snprintf (msg, MSG_BUFFER_SIZE, "hello world #%ld", value);
    Serial.print("Publish message: ");
    Serial.println(msg);
    client->publish("testTopic", msg);
    */
  }
}
