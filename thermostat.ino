// Thermostat and LCD from The Shed Magazine http://theshedmag.co.nz
#include <LiquidCrystal.h>
#include <EEPROM.h>

/*******************************************************
 A thermostat program with display and keyboard
 Licenced under the GPLv3 or later.
 (C)2015 vik@diamondage.co.nz
 
 Uses a standard Arduino 1602 LCD, a heater drivign relay
 on HEATER_PIN, a reay for fan or suchlike on COOLIN_PIN
 and a 100K thermistor on THERMISTOR PIN, which is pulled
 high via a 4K7 resistor. the other themistor pin is
 grounded.
 
(Hat tip to Mark Bramwell for the keyboard expertise)

********************************************************/

// select the pins used on the LCD panel
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

#define  HEATER_PIN  A5
#define  COOLING_PIN  A1
#define  THERMISTOR_PIN  A3

// define some values used by the panel and buttons
int lcd_key     = 0;
int adc_key_in  = 0;
#define btnRIGHT  0
#define btnUP     1
#define btnDOWN   2
#define btnLEFT   3
#define btnSELECT 4
#define btnNONE   5

// Where to find max/min in EEPROM
#define EEPROM_MIN  0
#define EEPROM_MAX  2
#define EEPROM_FLAG  4

// These values must live at the FLAG if EEPROM is valid.
#define FLAG_1  215
#define FLAG_2  68

// States for our State Machine
#define ST_INIT  0
#define ST_SET_TEMP_DISPLAY 1
#define ST_TEMP_CHANGED 3
#define ST_CHECK_MENU 4

// Temperature constants
const int tempTable[]={
  480,120*2,
  581,105*2,
  670,90*2,
  740,80*2,
  814,70*2,
  856,60*2,
  900,50*2,
  940,40*2,
  966,30*2,
  986,20*2,
  994,15*2,
  1000,10*2,
  1009,0,
  1013,-6*2,
};
#define TEMP_TABLE_ENTRIES  14
#define BAD_SENSOR 9999

// Calculate temperature through a look-up table. Returns BAD_SENSOR if out of range
// This may not be the best way, but it is the most legible!
int calcTemp(int sens) {
  int i;
  int sensDiff;
  int sensDelta;
  int tempDiff;
  // Spot very low values
  if (sens <= tempTable[0]) return BAD_SENSOR;
  // Spot very high values
  if (sens >= tempTable[(TEMP_TABLE_ENTRIES*2)-2]) return BAD_SENSOR;
  // OK, we're in range. Trot through the table until we overtake our sensor reading
  i=1;
  while (tempTable[(i*2)]<sens) i++;
  // Find the difference between the last 2 table entries;
  sensDiff=tempTable[(i*2)]-tempTable[(i*2)-2];
  // Find the difference between the last 2 readings;
  tempDiff=tempTable[(i*2)+1]-tempTable[(i*2)-1];
  // Now the amount our sensor passed the previous table entry by
  sensDelta=sens-tempTable[(i*2)-2];
  // Now we can work out a ratio,and calculate the corresponding temperature
  return tempTable[(i*2)-1]+((tempDiff*sensDelta+1)/sensDiff);
}



int state=ST_INIT;
int temp_max=110*2;
int temp_min=50*2;
int temp_now=0;
boolean  heating=false;
boolean  lastHeating=false;
boolean  cooling=false;
boolean  lastCooling=false;

// read the buttons
int read_LCD_buttons()
{
   adc_key_in = analogRead(0);      // read the value from the sensor 
   // my buttons when read are centered at these valies: 0, 144, 329, 504, 741
   // we add approx 50 to those values and check to see if we are close
   if (adc_key_in > 1000) return btnNONE; // We make this the 1st option for speed reasons since it will be the most likely result
   // For V1.1 us this threshold
  /* if (adc_key_in < 50)   return btnRIGHT;  
   if (adc_key_in < 250)  return btnUP; 
   if (adc_key_in < 450)  return btnDOWN; 
   if (adc_key_in < 650)  return btnLEFT; 
   if (adc_key_in < 850)  return btnSELECT;  */
  
   // For V1.0 comment the other threshold and use the one below:
  
   if (adc_key_in < 50)   return btnRIGHT;  
   if (adc_key_in < 195)  return btnUP; 
   if (adc_key_in < 380)  return btnDOWN; 
   if (adc_key_in < 555)  return btnLEFT; 
   if (adc_key_in < 790)  return btnSELECT;   
  
  
  
   return btnNONE;  // when all others fail, return this...
}


void printPadded(int n) {
  int absn=abs(n);
  if (absn>999)
    lcd.print("Def");
  else {
    if (n>=0)
      if (absn<100) lcd.print(" ");
    if (absn<10) lcd.print(" ");
    lcd.print(n);
  }
}

void setup()
{
  // initialize serial communications at 9600 bps:
  Serial.begin(9600);
  lcd.begin(16, 2);              // start the library
  lcd.setCursor(0,0);
  pinMode(HEATER_PIN,OUTPUT);
  pinMode(COOLING_PIN,OUTPUT);
  pinMode(THERMISTOR_PIN,INPUT);
  heating=false;
  state=ST_INIT;
  // See if we can read in the EEPROM flag data
  if ((EEPROM.read(EEPROM_FLAG)==FLAG_1) && (EEPROM.read(EEPROM_FLAG+1)==FLAG_2)) {
    temp_min=EEPROM.read(EEPROM_MIN);
    temp_min+=EEPROM.read(EEPROM_MIN+1)*256;
    temp_max=EEPROM.read(EEPROM_MAX);
    temp_max+=EEPROM.read(EEPROM_MAX+1)*256;
  }
}
 
void loop()
{
  int tempReading;
  char heat_display;

  // Check the temperature. Average it a bit.
  tempReading=calcTemp((analogRead(THERMISTOR_PIN)+analogRead(THERMISTOR_PIN))/2);

  // We had 2 bad sensor readings. Stop dead.
  if (tempReading==BAD_SENSOR) {
    digitalWrite(HEATER_PIN,0);
    digitalWrite(COOLING_PIN,0);
    lcd.clear();
    lcd.home();
    lcd.noCursor();
    lcd.print("BAD SENSOR");
    lcd.setCursor(0,1);
    lcd.print("HALTED");
    while (true); // Nothing
  }

  // Turn heating on/off as required.
  if (heating) {
    if (tempReading > temp_min+1)
      heating=false;
  } else {
    if (tempReading < temp_min-1)
      heating=true;
  }
  // Cooling on/off as required.
  if (cooling) {
    if (tempReading < temp_max-1)
      cooling=false;
  } else {
    if (tempReading > temp_max+1)
      cooling=true;
  }

  switch(state) {
    // First thing we do to restart everything. Display version.
    case ST_INIT:
       lcd.print("Thermostat V0.1"); // print a simple message
       delay(1000);
       lcd.clear();
       state=ST_SET_TEMP_DISPLAY;
       // Drop through!

    // Clear the display and prep for showing temperature
    case ST_SET_TEMP_DISPLAY:
      lcd.clear();
      lcd.home();
      lcd.noCursor();
      lcd.print("   C    Max    C");
      lcd.setCursor(8,1);
      lcd.print("Min    C");
      // Drop through!

    // The temp has changed. Update new values.
    case ST_TEMP_CHANGED:
      lcd.home();
      temp_now=tempReading;
      printPadded(temp_now/2);
      lcd.setCursor(12,0);
      printPadded(temp_max/2);
      lcd.setCursor(6,0);
      heat_display='.';
      if (heating) heat_display='+';
      if (cooling) heat_display='-';
      lcd.print(heat_display);
      lcd.setCursor(12,1);
      printPadded(temp_min/2);
      state=ST_CHECK_MENU;
      delay(500);
      break;

    case ST_CHECK_MENU:
      lcd_key = read_LCD_buttons();  // read the buttons
      lcd.setCursor(0,1);
      
       switch (lcd_key)               // depending on which button was pushed, we perform an action
       {
         case btnRIGHT:
           {
           lcd.print("MAX UP ");
           temp_max+=2;
           state=ST_TEMP_CHANGED;
           break;
           }
         case btnLEFT:
           {
           lcd.print("MAX DN ");
           if (temp_max>(temp_min+3)) {
             temp_max-=2;
             state=ST_TEMP_CHANGED;
           }
           break;
           }
         case btnUP:
           {
           lcd.print("MIN UP");
           if (temp_min<(temp_max-3)) {
             temp_min+=2;
             state=ST_TEMP_CHANGED;
           }
           break;
           }
         case btnDOWN:
           lcd.print("MIN DN ");
           temp_min-=2;
           state=ST_TEMP_CHANGED;
           break;

         case btnSELECT:
           lcd.print("SAVE  ");
           // See if we can read in the EEPROM flag data
           EEPROM.write(EEPROM_FLAG,FLAG_1);
           EEPROM.write(EEPROM_FLAG+1,FLAG_2);
           EEPROM.write(EEPROM_MIN+1,temp_min/256);
           EEPROM.write(EEPROM_MIN,temp_min&255);
           EEPROM.write(EEPROM_MAX+1,temp_max/256);
           EEPROM.write(EEPROM_MAX,temp_max&255);
           break;

         case btnNONE:
           lcd.print("NONE  ");
           if (temp_now!=tempReading) state=ST_TEMP_CHANGED;
           break;
      }

     default:
       break;
  }
  
  // State machine finished. Now we do all the things that must be done.

  // print the results to the serial monitor:
  Serial.print("Sensor = " );                       
  Serial.print(analogRead(THERMISTOR_PIN));
  Serial.print("\tTemp = ");
  Serial.print(tempReading);
  Serial.println("C");
  
  digitalWrite(HEATER_PIN,heating?1:0);
  digitalWrite(COOLING_PIN,cooling?1:0);
  // Pause to stop power fluctuations annoying thermistor.
  if (lastHeating!=heating) {
    delay(300);
    lastHeating=heating;
    state=ST_TEMP_CHANGED;
  }
  // Pause to stop power fluctuations annoying thermistor.
  if (lastCooling!=cooling) {
    delay(300);
    lastCooling=cooling;
    state=ST_TEMP_CHANGED;
  }

}
