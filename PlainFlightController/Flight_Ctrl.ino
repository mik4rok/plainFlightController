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

#include "Flight_Ctrl.h"
#include "Radio_Ctrl.h"
#include "PIDF.h"
#include "IMU.h"
#include "Actuators.h"

extern void playLedSequence(states currentState);
states getRequiredState(states lastState);
void modelMixer(Actuators *actuate, int32_t roll, int32_t pitch, int32_t yaw);
void motorMixer(Actuators *actuate, int32_t yaw);


/*
* DESCRIPTION: Main routine of plainFlight controller.
*/
void flightControl(void)
{
  states currentState;
  static states lastState = state_disarmed;
  int32_t roll_PIDF, pitch_PIDF, yaw_PIDF;
  Actuators control = {0};

  getSbus();
  readIMUdata();
  Madgwick6DOF(imu.gyro_X, imu.gyro_Y, imu.gyro_Z, imu.accel_X, imu.accel_Y, imu.accel_Z, timeDelta);
  batteryMonitor();
  currentState = getRequiredState(lastState);  
  processDemands(currentState);
  playLedSequence(currentState);

  if (lastState != currentState)
  {
    #if defined(DEBUG_FLIGHT_STATE)
      Serial.print("State: ");
      Serial.println(currentState);
    #endif
    rollPIF.iTermReset();
    pitchPIF.iTermReset();
    yawPIF.iTermReset();
  } 

  switch(currentState)
  {
    case state_calibrating:
      //Craft has to be still to calibrate and exit this state.
      //Fall throught to disarmed to keep motor turned off...
      //then fall throught to pass-through so servo controls work just incase we got here whilst in flight i.e. after reset or power supply brown-out.
      calibrateGyro();
    case state_disarmed:  
      //If disarmed then operate in pass through mode and ensure throttle is at minimum.
      rxCommand.throttle = MOTOR_MIN_TICKS;
    case state_pass_through:
      modelMixer(&control, rxCommand.roll, rxCommand.pitch, rxCommand.yaw);
      motorMixer(&control,rxCommand.yaw);
      control.servo1 = (int32_t)map(control.servo1,-PASS_THROUGH_RES, PASS_THROUGH_RES, SERVO_MIN_TICKS, SERVO_MAX_TICKS);
      control.servo2 = (int32_t)map(control.servo2,-PASS_THROUGH_RES, PASS_THROUGH_RES, SERVO_MIN_TICKS, SERVO_MAX_TICKS);
      control.servo3 = (int32_t)map(control.servo3,-PASS_THROUGH_RES, PASS_THROUGH_RES, SERVO_MIN_TICKS, SERVO_MAX_TICKS);
      control.servo4 = (int32_t)map(control.servo4,-PASS_THROUGH_RES, PASS_THROUGH_RES, SERVO_MIN_TICKS, SERVO_MAX_TICKS);
      break;
    
    case state_rate:
      //Gyro based rate mode, control demands are in degrees/second x100.
      roll_PIDF = rollPIF.pidfController(  rxCommand.roll, GYRO_X, &gains[rate_gain].roll);
      pitch_PIDF = pitchPIF.pidfController(rxCommand.pitch,GYRO_Y, &gains[rate_gain].pitch);
      #if defined(USE_HEADING_HOLD) || defined(USE_HEADING_HOLD_WHEN_YAW_CENTRED)
        headingHold(&yaw_PIDF);
      #else
        //Yaw still works in rate mode, though you could use Madgwick output as heading hold function 
        yaw_PIDF = yawPIF.pidfController(rxCommand.yaw,  GYRO_Z, &gains[rate_gain].yaw);
      #endif
      modelMixer(&control, roll_PIDF, pitch_PIDF, yaw_PIDF);
      motorMixer(&control, yaw_PIDF);
      //Convert PID demands to timer ticks for servo PWM/PPM
      control.servo1 = (int32_t)map(control.servo1,-PIDF_MAX_LIMIT, PIDF_MAX_LIMIT, SERVO_MIN_TICKS, SERVO_MAX_TICKS);
      control.servo2 = (int32_t)map(control.servo2,-PIDF_MAX_LIMIT, PIDF_MAX_LIMIT, SERVO_MIN_TICKS, SERVO_MAX_TICKS);
      control.servo3 = (int32_t)map(control.servo3,-PIDF_MAX_LIMIT, PIDF_MAX_LIMIT, SERVO_MIN_TICKS, SERVO_MAX_TICKS);
      control.servo4 = (int32_t)map(control.servo4,-PIDF_MAX_LIMIT, PIDF_MAX_LIMIT, SERVO_MIN_TICKS, SERVO_MAX_TICKS);
      break;

    default:
    case state_failsafe: 
      rxCommand.roll  = FAILSAFE_ROLL_ANGLE_x100;
      rxCommand.pitch = FAILSAFE_PITCH_ANGLE_x100;
      rxCommand.yaw = 0;
    case state_auto_level:
      //Gyro & accelerometer based Madgwick filter for levelled mode, control demands are in degrees x100.
      roll_PIDF = rollPIF.pidfController(rxCommand.roll, (int32_t)((imu.roll + trim.accRoll) * 100.0f), &gains[levelled_gain].roll);
      pitch_PIDF = pitchPIF.pidfController(rxCommand.pitch,(int32_t)((imu.pitch + trim.accPitch) * 100.0f), &gains[levelled_gain].pitch); 
      #if defined(USE_HEADING_HOLD) && defined(USE_HEADING_HOLD_WHEN_YAW_CENTRED)
        #error Cannot have both USE_HEADING_HOLD and USE_HEADING_HOLD_WHEN_YAW_CENTRED defined at the same time! 
      #elif defined(USE_HEADING_HOLD) || defined(USE_HEADING_HOLD_WHEN_YAW_CENTRED)
        headingHold(&yaw_PIDF);
      #else
        //Yaw still works in rate mode, though you could use Madgwick output as heading hold function 
        yaw_PIDF = yawPIF.pidfController(rxCommand.yaw,  GYRO_Z, &gains[rate_gain].yaw);
      #endif
      modelMixer(&control, roll_PIDF, pitch_PIDF, yaw_PIDF);
      motorMixer(&control, yaw_PIDF);
      //Convert PID demands to timer ticks for servo PWM/PPM
      control.servo1 = (int32_t)map(control.servo1,-PIDF_MAX_LIMIT, PIDF_MAX_LIMIT, SERVO_MIN_TICKS, SERVO_MAX_TICKS);
      control.servo2 = (int32_t)map(control.servo2,-PIDF_MAX_LIMIT, PIDF_MAX_LIMIT, SERVO_MIN_TICKS, SERVO_MAX_TICKS);
      control.servo3 = (int32_t)map(control.servo3,-PIDF_MAX_LIMIT, PIDF_MAX_LIMIT, SERVO_MIN_TICKS, SERVO_MAX_TICKS);
      control.servo4 = (int32_t)map(control.servo4,-PIDF_MAX_LIMIT, PIDF_MAX_LIMIT, SERVO_MIN_TICKS, SERVO_MAX_TICKS);
      break;
  }

  //Arguably flaps should be in modelMixer(), however, to obtain constant flap offsets it needs to be done after any map() functions of certain flight modes.
  #if (defined(MIXER_PLANE_FULL_HOUSE) || defined(MIXER_PLANE_FULL_HOUSE_V_TAIL)) && defined(USE_FLAPS)
    uint32_t mappedFlaps = map(rxCommand.aux1Switch, (long)switch_low, (long)switch_high, 0, SERVO_HALF_TRAVEL_TICKS);
    control.servo1 -= mappedFlaps;
    control.servo2 += mappedFlaps;
  #endif

  //Add any trim offsets. Multiply by SERVO_TRIM_MULTIPLIER to normalise trim depending upon servo refresh rate due to changes in timer resolution.
  control.servo1 += trim.servo1 * SERVO_TRIM_MULTIPLIER;
  control.servo2 += trim.servo2 * SERVO_TRIM_MULTIPLIER;
  control.servo3 += trim.servo3 * SERVO_TRIM_MULTIPLIER;
  control.servo4 += trim.servo4 * SERVO_TRIM_MULTIPLIER;

  #if defined(USE_LOW_VOLT_CUT_OFF)
    //Low battery voltage will start throttle limiting
    limitThrottle(&control.motor1, rxCommand.throttleIsLow);   
    limitThrottle(&control.motor2, rxCommand.throttleIsLow);
  #endif
  
  //Update all actuators
  writeActuators(&control);
  lastState = currentState;
}



/*
* DESCRIPTION: Processes RC switch states to determine the require statemachine state.
* NOTE: Only allows arming if throttle is low.
* PARAMETERS:  The last flight state set.
* RETURNS:     The decoded state from set TX switches and SBUS failsafe.
*/
states getRequiredState(states lastState)
{  
  states requiredState;

  if (!imu.calibrated)
  {
    //We wait in calibrating until gyro has successfully calibrated.
    requiredState = state_calibrating;
  }
  else
  {
    if (imu.calibrated && (lastState == state_calibrating))
    {
      requiredState = state_disarmed;
    }
    else if (rxCommand.failsafe)
    {
      Serial.println("Failsafe");
      requiredState = state_failsafe;
    }
    else if (rxCommand.armSwitch)
    {  
      if ((rxCommand.throttleIsLow && (lastState == state_disarmed)) || (lastState != state_disarmed))
      {
        switch (rxCommand.modeSwitch)
        {
          case switch_low: 
            requiredState = state_pass_through;
            break;

          case switch_middle: 
            requiredState = state_rate;
            break;

          case switch_high: 
            requiredState = state_auto_level;
            break;
            
          default: 
            requiredState = state_failsafe;     //Should never get here but if we did then force failsafe and keep fingers crossed
            break;
        } 
      }   
    }
    else
    {
      requiredState = state_disarmed;
    }
  }

  return requiredState;
}


/*
* DESCRIPTION: Mixes differential throttle if required, or pases through rx stick command for throttle value.
* NOTE: Also checks for failsafe and sets throttle to off if failsafe active.
*/
void motorMixer(Actuators *actuate, int32_t yaw)
{  
  //Convert PID/stick commands to timer ticks for servo PWM/PPM or Oneshot125
  #if defined(USE_DIFFERENTIAL_THROTTLE)
    actuate->motor1 = (rxCommand.failsafe) ? MOTOR_MIN_TICKS : rxCommand.throttle + yaw;
    actuate->motor2 = (rxCommand.failsafe) ? MOTOR_MIN_TICKS : rxCommand.throttle - yaw;
  #else
    actuate->motor1 = (rxCommand.failsafe) ? MOTOR_MIN_TICKS : rxCommand.throttle;
    actuate->motor2 = actuator.motor1;
  #endif 

  #if defined(DEBUG_MOTOR_MIXER)
    Serial.print("Motor1: ");
    Serial.print(actuate->motor1);
    Serial.print(", Motor2: ");
    Serial.print(actuate->motor2);
    #if defined(USE_DIFFERENTIAL_THROTTLE)
      Serial.print(", mYaw: ");
      Serial.print(mappedYaw);
    #endif
    Serial.println();
  #endif 
}


/*
* DESCRIPTION: Mixes control functions for the model type selected.
*/
void modelMixer(Actuators *actuate, int32_t roll, int32_t pitch, int32_t yaw)
{
  #if defined(MIXER_FLYING_WING)
    actuate->servo1 = roll - pitch;
    actuate->servo2 = roll + pitch;
    actuate->servo3 = yaw;
    actuate->servo4 = SERVO_CENTRE_TICKS;     //Spare Output
  #elif defined(MIXER_PLANE_FULL_HOUSE_V_TAIL) 
    actuate->servo1 = roll;
    actuate->servo2 = roll;
    actuate->servo3 = yaw + pitch;
    actuate->servo4 = yaw - pitch;
  #elif defined(MIXER_PLANE_FULL_HOUSE)
    actuate->servo1 = roll;
    actuate->servo2 = roll;
    actuate->servo3 = pitch;
    actuate->servo4 = yaw; 
  #elif defined(MIXER_PLANE_V_TAIL)
    actuate->servo1 = roll + pitch;
    actuate->servo2 = roll - pitch;
    actuate->servo3 = SERVO_CENTRE_TICKS;     //Spare Output;
    actuate->servo4 = SERVO_CENTRE_TICKS;     //Spare Output; 
  #elif defined(MIXER_PLANE_RUDDER_ELEVATOR)
    actuate->servo1 = roll;
    actuate->servo2 = pitch;
    actuate->servo3 = SERVO_CENTRE_TICKS;     //Spare Output
    actuate->servo4 = SERVO_CENTRE_TICKS;     //Spare Output
  #else
    #error No model MIXER defined by user ! 
  #endif

  #if defined(DEBUG_SERVO_MIXER)
    Serial.print("Servo1: ");
    Serial.print(actuate->servo1);
    Serial.print(", Servo2: ");
    Serial.print(actuate->servo2);
    Serial.print(", Servo3: ");
    Serial.print(actuate->servo3);
    Serial.print(", Servo4: ");
    Serial.println(actuate->servo4);
  #endif
}


/*
* DESCRIPTION: Creates a heading hold feature for rudder equipt aircraft.
* Applies i gain to rudder when enabled to create a hold like function. Have not used Madwick yaw output as it is gyro based only and suffers from drift.
* As a result kept it simple by just adding i gain to rudder when required via Tx switch.
* Note: This Yaw PID output could be applied to roll with modification if you have no rudder available.
*/
void headingHold(int32_t* yaw)
{
  if (rxCommand.headingHold)
  {
    *yaw = yawPIF.pidfController(rxCommand.yaw,  GYRO_Z, &gains[levelled_gain].yaw);  
  }
  else
  {
    yawPIF.iTermReset();
    *yaw = yawPIF.pidfController(rxCommand.yaw,  GYRO_Z, &gains[rate_gain].yaw);  
  }
}

