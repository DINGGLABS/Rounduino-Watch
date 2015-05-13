/** ===========================================================
 *  example watch
 *
 * \file       example_watch_V02
 * \author     jh
 * \date       xx.04.2015
 *
 * \controller ATmega32U4
 * \f_clk      ext. 8MHz
 *
 * \brief      example SW to control the DINGGLABS Rounduino
 *
 * \history    -02 state machine (configurable watch)
 *             -01 creating the file (example_empty as origin)
 * @{
 ============================================================== */

/* Configurable defines --------------------------------------- */
/* system behavior defines */
#define DELAY_STARTMESSAGE         2000
#define MAX_DURATION_SHORTCLICK    1000    // max. duration in ms a SHORTCLICK will be recognized
#define MAX_DURATION_FEEDBACK       500    // max. duration in ms a acustical feedback will be given
#define DISPLAY_DURATION_DATE      3000    // duration to display the date
#define DISPLAY_DURATION_WATCH   300000    // duration to display the watch

/* ADC */
#define V_SCAN_MAX                 1023
#define V_SCAN_OFFSET               700

/* Includes --------------------------------------------------- */
#include "Rounduino_lib.h"
#include <SPI.h>
#include <Wire.h>

/* State machine ---------------------------------------------- */
/* states */
#define STATE_INIT           0
#define STATE_OFF            1
#define STATE_ON             2
#define STATE_DATE           3
#define STATE_CONFIG         4

/* events */
#define EVENT_NONE           0
#define EVENT_SHORTCLICK     1
#define EVENT_LONGCLICK      2

/* General defines -------------------------------------------- */
#define FOREVER           true
#define UP                true
#define DOWN             false

/* Global variables ------------------------------------------- */
/* config week-days */
enum Day {MO=1, DI=2, MI=3, DO=4, FR=5, SA=6, SO=7};

/* config substates */
enum Substate {Y, M, D, d, h, m, s};

/* config flag */
boolean configFlag;

/* defaults state machine */
byte state = STATE_INIT;          // first state
byte configSubstate = Y;          // first substate)

/* timing reference variables */
unsigned long startSecond = 0;    // in ms
unsigned long startButton1 = 0;
unsigned long startButton2 = 0;
unsigned long startButton3 = 0;

/** ===========================================================
 * \fn      setup
 * \brief   standard Arduino setup-function with all
 *          initializations
 *
 * \param   -
 * \return  -
 ============================================================== */
void setup()
{
//  Serial.begin(115200);
//  while(!Serial);
//  Serial.println("-------------------------");
//  Serial.println("setup");

  initRounduino();
  
/* init time */
  Time.sec = 0;
  Time.min = 0;
  Time.hour = 0;
  Time.day = 1;
  Time.date = 1;
  Time.month = 1;
  Time.year = 2015;
  
  setCurrentTime();
  
  clearSymbolList();
  clearDisplay();
}

/** ===========================================================
 * \fn      loop
 * \brief   standard Arduino forever loop-function with the
 *          state machine
 *
 * \param   -
 * \return  -
 ============================================================== */
void loop()
{ 
  /* check if a butten has been clicked (short, long or not) */
  byte buttonEvent1 = getButtonEvent1(MAX_DURATION_SHORTCLICK, MAX_DURATION_FEEDBACK);
  byte buttonEvent2 = getButtonEvent2(MAX_DURATION_SHORTCLICK, MAX_DURATION_FEEDBACK);
  byte buttonEvent3 = getButtonEvent3(MAX_DURATION_SHORTCLICK, MAX_DURATION_FEEDBACK);  
  
  /* State machine -------------------------------------------- */
  switch (state)
  {
    case STATE_INIT:
    {
      static unsigned long startMessageDuration = 0;
      static boolean firstInitPass = true;
      
      /* display startup message */
      const unsigned char str1[] = {"ROUNDUINO"};
      drawString(str1, 8, 55, MAX_BRIGHTNESS);
      delay(200);  // drawString needs some time to function! (~20ms per character)
       
      if (firstInitPass)
      {
        startMessageDuration = millis();
        firstInitPass = false;
      }
      
      if ((millis() - startMessageDuration) >= DELAY_STARTMESSAGE)
      {
        firstInitPass = true;
        
        clearDisplay();
        
        state = STATE_ON;
        startSecond = millis();  // timing reference for the on-state
      }
      
      break;
    }
      
    case STATE_ON:
    {
//      displayButtonStates(buttonEvent1, buttonEvent2, buttonEvent3);  // debugging
      
      if (buttonEvent1 == EVENT_LONGCLICK)
      {
        clearDisplay();
        state = STATE_CONFIG;
      }
      else if (buttonEvent1 == EVENT_SHORTCLICK)
      {
        clearDisplay();
        state = STATE_DATE;
        startSecond = millis();  // timing reference for the date-state
      }
      else
      {
        drawBattery(80, 10, MAX_BRIGHTNESS);
        displayWatch();
      }
      
      break; 
    } 
      
    case STATE_OFF:
    {
      state = STATE_INIT;
      
      turnOff();
      /* ...continues here after waking up */
      
      startSecond = millis();    // timing reference to turn off after a certain time
      
      break;
    }
    
    case STATE_DATE:
    {
      if (buttonEvent1 == EVENT_SHORTCLICK || (millis() - startSecond) >= DISPLAY_DURATION_DATE)
      {
        clearDisplay();
        state = STATE_ON;
      }
      else displayDate();
      
      break;
    }
    
    case STATE_CONFIG:
    { 
      static boolean configDisplayDate = true;
      
      /* display the date or the watch */
      if (configDisplayDate) displayDate();
      else
      {
        drawBattery(80, 10, MAX_BRIGHTNESS);
        displayWatch();
      }
      
      clearCustomSymbol();
      
      /* substate machine */
      switch(configSubstate)
      {
        case Y:
        {
          /* configuration started: */
          configFlag = true;
          
          createCustomSymbol(83, 80, MAX_BRIGHTNESS);
          if (buttonEvent1 >= EVENT_SHORTCLICK) Time.year++;            // '>=' means short-click or long-click
          if (buttonEvent2 == EVENT_SHORTCLICK) configSubstate = M;
          if (buttonEvent3 >= EVENT_SHORTCLICK) Time.year--;
          if (Time.year > 2099) Time.year = 2000;
          if (Time.year < 0) Time.year = 2099;
          break;
        }

        case M:
        {
          createCustomSymbol(95, 47, MAX_BRIGHTNESS);
          if (buttonEvent1 >= EVENT_SHORTCLICK) Time.month++;
          if (buttonEvent2 == EVENT_SHORTCLICK) configSubstate = D;
          if (buttonEvent3 >= EVENT_SHORTCLICK) Time.month--;
          if (Time.month > 12) Time.month = 1;
          if (Time.month < 1) Time.month = 12;
          break;
        }
        
        case D:
        {
          createCustomSymbol(23, 47, MAX_BRIGHTNESS);
          if (buttonEvent1 >= EVENT_SHORTCLICK) Time.date++;
          if (buttonEvent2 == EVENT_SHORTCLICK) configSubstate = d;
          if (buttonEvent3 >= EVENT_SHORTCLICK) Time.date--;
          
          switch (Time.month)
          {
            case 1:   // January
            case 3:   // March
            case 5:   // May
            case 7:   // July
            case 8:   // August
            case 10:  // October
            case 12:  // December
              if (Time.date > 31) Time.date = 1;
              if (Time.date < 1) Time.date = 31;
              break;
            case 4:   // April
            case 6:   // June
            case 9:   // September
            case 11:  // November
              if (Time.date > 30) Time.date = 1;
              if (Time.date < 1) Time.date = 30;
              break;
            case 2:   // February
              /* check if it's a leap year */
              if ((Time.year % 4) == 0)
              {
                if (Time.date > 29) Time.date = 1;
                if (Time.date < 1) Time.date = 29;
              }
              else
              {
                if (Time.date > 28) Time.date = 1;
                if (Time.date < 1) Time.date = 28;
              }
              break;
            default: while (FOREVER) displayError();
          }
          break;
        }
        
        case d:
        {
          createCustomSymbol(35, 14, MAX_BRIGHTNESS);
          drawCustomSymbol();
          createCustomSymbol(59, 14, MAX_BRIGHTNESS);
          if (buttonEvent1 >= EVENT_SHORTCLICK) Time.day++;
          if (buttonEvent2 == EVENT_SHORTCLICK)
          {
            configSubstate = h;
            clearDisplay();
            configDisplayDate = false;
          }
          if (buttonEvent3 >= EVENT_SHORTCLICK) Time.day--;
          if (Time.day > 7) Time.day = 1;
          if (Time.day < 1) Time.day = 7;
          break;
        }
        
        case h:
        {
          createCustomSymbol(23, 47, MAX_BRIGHTNESS);
          if (buttonEvent1 >= EVENT_SHORTCLICK) Time.hour++;
          if (buttonEvent2 == EVENT_SHORTCLICK) configSubstate = m;
          if (buttonEvent3 >= EVENT_SHORTCLICK) Time.hour--;
          if (Time.hour > 23) Time.hour = 0;
          if (Time.hour < 0) Time.hour = 23;
          break;
        }
        
        case m:
        {
          createCustomSymbol(95, 47, MAX_BRIGHTNESS);
          if (buttonEvent1 >= EVENT_SHORTCLICK) Time.min++;
          if (buttonEvent2 == EVENT_SHORTCLICK) configSubstate = s;
          if (buttonEvent3 >= EVENT_SHORTCLICK) Time.min--;
          if (Time.min > 59) Time.min = 0;
          if (Time.min < 0) Time.min = 59;
          break;
        }
        
        case s:
        {
          createCustomSymbol(59, 80, MAX_BRIGHTNESS);
          if (buttonEvent1 >= EVENT_SHORTCLICK) Time.sec++;
          if (buttonEvent2 == EVENT_SHORTCLICK)
          {
            configSubstate = Y;
            configDisplayDate = true;
            
            /* configuration finished... */
            setCurrentTime();
            configFlag = false;
            
            clearDisplay();
            state = STATE_ON;
            startSecond = millis();  // timing reference for the on-state
          }
          if (buttonEvent3 >= EVENT_SHORTCLICK) Time.sec--;
          if (Time.sec > 59) Time.sec = 0;
          if (Time.sec < 0) Time.sec = 59;
          break;
        }
        
        default: while (FOREVER) displayError();
      }
      
      drawCustomSymbol();
      clearSymbolList();
      
      break;
    }
    
    default: while (FOREVER) displayError();
  }
  
  if (buttonEvent2 == EVENT_LONGCLICK || (millis() - startSecond) > DISPLAY_DURATION_WATCH)
  { 
    /* configuration interrupted... */
    if (configFlag)
    {
      /* set current time and reset configuration state */
      setCurrentTime();
      configSubstate = Y;
      configFlag = false;
    }
            
    clearDisplay();
    state = STATE_OFF;
  }
}

/** ===========================================================
 * \fn      getButtonEventX
 * \brief   returns given values on base of current button 
 *          state
 *
 * \requ    bool function: getButtonStateX()
 *          define: EVENT_NONE
 *          define: EVENT_SHORTCLICK
 *          define: EVENT_LONGCLICK
 *
 * \param   (uint) max. duration in ms a short click will be
 *                 recognized
 *          (uint) max. duration in ms a acustical feedback will
 *                 be given
 *
 * \return  (byte) 0 = no event
 *                 1 = short button click
 *                 2 = long button click
 ============================================================== */
byte getButtonEvent1(unsigned int max_duration_shortclick, unsigned int max_duration_feedback)
{
  static boolean button1WasPressed = false;
  byte buttonEvent = EVENT_NONE;
  
  piezoOn = false;

  /* check if pressed */
  if (getButtonState1())
  {
    if (!button1WasPressed)
    {
      startButton1 = millis();  // get reference if first time pressed
      button1WasPressed = true;
    }
    
    /* calculate past time since pushed to give a feedback when a long click have been recognized */
    if ((millis() - startButton1) > max_duration_shortclick &&
        (millis() - startButton1) < (max_duration_feedback + max_duration_shortclick))
    {
      piezoFrequencyDivisor = LOW_PIEZO_FREQUENCY_DIVISOR;
      piezoOn = true;
    }
  }
  /* button have been released */
  else if (button1WasPressed)
  {
    button1WasPressed = false;
    
    /* calculate past time since pushed and set button event */
    if ((millis() - startButton1) <= max_duration_shortclick)
    {
      /* short click: */
      buttonEvent = EVENT_SHORTCLICK;
      piezoFrequencyDivisor = STD_PIEZO_FREQUENCY_DIVISOR;
      piezoOn = true;
    }
    else
    {
      /* long click: */
      buttonEvent = EVENT_LONGCLICK;
    }
  }
  
  return buttonEvent;
}

byte getButtonEvent2(unsigned int max_duration_shortclick, unsigned int max_duration_feedback)
{
  static boolean button2WasPressed = false;
  byte buttonEvent = EVENT_NONE;

  /* check if pressed */
  if (getButtonState2())
  {
    if (!button2WasPressed)
    {
      startButton2 = millis();  // get reference if first time pressed
      button2WasPressed = true;
    }
    
    /* calculate past time since pushed to give a feedback when a long click have been recognized */
    if ((millis() - startButton2) > max_duration_shortclick &&
        (millis() - startButton2) < (max_duration_feedback + max_duration_shortclick))
    {
      piezoFrequencyDivisor = LOW_PIEZO_FREQUENCY_DIVISOR;
      piezoOn = true;
    }
  }
  else if (button2WasPressed)
  {
    button2WasPressed = false;
    
    /* calculate past time since pushed and set button event */
    if ((millis() - startButton2) <= max_duration_shortclick) 
    {
      /* short click: */
      buttonEvent = EVENT_SHORTCLICK;
      piezoFrequencyDivisor = STD_PIEZO_FREQUENCY_DIVISOR;
      piezoOn = true;
    }
    else
    {
      /* long click: */
      buttonEvent = EVENT_LONGCLICK;
    }
  }
  
  return buttonEvent;
}

byte getButtonEvent3(unsigned int max_duration_shortclick, unsigned int max_duration_feedback)
{
  static boolean button3WasPressed = false;
  byte buttonEvent = EVENT_NONE;

  /* check if pressed */
  if (getButtonState3())
  {
    if (!button3WasPressed)
    {
      startButton3 = millis();  // get reference if first time pressed
      button3WasPressed = true;
    }
    
    /* calculate past time since pushed to give a feedback when a long click have been recognized */
    if ((millis() - startButton3) > max_duration_shortclick &&
        (millis() - startButton3) < (max_duration_feedback + max_duration_shortclick))
    {
      piezoFrequencyDivisor = LOW_PIEZO_FREQUENCY_DIVISOR;
      piezoOn = true;
    }
  }
  else if (button3WasPressed)
  {
    button3WasPressed = false;
    
    /* calculate past time since pushed and set button event */
    if ((millis() - startButton3) <= max_duration_shortclick) 
    {
      /* short click: */
      buttonEvent = EVENT_SHORTCLICK;
      piezoFrequencyDivisor = STD_PIEZO_FREQUENCY_DIVISOR;
      piezoOn = true;
    }
    else
    {
      /* long click: */
      buttonEvent = EVENT_LONGCLICK;
    }
  }
  
  return buttonEvent;
}

/** ===========================================================
 * \fn      drawBattery
 * \brief   draws the battery symbol on given coordinates and
 *          brightness
 *          this function have to be called from time to time
 *          to update current battery state
 *          (resets the buffer)
 *
 * \param   (byte) x and y coordinate of the battery symbol
 *          (byte) brightness of the battery symbol
 *
 * \return  -
 ============================================================== */
void drawBattery(byte x, byte y, byte b)
{
  clearCustomSymbol();
  
  addRectangle(0, 0, 15, 7);  
  addVLine(15, 2, 3);
  addVLine(16, 2, 3);
  
  /* get battery voltage and draw battery content */
  unsigned int v = (readBatteryVoltage() - V_SCAN_OFFSET);
  addFilledRectangle(1, 1, 13 * v / (V_SCAN_MAX - V_SCAN_OFFSET), 5);

  createCustomSymbol(x, y, b);
  drawCustomSymbol();
  
  clearCustomSymbol();
  clearSymbolList();
}

/** ===========================================================
 * \fn      displayWatch
 * \brief   draws the watch (hh:mm:ss) on the display
 *          this function have to be called at least every
 *          second to update the watch
 *
 * \param   -
 *
 * \return  -
 ============================================================== */
void displayWatch()
{
  if (!configFlag) getCurrentTime();
  
  createBigNumberSymbol(':', 47, 47, MAX_BRIGHTNESS);
  unsigned char string[3];
  
  itoa(Time.sec, (char*) string, 10);
  if(Time.sec < 10) 
  {
    createBigNumberSymbol('0', 35, 80, MAX_BRIGHTNESS);
    drawBigNumbers(string, 59, 80, MAX_BRIGHTNESS);
  }
  else drawBigNumbers(string, 35, 80, MAX_BRIGHTNESS);
  
  itoa(Time.min, (char*) string, 10);
  if(Time.min < 10)
  {
    createBigNumberSymbol('0', 71, 47, MAX_BRIGHTNESS);
    drawBigNumbers(string, 95, 47, MAX_BRIGHTNESS);
  }
  else drawBigNumbers(string, 71, 47, MAX_BRIGHTNESS);
  
  itoa(Time.hour, (char*) string, 10);
  if(Time.hour < 10)
  {
    createBigNumberSymbol('0', 0, 47, MAX_BRIGHTNESS);
    drawBigNumbers(string, 23, 47, MAX_BRIGHTNESS);
  }
  else drawBigNumbers(string, 0, 47, MAX_BRIGHTNESS);
  
  drawSymbols();
  clearSymbolList();
}

/** ===========================================================
 * \fn      displayDate
 * \brief   draws the date (dd:mm:yyyy) on the display
 *          this function have to be called from time to time
 *          to update the date
 *
 * \param   -
 *
 * \return  -
 ============================================================== */
void displayDate()
{
  unsigned char stringYear[5];
  unsigned char string[3];
  
  if (!configFlag) getCurrentTime();
  
  createBigNumberSymbol('.', 47, 47, MAX_BRIGHTNESS);
  
  /* draw year */
  itoa(Time.year, (char*) stringYear, 10);
  drawBigNumbers(stringYear, 11, 80, MAX_BRIGHTNESS);
  
  /* draw month */
  itoa(Time.month, (char*) string, 10);
  if(Time.month < 10)
  {
    createBigNumberSymbol('0', 71, 47, MAX_BRIGHTNESS);
    drawBigNumbers(string, 95, 47, MAX_BRIGHTNESS);
  }
  else drawBigNumbers(string, 71, 47, MAX_BRIGHTNESS);
  
  /* draw day (date) */
  itoa(Time.date, (char*) string, 10);
  if(Time.date < 10)
  {
    createBigNumberSymbol('0', 0, 47, MAX_BRIGHTNESS);
    drawBigNumbers(string, 23, 47, MAX_BRIGHTNESS);
  }
  else drawBigNumbers(string, 0, 47, MAX_BRIGHTNESS);
  
  /* draw week-day */
  switch (Time.day)
  {
    case MO: string[0] = 'M'; string[1] = 'o'; break;
    case DI: string[0] = 'D'; string[1] = 'i'; break;
    case MI: string[0] = 'M'; string[1] = 'i'; break;
    case DO: string[0] = 'D'; string[1] = 'o'; break;
    case FR: string[0] = 'F'; string[1] = 'r'; break;
    case SA: string[0] = 'S'; string[1] = 'a'; break;
    case SO: string[0] = 'S'; string[1] = 'o'; break;
  }
  string[2] = 0; 
  
  drawString(string, 51, 20, MAX_BRIGHTNESS);
  
  clearSymbolList();
}

void displayError()
{
  const unsigned char str[] = {"Error"};
  
  drawString(str, 16, 63, MAX_BRIGHTNESS/2);
  delay(500);
  clearDisplay();
  delay(500);
}

//void displayButtonStates(byte bE1, byte bE2, byte bE3)
//{
//  const char str1[] = {"B1"};
//  const char str2[] = {"B2"};
//  const char str3[] = {"B3"};
//  const char str4[] = {"N"};
//  const char str5[] = {"S"};
//  const char str6[] = {"L"};
//  
//  drawString(str1, 11, 14, MAX_BRIGHTNESS);
//  switch (bE1)
//  {
//    case EVENT_NONE: drawString(str4, 83, 14, MAX_BRIGHTNESS); break;
//    case EVENT_SHORTCLICK: drawString(str5, 83, 14, MAX_BRIGHTNESS); break;
//    case EVENT_LONGCLICK: drawString(str6, 83, 14, MAX_BRIGHTNESS); break;
//  }
//  
//  drawString(str2, 11, 47, MAX_BRIGHTNESS);
//  switch (bE2)
//  {
//    case EVENT_NONE: drawString(str4, 83, 47, MAX_BRIGHTNESS); break;
//    case EVENT_SHORTCLICK: drawString(str5, 83, 47, MAX_BRIGHTNESS); break;
//    case EVENT_LONGCLICK: drawString(str6, 83, 47, MAX_BRIGHTNESS); break;
//  }
//  
//  drawString(str3, 11, 80, MAX_BRIGHTNESS);
//  switch (bE3)
//  {
//    case EVENT_NONE: drawString(str4, 83, 80, MAX_BRIGHTNESS); break;
//    case EVENT_SHORTCLICK: drawString(str5, 83, 80, MAX_BRIGHTNESS); break;
//    case EVENT_LONGCLICK: drawString(str6, 83, 80, MAX_BRIGHTNESS); break;
//  }
//}

/**
 * @}
 */
