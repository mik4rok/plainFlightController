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

#include "Sbus_Rx.h"
#include "Flight_Ctrl.h"
#include "Defines.h"
#include "Radio_Ctrl.h"


//Module specific defines
#define SBUS_TIMEOUT  20000   //microseconds
//Enable SBUS_DEBUG then use serial monitor to find max and min SBUS values for your Tx... or adjust endpoints on Tx 
#define MAX_SBUS_US   1810
#define MIN_SBUS_US   172
#define MID_SBUS_US   991     //(MIN_SBUS_US + ((MAX_SBUS_US - MIN_SBUS_US) / 2)) ...save a few microseconds by not calculating realtime.

//Arduino requires these declarations here for typedef's to work in function prototypes
void processDemands(states currentState);


/*
* DESCRIPTION: Grabs one SBUS packet, also detects loss of packets to set failsafe.
* Note: At power on the receiver may not output any data until connected with transmitter i.e. Tx failsafe set to no pulses.
*/
void getSbus(void) 
{
  static uint64_t sbusTimeout = 0;

  if (sbusRxMsg()) 
  {
    rxCommand.newSbusPacket = true;
    sbusTimeout = micros() + SBUS_TIMEOUT;
    #if defined(SBUS_DEBUG)
      printSbusData();
    #endif
  }
  else if (micros() >= sbusTimeout)
  {
    //For this situation we need to detect and force failsafe flag.
    rxCommand.failsafe = true;
  }
}


/*
* DESCRIPTION: Takes SBUS data and scales it to bounds that we want to work with.
* For pitch, roll and yaw...
* When in pass through SBUS is scaled between 1ms and 2ms, bounds defined by MIN_SBUS_US and MAX_SBUS_US
* When in rate mode SBUS data is scales to degrees/second, bounds defined by MAX_XXXX_ANGLE_DEGS_x100.
* When in levelled mode SBUS data is scaled to a maximum pitch/roll angle, bounds defined by MAX_XXXX_ANGLE_DEGS_x100
* Note integer maths is used.
* For switch inputs, these are converted to states depending upon switch function.
* Throttle is effectively left in pass through, but scaled to suit PWM timer values. 
*/
void processDemands(states currentState)
{
  if (rxCommand.newSbusPacket)
  {
    rxCommand.newSbusPacket = false;

    switch(currentState)
    {
      case state_auto_level:
        rxCommand.roll =  (abs((int32_t)(rxData.ch[roll] - MID_SBUS_US)) > deadband.roll) ? map(rxData.ch[roll], MIN_SBUS_US, MAX_SBUS_US, -MAX_ROLL_ANGLE_DEGS_x100,  MAX_ROLL_ANGLE_DEGS_x100) : 0;
        rxCommand.pitch = (abs((int32_t)(rxData.ch[pitch] - MID_SBUS_US)) > deadband.pitch) ? map(rxData.ch[pitch], MIN_SBUS_US, MAX_SBUS_US, -MAX_PITCH_ANGLE_DEGS_x100, MAX_PITCH_ANGLE_DEGS_x100) : 0;
        //Rudder still works in rate mode when level mode (though you could create a heading hold feature with Madgwick output)
        rxCommand.yaw =   (abs((int32_t)(rxData.ch[yaw] - MID_SBUS_US)) > deadband.yaw) ? map(rxData.ch[yaw], MIN_SBUS_US, MAX_SBUS_US, -MAX_YAW_RATE_DEGS_x100, MAX_YAW_RATE_DEGS_x100) : 0;
        break;
    
      case state_rate: 
        rxCommand.roll =  (abs((int32_t)(rxData.ch[roll] - MID_SBUS_US)) > deadband.roll) ? map(rxData.ch[roll], MIN_SBUS_US, MAX_SBUS_US, -MAX_ROLL_RATE_DEGS_x100, MAX_ROLL_RATE_DEGS_x100) : 0;
        rxCommand.pitch = (abs((int32_t)(rxData.ch[pitch] - MID_SBUS_US)) > deadband.pitch) ? map(rxData.ch[pitch], MIN_SBUS_US, MAX_SBUS_US, -MAX_PITCH_RATE_DEGS_x100,MAX_PITCH_RATE_DEGS_x100) : 0;
        rxCommand.yaw =   (abs((int32_t)(rxData.ch[yaw] - MID_SBUS_US)) > deadband.yaw) ?  map(rxData.ch[yaw], MIN_SBUS_US, MAX_SBUS_US, -MAX_YAW_RATE_DEGS_x100,  MAX_YAW_RATE_DEGS_x100) : 0;
        break;
    
      default:
      case state_disarmed:
      case state_failsafe:
      case state_pass_through:
        rxCommand.roll =  (abs((int32_t)(rxData.ch[roll] - MID_SBUS_US)) > deadband.roll) ? map(rxData.ch[roll], MIN_SBUS_US, MAX_SBUS_US, -PASS_THROUGH_RES, PASS_THROUGH_RES) : 0;
        rxCommand.pitch = (abs((int32_t)(rxData.ch[pitch] - MID_SBUS_US)) > deadband.pitch) ? map(rxData.ch[pitch], MIN_SBUS_US, MAX_SBUS_US, -PASS_THROUGH_RES,PASS_THROUGH_RES) : 0;
        rxCommand.yaw =   (abs((int32_t)(rxData.ch[yaw] - MID_SBUS_US)) > deadband.yaw) ?  map(rxData.ch[yaw], MIN_SBUS_US, MAX_SBUS_US, -PASS_THROUGH_RES,  PASS_THROUGH_RES) : 0;
        break;
    }

    rxCommand.throttle = map(rxData.ch[throttle],MIN_SBUS_US, MAX_SBUS_US, MOTOR_MIN_TICKS, MOTOR_MAX_TICKS);
    //Channels 4 to 7 are uses as switch inputs, map to the required enum state
    rxCommand.armSwitch =  (MID_SBUS_US < rxData.ch[aux1]) ? true : false;
    //Mode switch defines the required flight mode.
    rxCommand.modeSwitch = (Switch_States)map(rxData.ch[aux2], MIN_SBUS_US, MAX_SBUS_US, (long)switch_low, (long)switch_high);
    //aux1 & 2 are decoded as 3 position switches, but will work with a 2 position Tx switch
    rxCommand.aux1Switch = (Switch_States)map(rxData.ch[aux3], MIN_SBUS_US, MAX_SBUS_US, (long)switch_low, (long)switch_high);
    rxCommand.aux2Switch = (Switch_States)map(rxData.ch[aux4], MIN_SBUS_US, MAX_SBUS_US, (long)switch_low, (long)switch_high);  
    //Copy failsafe flag
    rxCommand.failsafe = rxData.failsafe;
    rxCommand.throttleIsLow = (rxData.ch[throttle] < (MIN_SBUS_US + THROTTLE_LOW_THRESHOLD)) ? true : false;

    #if defined(USE_HEADING_HOLD_WHEN_YAW_CENTRED)
      //When rudder is centred and Aux2 switch is enabled
      bool rudderCentre = (abs((int32_t)(rxData.ch[yaw] - MID_SBUS_US)) < deadband.yaw) ? true : false;
      rxCommand.headingHold = (rudderCentre && (rxCommand.aux2Switch == switch_high)) ? true : false;
    #elif defined(USE_HEADING_HOLD)
      //When Aux2 switch is enabled
      rxCommand.headingHold = (rxCommand.aux2Switch == switch_high) ? true : false;
    #endif

    #if defined(DEBUG_RADIO_COMMANDS)
      printRadioCommands();
    #endif
  }
}


/*
* DESCRIPTION: When SBUS_DEBUG is defined data is printed to PC terminal.
*/
#if defined(SBUS_DEBUG)
  void printSbusData(void)
  {
    // Display the received data 
    for (uint8_t i = 0; i < 16; i++) 
    {
      Serial.print(rxData.ch[i]);
      Serial.print("\t");
    }
    // Display lost frames and failsafe data 
    Serial.print(rxData.lost_frame);
    Serial.print("\t");
    Serial.println(rxData.failsafe);
  }
#endif

/*
* DESCRIPTION: When DEBUG_RADIO_COMMANDS is defined data is printed to PC terminal.
*/
#if defined(DEBUG_RADIO_COMMANDS)
  void printRadioCommands(void)
  {
    Serial.print("armed: ");
    Serial.print(rxCommand.armSwitch);
    Serial.print(", mode: ");
    Serial.print(rxCommand.modeSwitch);
    Serial.print(", aux1: ");
    Serial.print(rxCommand.aux1Switch);
    Serial.print(", aux2: ");
    Serial.print(rxCommand.aux2Switch);
    //Add other switches here as you need them
    Serial.print(", thr: ");
    Serial.print(rxCommand.throttle);
    Serial.print(", ail: ");
    Serial.print(rxCommand.roll);
    Serial.print(", pch: ");
    Serial.print(rxCommand.pitch);
    Serial.print(", yaw: ");
    Serial.println(rxCommand.yaw);
  }
#endif