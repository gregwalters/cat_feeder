#include <Servo.h>
#include "RTClib.h"
#include <Wire.h>
#include <Adafruit_RGBLCDShield.h>
#include <utility/Adafruit_MCP23017.h>
#include <EEPROM.h>

// LCD Backlight Colors
#define OFF 0x0
#define RED 0x1
#define YELLOW 0x3
#define GREEN 0x2
#define TEAL 0x6
#define BLUE 0x4
#define VIOLET 0x5
#define WHITE 0x7

// Constants for Setup
Adafruit_RGBLCDShield lcd = Adafruit_RGBLCDShield();
RTC_DS1307 rtc;
Servo theServo;
byte led = 13;
byte servo = 9;

// LED Control
byte onBoardLedState = LOW;
int blinkDuration = 250;
int onBoardLedInterval = 1750;

// Servo Control
byte servoMaxDegrees = 100;
byte servoMinDegrees = 0;
long servoSpeed = 2; // lower is faster
byte servoPauseOpen = 150; // how long to stay open
int servoDegrees = 1; // higher is faster
int servoPosition = 0;

// Time Keeping
unsigned long currentMillis = 0;
unsigned long previousOnBoardLedMillis = 0;
unsigned long previousServoMillis = 0;
unsigned long previousLCDMillis = 0;
unsigned long lastActivityMillis = 0;
unsigned long lastColorChangeMillis = 0;
unsigned long pauseTime = 0;
DateTime now = DateTime(2014, 1, 1, 1, 1, 1);
word year = 0;
byte month = 0;
byte day = 0;
byte hour = 0;
byte minute = 0;
byte second = 0;

// Buttons
uint8_t buttons = 0;
boolean select = false;

// LCD Control
int lcdUpdateFreq = 150;
boolean displayOn = true;
byte colors[8] = {OFF, RED, YELLOW, GREEN, TEAL, BLUE, VIOLET, WHITE};
byte color = 7;
byte topMenuPos = 0;
byte middleMenuPos = 0;
byte lastTopMenuPos = 255;
byte lastMiddleMenuPos = 255;
char cursorPos[2] = {0,0};
word lcdTimeout = 60000;
int colorChangeTime = 1500;
boolean lcdContentsChanged = true;
boolean force = false;
byte claimingMenu = 0;

// default display 1: Next Feed: HH:MM
// default display 2: PUSH SEL 2 FEED
// Menu 1           : Set time
// Menu 1.l1        : YYYY/MM/DD
// Menu 1.l2        : HH:MM:SS
// Menu 2           : Set feed min del
// Menu 2.l1        : Min minutes
// Menu 2.l2        : $CurrentValue
// Menu 3           : Set feed max del
// Menu 3.l1        : Max minutes
// Menu 3.l2        : $CurrentValue
// Menu 4           : Set feed amount
// Menu 4.l1        : 
// Menu 4.l2        : $CurrentValue
char menuItems[7][3][17] = {
  {"Next Feed: ", "Push SEL 2 Feed", ""},
  {"Set Time"        , "YYYY/MM/DD"     , "HH:MM:SS"},
  {"Set feed min del", "Min Minutes"    , "Value: "},
  {"Set feed max del", "Max Minutes"    , "Value: "},
  {"Set feed amount ", "Food Amount"    , "Value: "},
  {"Save Config"     , ""               , ""},
  {"Read Config"     , ""               , ""},
};

// Food Control
word minFoodDelay = 30;
word maxFoodDelay = 90;
word openFoodDelay = 150;
word tmpDelay = 0;

boolean changesToSave = false;

void setup() {
  setup_serial();
  setup_servo();
  setup_heartbeat();
  setup_RTC();
  setup_LCD();
  readFromEEPROM();
}

void setup_serial() {
  while (!Serial); // for Leonardo/Micro/Zero
  Serial.begin(57600);
}

void setup_servo() {
  //min default 544, max default 2400
  int min = 800;
  int max = 2900;
  int pos = 0;
  theServo.attach(servo, min, max);
  
  // Close the serveo because the startup code leaves it half open
  for (pos = 180; pos >= 0; pos -= 1) {
    theServo.write(pos);
    delay(servoSpeed);
  }
}

void setup_heartbeat() {
  pinMode(led, OUTPUT);
}

void setup_RTC() {
  // we power the RTC from pings A2 & A3 directly
  pinMode(A3, OUTPUT);
  pinMode(A2, OUTPUT);
  digitalWrite(A3, HIGH);
  digitalWrite(A2, LOW);

  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }
}

void setup_LCD() {
  lcd.begin(16, 2);
  lcd.setBacklight(WHITE);
  lcd.clear();
}

void updateServoPosition() {
  if ( (currentMillis - previousServoMillis) >= (servoSpeed + pauseTime) ) {
    pauseTime = 0;
    if ( (servoPosition >= servoMaxDegrees) && (servoDegrees == 1) ){
      servoDegrees = - servoDegrees; 
      pauseTime = openFoodDelay;
      Serial.println("Holding Open");
    } else if ( (servoPosition <= servoMinDegrees) && (servoDegrees < 1) ) {
      servoDegrees = - servoDegrees;
      pauseTime = random(minFoodDelay * 60l * 1000l, maxFoodDelay * 60l * 1000l);
      Serial.print("Holding Closed: ");
      Serial.println(pauseTime);
      lcdContentsChanged = true;
    } 
    servoPosition += servoDegrees;
    theServo.write(servoPosition);
    previousServoMillis = currentMillis;
  }
}

void updateOnBoardLedState() {
  if (onBoardLedState == LOW) {
    if (currentMillis - previousOnBoardLedMillis >= onBoardLedInterval) {
      onBoardLedState = HIGH;
      previousOnBoardLedMillis += onBoardLedInterval;
    }
  } else {
    if (currentMillis - previousOnBoardLedMillis >= blinkDuration) {
      onBoardLedState = LOW;
      previousOnBoardLedMillis += blinkDuration;
    }
  }
}

void switchLeds() {
  digitalWrite(led, onBoardLedState);
  //digitalWrite(other_led, otherLedState);
}

void readButtons() {
  buttons = lcd.readButtons();

}

void turnOnLCD() {
  displayOn = true;
  lastActivityMillis = currentMillis;
  lcdContentsChanged = true;
  Serial.print("X: ");
  Serial.println(cursorPos[0], DEC);
  Serial.print("Y: ");
  Serial.println(cursorPos[1], DEC);
}

void updateLCDContents() {
  if (currentMillis - previousLCDMillis >= lcdUpdateFreq) {
    if ((currentMillis - lastActivityMillis >= lcdTimeout) && (displayOn) ) {
      displayOn = false;
      cursorPos[0] = 0;
      cursorPos[1] = 0;
      claimingMenu = 0;
      lcdContentsChanged = true;
    }
    if (buttons) {
      if (buttons & BUTTON_UP) {
        cursorPos[1] -= 1;
        turnOnLCD();
      }
      if (buttons & BUTTON_DOWN) {
        cursorPos[1] += 1;
        turnOnLCD();
      }
      if (buttons & BUTTON_LEFT) {
        cursorPos[0] -= 1;
        turnOnLCD();
      }
      if (buttons & BUTTON_RIGHT) {
        cursorPos[0] += 1;
        turnOnLCD();
      }
      if (buttons & BUTTON_SELECT) {
        select = true;
        turnOnLCD();
      }
    }
    previousLCDMillis += lcdUpdateFreq;
  }
}

void updateLCDColor() {
  if (currentMillis - lastColorChangeMillis >= colorChangeTime) {
    if (displayOn) {
      if (color == 7) {
        color = 1;
      } else {
        color += 1;
      }
    } else {
      color = 0;
    }
    lastColorChangeMillis += colorChangeTime;
  }
}

void padPrint(byte number) {
  if (number < 10) {
    lcd.print("0");
  }
  lcd.print(number, DEC);
}

void switchLCD() {
  byte cursorXModifier = 0;
  byte cursorYModifier = 0;
  lcd.setBacklight(colors[color]);
  if (lcdContentsChanged) {
    lcd.clear();
    lcd.blink();        
    if ( (cursorPos[1] == 0) && (claimingMenu == 0) ) {
      lcd.noBlink();
      lcd.setCursor(0,0);
      lcd.print(menuItems[0][0]); // Next feed time
      DateTime tmp = rtc.now();
      DateTime future (tmp + TimeSpan((pauseTime - (currentMillis - previousServoMillis) ) / 1000));
      padPrint(future.hour());
      lcd.print(":");
      padPrint(future.minute());
      lcd.setCursor(0,1);
      lcd.print(menuItems[0][1]); // Select to feed
      if (select) {
        pauseTime = 0;
        select = false;
        Serial.println("Servo should run!");
      }
    } else if ( ( (cursorPos[1] >= 1) && ( cursorPos[1] <= 2) && (claimingMenu < 3) ) || (claimingMenu == 1) || (claimingMenu == 2) ) {
      if (cursorPos[0] == 0) {
        lcd.setCursor(0,0);
        lcd.print(menuItems[1][0]); // Time
        lcd.setCursor(0,1);
        lcd.print(menuItems[2][0]); // Min feed delay
        claimingMenu = 0;
      } else if ( ( (cursorPos[0] >= 1) && (cursorPos[1] == 1) && (claimingMenu == 0) ) || (claimingMenu == 1)) {
        if (now.year() == 2014) {
          Serial.println("Getting time data from RTC");
          now = rtc.now();
          year = now.year();
          month = now.month();
          day = now.day();
          hour = now.hour();
          minute = now.minute();
          second = now.second();
        }
        lcd.setCursor(0,0);
        if ( (cursorPos[0] >= 1) && (cursorPos[0] <= 4) ) {
          char modifier = cursorPos[1] - 1;
          year = now.year() + modifier;
        } 
        lcd.print(year);
        if ( cursorPos[0] == 5 ) {
          cursorPos[1] = 1;
        }
        lcd.print("/");
        if ( (cursorPos[0] >= 6) && (cursorPos[0] <= 7) ) {
          char modifier = cursorPos[1] - 1;
          month = now.month() + modifier;
        } 
        padPrint(month);
        if ( cursorPos[0] == 8 ) {
          cursorPos[1] = 1;
        }
        lcd.print("/");
        if ( (cursorPos[0] >= 9) && (cursorPos[0] <= 10) ) {
          char modifier = cursorPos[1] - 1;
          day = now.day() + modifier;
        } 
        padPrint(day);
        lcd.setCursor(0,1);
        if ( cursorPos[0] == 11) {
          cursorPos[1] = 1;
        }
        if ( (cursorPos[0] >= 12) && (cursorPos[0] <= 13) ) {
          char modifier = cursorPos[1] - 1;
          hour = now.hour() + modifier;
        } 
        padPrint(hour);
        if (cursorPos[0] == 14) {
          cursorPos[1] = 1;
        }
        lcd.print(":");
        if ( (cursorPos[0] >= 15) && (cursorPos[0] <= 16) ) {
          char modifier = cursorPos[1] - 1;
          minute = now.minute() + modifier;
        } 
        padPrint(minute);
        if ( cursorPos[0] == 17 ) {
          cursorPos[1] = 1;
        }
        lcd.print(":");
        if ( (cursorPos[0] >= 18) && (cursorPos[0] <= 19) ) {
          char modifier = cursorPos[1] - 1;
          second = now.second() + modifier;
        } 
        padPrint(second);
        //lcd.print(menuItems[1][2]); //HH:MM:SS
        if (cursorPos[1] % 2 == 0) {
          cursorYModifier += 1;
        }
        cursorXModifier = -1;
        if (cursorPos[0] >= 12) {
          cursorXModifier -= 11;
          cursorYModifier += 1;
        }
        claimingMenu = 1;
        if (select) {
          rtc.adjust(DateTime(year, month, day, hour, minute, second));
          cursorPos[0] = 0;
          cursorPos[1] = 1;
          claimingMenu = 0;
          now = DateTime(2014, 1, 1, 1, 1, 1);
          select = false;
          force = true;
          changesToSave = true;
          Serial.println("Adjusted RTC");
        }
      } else if ( ( (cursorPos[0] >= 1) && (cursorPos[1] == 2) ) || (claimingMenu == 2) ) {
        lcd.setCursor(0,0);
        lcd.print(menuItems[2][1]); // Min
        lcd.setCursor(0,1);
        lcd.print(menuItems[2][2]); // Value
        if ( (cursorPos[0] >= 1) && (cursorPos[0] <= 2) ) {
          tmpDelay = minFoodDelay;
          tmpDelay += cursorPos[1] - 2;
        }
        lcd.print(tmpDelay, DEC);
        if (abs(cursorPos[1]) % 2 == 1) {
          cursorYModifier += 1;
        }
        cursorXModifier = 6;
        claimingMenu = 2;
        if (select) {
          minFoodDelay = tmpDelay;
          claimingMenu = 0;
          select = false;
          force = true;
          cursorPos[0] = 0;
          cursorPos[1] = 2;
          changesToSave = true;
          Serial.println("Adjusted Min food delay");
        }
      }
    } else if ( ( (cursorPos[1] >= 3) && ( cursorPos[1] <= 4) && ( (claimingMenu == 0) || (claimingMenu > 2 ) ) ) || (claimingMenu == 3) || (claimingMenu == 4) ) {
      if (cursorPos[0] == 0) {
        lcd.setCursor(0,0);
        lcd.print(menuItems[3][0]); // Max feed delay
        lcd.setCursor(0,1);
        lcd.print(menuItems[4][0]); // Feed amount
        claimingMenu = 0;
      } else if ( ( (cursorPos[0] >= 1) && (cursorPos[1] == 3) && (claimingMenu == 0 ) ) || (claimingMenu == 3) ) {
        lcd.setCursor(0,0);
        lcd.print(menuItems[3][1]); // Max
        lcd.setCursor(0,1);
        lcd.print(menuItems[3][2]); // Value
        if ( (cursorPos[0] >= 1) && (cursorPos[0] <= 2) ) {
          tmpDelay = maxFoodDelay;
          tmpDelay += cursorPos[1] - 3;
        }
        lcd.print(tmpDelay, DEC);
        if (abs(cursorPos[1]) % 2 == 1) {
          cursorYModifier += 1;
        }
        cursorXModifier = 6;
        //cursorYModifier = -1;
        claimingMenu = 3;
        if (select) {
          maxFoodDelay = tmpDelay;
          claimingMenu = 0;
          select = false;
          force = true;
          cursorPos[0] = 0;
          cursorPos[1] = 3;
          changesToSave = true;
          Serial.println("Adjusted max food delay");
        }
      } else if (((cursorPos[0] >= 1) && (cursorPos[1] == 4)) || (claimingMenu == 4)) {
        lcd.setCursor(0,0);
        lcd.print(menuItems[4][1]); // Null
        lcd.setCursor(0,1);
        lcd.print(menuItems[4][2]); // Value
        if ( (cursorPos[0] >= 1) && (cursorPos[0] <= 2) ) {
          tmpDelay = openFoodDelay;
          tmpDelay += cursorPos[1] - 4;
        }
        lcd.print(tmpDelay, DEC);
        if (abs(cursorPos[1]) % 2 == 1) {
          cursorYModifier += 1;
        }
        cursorXModifier = 6;
        claimingMenu = 4;
        if (select) {
          openFoodDelay = tmpDelay;
          claimingMenu = 0;
          select = false;
          force = true;
          cursorPos[0] = 0;
          cursorPos[1] = 4;
          changesToSave = true;
          Serial.println("Adjusted open food delay");
        }
      }
    } else if ( (cursorPos[1] >= 5) && (cursorPos[1] <= 6) && (claimingMenu == 0) ) {
      lcd.setCursor(0,0);
      lcd.print(menuItems[5][0]); // Save
      lcd.setCursor(0,1);
      lcd.print(menuItems[6][0]); // Read
      if (select) {
        if (cursorPos[1] == 5) {
          saveToEEPROM();
        } else if (cursorPos[1] == 6) {
          readFromEEPROM();
        }
        select = false;
      }
      claimingMenu = 0;        
    }
    lcd.setCursor(cursorPos[0] + cursorXModifier, (abs(cursorPos[1]) - 1 - cursorYModifier) % 2);
    lcdContentsChanged = force;
  }
}

void writeWord(byte addr, word thing) {
  byte high = highByte(thing);
  byte low  = lowByte(thing);
  EEPROM.write(addr, high);
  EEPROM.write(addr + 1, low);
}

void saveToEEPROM() {
  writeWord(0, minFoodDelay);
  writeWord(2, maxFoodDelay);
  writeWord(4, openFoodDelay);
  Serial.println("Saved to EEPROM");
}

word readWord(byte addr) {
  byte high = EEPROM.read(addr);
  byte low  = EEPROM.read(addr + 1);
  Serial.println("Read from EEPROM");
  return word(high, low);
}

void readFromEEPROM() {
  minFoodDelay = readWord(0);
  maxFoodDelay = readWord(2);
  openFoodDelay = readWord(4);
}

void loop() {
  currentMillis = millis();
  updateOnBoardLedState();
  switchLeds();
  updateServoPosition();
  readButtons();
  updateLCDContents();
  updateLCDColor();
  switchLCD();
  if (changesToSave) {
    saveToEEPROM();
  }
  select = false;
  force = false;
  changesToSave = false;
}
