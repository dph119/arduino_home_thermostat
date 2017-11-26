# arduino_home_thermostat
Code for controlling my home thermostat via a stepper motor, using information from a BMP180 temperature sensor
connected via ESP8266.

## Introduction

Where I live, 90% of the radiators are in one room, and the thermostat is in another room, far enough
away that I'm melting by the time the thermostat realizes it's time to turn off the heat.
There are way more practical solutions to this, and it's not that big a problem. But I decided
to create this Arduino setup where it can keep track of the temperature in the room
and adjust the thermostat accordingly.

The setup also made it easy to add support to adjust the thermostat however I want.

Based on the reasearch I did to try and get this to work, I figured it may be worth posting
this in-case others might find it useful for whatever they're trying to do.

## Materials
1. Two (2) Arduino Uno
    1. One of them could probably be some smaller version
2. Two (2) 9V DC Power Adaptors (for the Arduinos)
3. One (1) BMP180 Temperature/Pressure Sensor
4. Two (2) ESP8266 WiFi chips
    1. I specifically used [this one by TDB Controls](https://www.amazon.com/TBD-Controls-ESP8266-Microcontroller-Arduino/dp/B01MT6T73L)
5. One (1) NEMA17 Stepper Motor
    1. Probably overkill. But I specifically used [this one from Pololu](https://www.pololu.com/product/1200/)
6. One (1) wheel to mount on the stepper motor
7. One (1) 12V/3A DC Power Adaptor (for the Stepper Motor)
8. One (1) Stepper Motor Driver
    1. I used an [OSEPP MTD-01](https://www.rpelectronics.com/mtd-01.html)
9. Two (2) Enclosure boxes (one for each Arduino)
10. Misc. mounting equipment for both the motor and enclosure. Not going into it here.

## Organization
This whole thing is broken up into two parts: (1) the part controlling the thermostat, and (2) the part
keeping track of the temperature in the other room. 
### Thermostat/Main Server (thermostat_arduino.ino and thermostat_esp8266.ino)
The Arduino on here is connected to:

1. Stepper Motor for adjusting the thermostat
2. ESP8266 for communicating with the other Arduino

The thermostat is an old one of the [Honeywell T87F](https://customer.honeywell.com/resources/techlit/TechLitDocuments/60-0000s/60-2222.pdf) variety.
The heat is turned on when the circuit is "completed" by a mercury switch.

The idea was to have the wheel driven by the stepper motor shoved up against the thermostat.
I considered another approach where I tap right into the wires (e.g. add some relay) used by the 
thermostat to turn on the heat, but figured this approach would leave the door open to being able to do
stuff like program the thermostat.

The ESP8266 is setup in AP_STA mode. It connects to the other Arduino, but can also
serve up some primitive stuff and take requests. The Arduino will by default regularly query
the other guy for a temperature, keeping some history and an average temperature.
It also tracks what kind of adjustments it previously did. Based on all of this, it figures
out whether it is time to make an adjustment, and by how much.

It currently uses a static IP address. If I connect to the network on my phone or whatever, then
I can visit pages like:

    42.42.42.42/stherm?value=80

Which will have the ESP8266 send a command to the Arduino, which will then drive the stepper
motor to set the thermostat to 80 degrees fahrenheit. Yeah I could've setup up a DNS, but I didn't really care.

Current supported commands:

Command | Variable(s) | Description
--- | --- | ---
/rtherm | (none) | Reset the thermostat
/stherm | value | Set the thermostat to some specified value
/sdtemp | value | Set the 'desired' temperature to regulate toward
/enable | (none) | Enable the whole setup
/disable | (none) | Disable the whole setup
/ | (none) | Simply serves up a summary of everything.

All of the commands will return an updated value (except of course the "/" command, which as mentioned above
returns a summary of everything).

This list isn't necessarily "finished" -- there are other things that would be nice
to be able to control without having to reprogram everything.

For example, it would be nice to also control how often we sample the temperature,
how often we can allow adjustments to the thermostat to be made, just how many temperature
samples do we actually keep, etc.

Another next-step idea is to add some support for tracking the time of the day.
Then, based on the current time, adjust the thermostat. For example,
when I leave my place for the day, it would be nice if the thermostat was turned down
a few degrees so I don't have to pay for heating an empty room.        

### Temperature Client (temperature_client_arduino.ino and temperature_client_esp8266.ino)
The Arduino on this end is connected to:

1. BMP180 Temperature/Pressure Sensor
2. ESP8266 for communicating to the other Arduino

The ESP8266 is running in AP_STA mode. This was originally so that I could directly
connect to it and view the temperature myself, but it's practically deprecated
now since the Thermostat Arduino can serve up a summary of everything, including the temperature
reported here.

Anyway, this guy is generally passive. Whenever it's own static IP address is visited, it simply serves up
the current temperature it grabbed from the BMP180.

The setup doesn't really scale as is, given the Thermostat ESP8266 directly connects to this.
If I were to start adding more clients so I can track the temperature in multiple rooms,
I'd probably set then all up to be strictly clients without any static IP stuff. Then have the
Thermostat Arduino get an ID from each client and store the temperatures accordingly.

## Gotchas
Here's a list of stupid things I wasted time on. Some points are generic Arduino things I just didn't know about.

### Programming the ESP8266
1. For the particular kind of chip I was using, you need to connect the GPIO0 pin to GND to specifically tell it you want to program it.

2. The chip also needs to be "enabled". This means the CH_PD pin must also be connected to VCC
or something similar.

3. It wasn't clear at first, but to communicate with it, the baud rate needs to be set to 115200.

4. There are a lot of warnings out there about how this chip runs on 3.3V, not 5V. You'll find out quickly enough
when the chip gets super hot and possibly starts to power cycle itself repeatedly.

5. The TX pin of the ESP8266 is connected to the TX pin of the Arduino (and RX to RX) for programming. But when you're using it
otherwise, connectivity is switched (TX->RX and RX->TX).

6. It wasn't uncommon for programming the chip to intermittently fail, but it worked after power cycling the chip.

### Communicating with the ESP8266
1. When the Arduino and ESP8266 send messages to each other over the Serial connection, I have them pretty much
exclusively use the "readStringUntil()" function for getting the messages.
The delimiter I used is '\n'. I made the unfortunate mistake of
assuming that the Serial.println() functions terminates their strings with ONLY a NEWLINE charater ('\n'), when in fact
it also includes the carriage return ('\r'). So there were issues where my commands weren't being "recognized"
because they contained an carriage return that isn't really obvious to see when printing the "invalid" command
in question. 

### Driving the Stepper Motor
1. The motor I used is a 5V motor. I accidentally tried using a 5V DC power supply. That's not gonna work
since there's going to be some voltage drop across the motor driver itself. This was my main cause for
the "stuttering motor" problem. Once I got a 12V power supply, the motor worked.

2. I forgot to make sure the motor driver and Arduino share the same ground. Even with the above issue fixed
the motor still wasn't producing as much torque as I expected. It would also occasionally jerk a huge amount
when I tried to drive it only 1 or 2 steps. The cause became obvious when the motor wouldn't work at all
after plugging in the DC power supply to the Arduino (as oppossed to running off USB). When I had
the outer part of the USB connector touch the connector on the Arduino, the motor suddenly worked, pointing
to a grounding issue. When I added a jumper between the ground pins of the driver and Arduino itself, the
motor produced the expected torque (and was generally snappier).

### BMP180 and Enclosed Spaces
I originally only drilled a small few holes in the enclosure that the BMP180, Arduino, and ESP8266
live in together. I quickly started to notice that the reported temperature was 5-10 degrees higher
than what it really was in the room (the chip is fairly accurate according to most people).
It quickly become obvious that the heat generated by the ESP8266 in particular was getting in the way.
I eventually had to make another opening for the BMP180 to poke out.

## Misc. Thermostat Notes
My thermostat only triggers the heat. There is no cooling system. In general, it's going to take
more time for a room to cool off than it does to heat up (it's not like the radiators suddenly stop emitting heat
when everything "turns off"). So, when we make adjustments to the thermostat, we need to account for that.
When we turn up the heat, we should be ready to turn down the thermostat more quickly.

Don't forget about the ambient temperature. Sometimes it will be higher than your desired temperature.
In that case, no amount of turning down the thermostat is going to actually make the room cooler.
In the worst case, the thermostat is unnecessarily turned all the way to the lowest temperature, with
nothing to show for it.

## Useful References
[The actual ESP8266 library code, which has very useful examples](https://github.com/esp8266/Arduino)

[ESP8266 Library Documentation is pretty good](http://arduino-esp8266.readthedocs.io/en/latest/index.html)

[A tutorial on getting query parameters for the ESP8266](https://techtutorialsx.com/2016/10/22/esp8266-webserver-getting-query-parameters/)

[A tutorial on inter-ESP8266 communication](http://www.microcontroller-project.com/esp8266-inter-communication-using-arduino-ide.html)

[A note on ESP8266 boot mode selection](https://github.com/espressif/esptool/wiki/ESP8266-Boot-Mode-Selection)

Not all of it applied in my case, though.

[ESP8266 AT Command Reference](https://room-15.github.io/blog/2015/03/26/esp8266-at-command-reference/)

I reprogrammed my ESP8266 and so didn't use this, but good for getting a little more insight into what it can do.
