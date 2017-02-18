# cat_feeder
Arduino Powered Cat Feeder

### Why

I was inspired by *How to keep hungry tigers busy in the early morning* [0][1] and some cats that especially love to eat until they vomit to make an Arduino-powered cat feeder that will dole out food little-by-little over the day so my cats aren't hungry and don't get a chance to eat until they're full to the point of vomiting. Thus was born my way-more-than-$20-hackable-cat-feeder.

### Features

* Randomly dispenses food at an interval you control
* Allows you to set the minimum random interval
* Allows you to set the maximum random interval
* Allows you to set how long the feeder will remain open (and thus the amount of food) per feeding
* Shows what time the next feeding will be
* Allows you to do ad-hoc feedings at the press of a button
* Has a menu system built-in that allows for the adjustment of previously mentioned settings and setting the clock
* Has an RTC clock that will keep the time between power cycles
* Stores settings in EEPROM for persistence between reboots
* Turns off the LCD backlight after a period of inactivity 
* Cycles through LCD background colors when the backlight is on in hopes a random dance party will break out

### Dependencies

https://github.com/adafruit/RTClib
https://github.com/adafruit/Adafruit-RGB-LCD-Shield-Library

### Issues

* Has no bounds checking on the menu so you can scroll into oblivion and get lost
* Has no bounds checking on the settings you can put in

### Parts list
|Part|Cost|Link|
-----|----|----|
|CR1220 12mm Diameter - 3V Lithium Coin Cell Battery|0.95|https://www.adafruit.com/product/380|
|RGB LCD Shield Kit w/ 16x2 Character Display|24.95|https://www.adafruit.com/product/714|
|Adafruit DS1307 Real Time Clock Assembled Breakout Board|7.50|https://www.adafruit.com/product/3296|
|TowerPro SG90 Micro Servo 2pk|7.95|https://www.amazon.com/gp/product/B01608II3Q|
|Uno R3|8.99|https://www.amazon.com/gp/product/B01M1YIL43|

[0] https://nachonachoman.svbtle.com/how-to-keep-hungry-tigers-busy-in-the-early-morning

[1] https://news.ycombinator.com/item?id=13271522
