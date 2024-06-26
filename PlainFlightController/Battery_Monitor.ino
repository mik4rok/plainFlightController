/* 
* Copyright (c) 2023,2024 P.Cook (alias 'plainFlight')
*
* This file is part of the PlainFlightController distribution (https://github.com/plainFlight/plainFlightController).
* 
* This program is free software: you can redistribute it and/or modify  
* it under the terms of the GNU General Public License as published by  
* the Free Software Foundation, version 3.
*
* This program is distributed in the hope that it will be useful, but 
* WITHOUT ANY WARRANTY; without even the implied warranty of 
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU 
* General Public License for more details.
*
* You should have received a copy of the GNU General Public License 
* along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include "Defines.h"

/*
* To measure battery voltage a potential divider is used with the following values:
* R1=61.9K, R2=17.4K... 15.04V max input = 3.3V out, ...12.6V in = 2.772V out
* Note: Wise to add 1uF capacitor across R2 but not vital. You may need to increase weighted filter values if not fitted. 
*   
*      ______ Vin... +ve of flight battery
*    _|_
*   |   |
*   |   |  R1 = 61.9K Ohms
*   |___|
*     |______ Vout... to BATT_ADC_PIN
*    _|_
*   |   |
*   |   |  R2 = 17.4K Ohms
*   |___|
*     |
*     |______ Gnd
*
* For the above resistor values and with ESP32C3 ADC resolution of 12bits; therefore 15.04V/4096 = 3.67mV per bit.
*/

//Modules specific defines
#define WEIGHT_NEW        1.0f
#define WEIGHT_OLD        999.0f
#define FILTER_DIVISOR    (WEIGHT_NEW + WEIGHT_OLD)
#define ADC_MULTIPLIER    0.0037    //3.67mV (0.00367V)... However, due to resistor tolerances fine tune this value with multimeter and DEBUG_BATTERY_VOLTS
#define MIN_CELL_VOLTAGE  3.31f     //3.31V 
#define LOW_CELL_VOLTAGE  3.5f      //3.5V per cell is the threshold where we start to pulse throttle to indicate low voltage
#define MOTOR_OFF_TIME    250U      //Off pulse ms
#define MOTOR_ON_TIME     750U      //On pulse ms
#define MIN_3S_VOLTAGE    9.9f      //9.9V
#define MIN_2S_VOLTAGE    6.6f      //6.6V
#define MIN_1S_VOLTAGE    3.3f      //3.3V

//Module variables
static float batteryVoltage = 8.4f;   //Set at 2 cells charged to speed filter initialisation
static uint32_t numberCells = 0U;


/*
* DESCRIPTION: Sets ADC pin for battery monitor, primes the battery voltage filter, then detects number of cells in flight pack/battery.
* NOTE: batteryVoltage initilised to 8.4V to help speed filter prime.
*/
void initBatteryMonitor(void)
{
  pinMode(BATT_ADC_PIN, INPUT);

  for(uint32_t i=0U; i<(uint32_t)FILTER_DIVISOR; i++)
  {
    batteryMonitor();
  }

  setNumberCells();
}


/*
* DESCRIPTION: Used at initialisation to determine number of lipo cells connected to ESC(s).
* Cell voltage used as this simplifies low battery detection when flight controller is used with varying cell counts.
*/
void setNumberCells(void)
{
  if (batteryVoltage > MIN_3S_VOLTAGE)
  {
    numberCells = 3U;
  }
  else if (batteryVoltage > MIN_2S_VOLTAGE)
  {
    numberCells = 2U;
  }
  else
  {
    numberCells = 1U;
  }

  #if defined(DEBUG_BATTERY_VOLTS)
    Serial.print("Cells detected: ");
    Serial.println(numberCells);
  #endif
}


/*
* DESCRIPTION: Samples, scales and applieds a weighted filter to the battery voltage.
*/
void batteryMonitor(void)
{
  float rawVoltage = analogRead(BATT_ADC_PIN) * ADC_MULTIPLIER;
  //Simple weighted filter...
  batteryVoltage = ((batteryVoltage * WEIGHT_OLD) + (rawVoltage * WEIGHT_NEW)) / FILTER_DIVISOR;

  #if defined(DEBUG_BATTERY_VOLTS)
    Serial.println(batteryVoltage);
  #endif
}


/*
* DESCRIPTION: Detects battery voltage and will pulse the throttle to indicate voltage is low. 
* When voltage falls to below 3.3V (for lipo) throttle is completely turned off.
* CAUTION: This method of indicating low battery is not recommended for quadcopters or VTOLs.
*/
void limitThrottle(int32_t* const requiredThrottle, bool throttleLow)
{
  static uint64_t pulseTime = 0U;
  static bool pulse;
  float cellVoltage = batteryVoltage / (float)numberCells;
  uint64_t nowTime = millis();

  //if LOW_CELL_VOLTAGE per cell then we start to limit throttle
  if (MIN_CELL_VOLTAGE > cellVoltage)
  {
    //Kill throttle completely
    *requiredThrottle = MOTOR_MIN_TICKS;
  }
  else if ((LOW_CELL_VOLTAGE > cellVoltage) && !throttleLow)
  {
    //Start pulsing throttle to indicate battery voltage low    
    if (pulse)
    {
      if (nowTime >= pulseTime)
      {
        pulseTime = nowTime + MOTOR_OFF_TIME;
        pulse = false;
      }      
    }
    else
    { 
      *requiredThrottle = MOTOR_MIN_TICKS;        //Set min throttle
    
      if (nowTime >= pulseTime)
      {
        pulseTime = nowTime + MOTOR_ON_TIME;
        pulse = true;
      } 
    }    
  }
  else
  {
    //If throttle is not low keep resetting time so we get full pulse time on next use
    pulseTime = nowTime + MOTOR_ON_TIME;
    pulse = true;
  }
}
