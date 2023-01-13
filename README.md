# SignalBox

Signalbox is an implementation of the friendship box.
The idea is a simple one: people you're close to all have a transparent box with an esp8266 connected to some LED lights.
When you're thinking of the person, but don't have anything in particular to say in a text or call, you can simply select their color on your box. This will send a message to their box to light up as your box's color! 

The project uses the free tier of HiveMQ MQTT, and the devices run on the (admittedly overkill for this project) Arduino Feather Huzzah esp8266 devboard.
To make this project I used [this (dead link) starter guide for hiveMQ](https://console.hivemq.cloud/clients/arduino-esp8266?uuid=c689cd9ab0cf406bbc142988277304d9). 
Special thanks to Danielle Thurow's blog [Duct Tape, Bubble Gum, and Solder](https://daniellethurow.com/blog/2021/9/8/interrupts-with-adafruit-feather-huzzah-esp8266) for making sense of interrupts with this device.
Also, [this thread](https://playground.arduino.cc/Linux/All/#Permission) was super helpful in troubleshooting Arduino IDE issues on Linux.

Apologies for nonexistent documentation: I sprinted to get this built before Christmas for a family gift. If any is requested I'd be more than happy to come back and write some more involved documentation.
