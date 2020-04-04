
/* Automated garden watering and water aeration system.
Created by: Tony Swanson
Date: 18 Nov 2017

Last Modified by: Tony Swanson
Date: 24 May 2018

Functions:
   *  Reads four capacitive moisture sensors (with outputs shifted and amplified to approximately a 0-5v range).  0v = wet = 100% humidity. 5v = dry = 0% humidity.
   *  Reads two input potentiometers (with 0-5v range) to set maximum and minimum soil moisture threshholds. 0v = Minimum value. 5v = Maximum value.
   *  Reads a debounced float switch to verify water reservoir level (analog input; >500 [of 1023] is full)
   *  Turns on a water pump as needed to maintain soil moisture level within bounds set by potentiometers
   *  Turns on air pumps and circulation pumps (on same circuit) if reservoir contains water
   *  Turns off all pumps (water, air, circulation) if reservoir is depleted
*/


// INITIALIZE GLOBAL VARIABLES
unsigned long time_now = 0UL;                   // time in ms from the start of the pump control if statement block started
unsigned long runtime_start = 0UL;              // time in ms at which the last pump run started
unsigned long runtime = 0UL;                    // Duration in ms since the water pump most recently started (i.e. start of a new cycle)
int sleepcycles = 0;                            // Number of times controller has been put to sleep, since runtime_start
const unsigned long sleepduration = 1721000UL;  // Duration of time in ms that the CSS555 timer will allow the arduino to sleep.
const float MaxPumpOff = 21.0;                  // Sets maximum threshhold to turn off water pump, currently set to 0.1V (21 of 1023) - "max" refers to at wettest point
const float MinPumpOff = 409.0;                 // Sets minimum threshhold to turn off water pump, currently set to 1.0V (205 of 1023)
const float MaxPumpOn = 327.0;                  // Sets maximum threshhold to turn on water pump, currently set to 0.5V (103 of 1023)
const float MinPumpOn = 818.0;                  // Sets minimum threshhold to turn on water pump, currently set to 2.5V (511 of 1023)
const unsigned long AirPumpOff = 2160000UL;     // Duration of time in ms that the air pumps will run.
const unsigned long PumpGap = 21600000UL;       // Duration of time in ms between air pump runs.
unsigned long AirPumpSleepEnd = 0UL;            // End time for MCU sleep during air pump mode


// INITIALIZE PIN NAMES
const int RXLED = 17;                           // sets RX led to pin 17 - used to blink as notifier program is running and has entered a new loop
const int AngledReservoir = A10;                // sets pin 10 (aka "A10") to read the float sensor in the smaller reservoir (closed = empty = pin pulled below 2.5v, pot value <500)
const int BigReservoir = A0;                    // sets pin 18 (aka "A0") to read the float sensor in the larger reservoir (closed = empty = pin pulled below 2.5v, pot value <500)
const int WaterPump = A3;                       // sets pin 21 (aka "A3") to actuate the water pump - HIGH is "On," LOW is "Off"
const int AirPump = 14;                         // sets pin 14 to actuate the air pump - HIGH is "On," LOW is "Off"
const int OnPot = A6;                           // set pins 4 (aka "A6") & 6 (aka "A7") to read potentiometers. "OnPot" controls soil moisture level that initiates watering. "OffPot" controls stop point
const int OffPot = A7;                  
const int TimerTrigger = 3;                     // sets pin 3 to trigger CSS555 timer
const int Wakeup = 2;                           // sets interrupt input, from CSS555, to wake up microcontroller
const int Reset = 7;                            // sets reset output pin
const int AngledSensor1 = A8;                   // sets pin 8 as a moisture sensor pin   0 = wet , 1023 = dry
const int AngledSensor2 = A9;                   // sets pin 9 as a moisture sensor pin   0 = wet , 1023 = dry
const int BigSensor1 = A1;                      // sets pin 19 as a moisture sensor pin   0 = wet , 1023 = dry
const int BigSensor2 = A2;                      // sets pin 20 as a moisture sensor pin   0 = wet , 1023 = dry


// INITIALIZE SUB-FUNCTION VARIABLES
int Sensor1Val = 0;                             // Reading from 1st moisture sensor called within "readSoilSensors" function
int Sensor2Val = 0;                             // Reading from 2nd moisture sensor called within "readSoilSensors" function
int Sensor3Val = 0;                             // Reading from 3rd moisture sensor called within "readSoilSensors" function
int Sensor4Val = 0;                             // Reading from 4th moisture sensor called within "readSoilSensors" function
//int MaxMoistureLevel = 0;                       // The "wettest" of the four moisture sensor values
int AvgMoistureLevel = 0;                       // The average of the four moisture sensor values
//int MinMoistureLevel = 0; 
int dwell = 0;                                  // Duration of time spent at each duty cycle step while soft-starting the pump motor (254 steps, which excludes "0" [0% / off] and "255" [100% / on])
int ii = 0;                                     // Counter used by 'FOR' loop in the pump motor soft-start routine


void setup() {


//SET PIN INPUTS/OUTPUTS
pinMode(RXLED, OUTPUT);                         // Set pin as output, to control RX LED
pinMode(AngledReservoir,INPUT);                 // Set pin as input, to read float sensor
pinMode(BigReservoir,INPUT);                    // Set pin as input, to read float sensor
pinMode(WaterPump,OUTPUT);                      // Set pin as output, to control water pump; HIGH is "On," LOW is "Off"
pinMode(AirPump,OUTPUT);                        // Set pin as output, to control air pump; HIGH is "On," LOW is "Off"
pinMode(OnPot,INPUT);                           // Set pin as input, to read watering gap input potentiometer
pinMode(OffPot,INPUT);                          // Set pin as input, to read watering duration input potentiometer
pinMode(TimerTrigger,OUTPUT);                   // Set pin as output, to trigger CSS555 timer
pinMode(Wakeup,INPUT);                          // Set pin as input, to wake up from CSS555 timer on falling edge
pinMode(Reset, OUTPUT);                         // set pin as output, to reset CSS555 timer
pinMode(AngledSensor1,INPUT);                   // Set pin as input, to read moisture sensor
pinMode(AngledSensor2,INPUT);                   // Set pin as input, to read moisture sensor
pinMode(BigSensor1,INPUT);                      // Set pin as input, to read moisture sensor
pinMode(BigSensor2,INPUT);                      // Set pin as input, to read moisture sensor


//SET UNUSED INPUTS/OUTPUTS TO CONSERVE POWER
pinMode(0,OUTPUT);
pinMode(1,OUTPUT);
pinMode(5,OUTPUT);
pinMode(15,OUTPUT);
pinMode(16,OUTPUT);


//SET PIN STATES
digitalWrite (RXLED, HIGH);                      // Set pin as high (off)
digitalWrite(WaterPump,LOW);                     // Set pin as low (pump off)
digitalWrite(AirPump,LOW);                       // Set pin as low (pump off)
digitalWrite(TimerTrigger,HIGH);                // Set pin as high, CSS555 timer triggers on falling edge
digitalWrite(Reset,HIGH);                       // Set pin as high, CSS555 resets on low
digitalWrite(0,HIGH);
digitalWrite(1,HIGH);
digitalWrite(5,HIGH);
digitalWrite(15,HIGH);
digitalWrite(16,HIGH);


// CALCULATE AIR PUMP CUTOFF TIME
AirPumpSleepEnd = AirPumpOff - sleepduration * 4 / 5;                    //Set cutoff to prevent air pump from running too long (max 1/5th sleep cycle over planned time)
}


void loop() {
//INITIALIZE LOOP VARIABLES
long PumpOff = 0;                                // Threshhold to turn off the pump 
long PumpOn = 0;                                 // Threshhold to turn on the pump
int OnVal = 0;                                   // Potentiometer reading for gap between waterings (input A7)
int OffVal = 0;                                  // Potenitometer reading for duration of waterings (input A6)
float PumpOnVal = 0.0;                           // Output of floating point calculation for threshhold to turn off water pump, above the minimum value 
float PumpOffVal = 0.0;                          // Output of floating point calculation for threshhold to turn on the water pump, above the minimum value


//BLINK LED TO SHOW NEW LOOP STARTED
digitalWrite(RXLED,LOW);                      // Turn on RXLED
delay(200);                                   // wait 
digitalWrite(RXLED,HIGH);                     // Turn off RXLED
delay(200);                                   // wait


//READ POTENTIOMETERS & CALCULATE PUMPDUR, PUMPGAP
OnVal = analogRead(OnPot);
OffVal = analogRead(OffPot);

PumpOnVal = (float)OnVal / 1023.0 * (MinPumpOn - MaxPumpOn);             // Calculate threshold at which the water pump will start
PumpOn = long(PumpOnVal) + long(MaxPumpOn);

PumpOffVal = (float)OffVal / 1023.0 * (MinPumpOff - MaxPumpOff);         // Calculate threshold at which the water pump will stop
PumpOff = long(PumpOffVal) + long(MaxPumpOff);


//CONFIRM RESERVOIR HAS WATER
if(analogRead(AngledReservoir) > 500){
  if(analogRead(BigReservoir) > 500){
    
      //BLINK LED TO SHOW PUMP ALGORITHM STARTED
      TXLED1;                                       // Turn on TXLED
      delay(200);                                   // wait 
      TXLED0;                                       // Turn off TXLED
      delay(200);                                   // wait
  
      
          //CHECK SOIL MOISTURE & TURN ON/OFF PUMPS ACCORDINGLY
      AvgMoistureLevel = readSoilSensors(AngledSensor1,AngledSensor2,BigSensor1,BigSensor2);               // Note the average soil moisture level (low value = high moisture content)
      
            
      if (digitalRead(WaterPump) == 1){
        if (AvgMoistureLevel < PumpOff){                                       // If water pump is on, and the average soil moisture content sensed is above the "wet" threshhold...
          delay(100);                                                          // Wait 0.1 second, then verify soil moisture levels again
          AvgMoistureLevel = readSoilSensors(AngledSensor1,AngledSensor2,BigSensor1,BigSensor2);
          if(AvgMoistureLevel < PumpOff){                                      // If soil moisture is confirmed to be above the "wet" threshhold            
            softstop(WaterPump,1024);                                          // Turn off water pump (~1s rampdown)
          }
        } 
      }
      
      else if (digitalRead(WaterPump) == 0){                                                                    
        if (AvgMoistureLevel > PumpOn){                                        // If water pump is off, and the average soil moisture content sensed is below the "dry" threshhold...
          delay(100);                                                          // Wait 0.1 second, then verify soil moisture levels again
          AvgMoistureLevel = readSoilSensors(AngledSensor1,AngledSensor2,BigSensor1,BigSensor2);
          if (AvgMoistureLevel > PumpOn){                                      // If soil moisture is confirmed to be below "dry" threshhold
            TXLED1;                                                            // Blink RXLED 2x to note pump is being turned on
            delay(100);                                                        
            TXLED0;                                          
            delay(100);
            digitalWrite(RXLED,LOW);                                           
            delay(100);                                                        
            digitalWrite(RXLED,HIGH);
            delay(100);                     
            softstart(WaterPump,1024);                                         // Turn on water pump (~1s rampup)
          }
        }
      }
      
      else{                                                                    // Else, if the pump somehow is in another "state," default to off
        digitalWrite(WaterPump,LOW);
      }
/*
      //CHECK RUNTIME & TURN ON/OFF AIRPUMP ACCORDINGLY
      time_now = millis();                                                     //Capture millis function value
      runtime = time_now - runtime_start + sleepcycles * sleepduration;        //Calculate length of time since water pump initially turned on, include duration of LED blinks in sleep duration)
      
      if (runtime_start < 1){
        digitalWrite(Reset,LOW);                                               //Reset CSS555 timer
        digitalWrite(Reset,HIGH);
        runtime_start = time_now;                                              //Reset time at which pump has been turned on
        digitalWrite(AirPump,HIGH);                                            //Turn on air pump
      }

      else if (runtime < AirPumpSleepEnd){
          digitalWrite(AirPump,HIGH);                                          //Confirm air pump is still on
          
          if (digitalRead(WaterPump)== 0){       
            sleepcycles = sleepcycles + 1;                                       //Increment sleep counter
            MCUsleep();                                                          //Go to sleep
            MCUwake();                                                           //Wake up  
          }
      } 

      else if (runtime <= AirPumpOff){                                         //If water pump has completed its user input duration, yet air pumps still are to be on, turn water pump off (or confirm off)
        digitalWrite(AirPump,HIGH);                                            //Confirm air pump is still on
      }

      else if (runtime < (AirPumpOff + PumpGap)){                              //If total runtime is less than duration for pump on time plus pump off time set by user, remain off
        digitalWrite(AirPump,LOW);                                             //Turn off air pump, confirm water pump is still off
 
        if (digitalRead(WaterPump)== 0){       
            sleepcycles = sleepcycles + 1;                                       //Increment sleep counter
            MCUsleep();                                                          //Go to sleep
            MCUwake();                                                           //Wake up  
        }
      }

      else {
        digitalWrite(AirPump,LOW);                                             //Confirm air pump is off
        runtime_start = 0;                                                     //Reset runtime_start time to re-initiate pump if statements
        sleepcycles = 0;                                                       //Reset sleep cycle counter
      }  

*/
    }
    //IF BIG RESERVOIR IS LOW, TURN OFF PUMPS
    else{
      delay(200);
      if (analogRead(BigReservoir) <= 500){                                             // Wait 0.2 second, then confirm reservoir level
          if (digitalRead(WaterPump) == 1){
              softstop(WaterPump,1024);                                                // Turn off water pump (~1s rampdown), air pump
              digitalWrite(AirPump,LOW); 
          }
          
          else {
              digitalWrite(WaterPump,LOW);
              digitalWrite(AirPump,LOW);
          }
      }
      MCUsleep();
      MCUwake();
    }
}


//IF ANGLED RESERVOIR IS LOW, TURN OFF PUMPS
else{
  delay(200);                                
  if (analogRead(AngledReservoir) <= 500){                                            // Wait 0.2 second, then confirm reservoir level
      if (digitalRead(WaterPump) == 1){
          softstop(WaterPump,1024);                                                // Turn off water pump (~1s rampdown), air pump
          digitalWrite(AirPump,LOW); 
      }
      
      else {
          digitalWrite(WaterPump,LOW);
          digitalWrite(AirPump,LOW);
      }
  }
  MCUsleep();
  MCUwake();
}
}


//SUB-FUNCTION TO MEASURE FOUR SOIL MOISTURE LEVELS, RETURNING THE VALUE OF THE "WETTEST" SENSOR - I.E. THE MINIMUM VALUE
int readSoilSensors (int Sensor1, int Sensor2, int Sensor3, int Sensor4){
 
Sensor1Val = analogRead(Sensor1);                                            // Read soil moisture sensors
Sensor2Val = analogRead(Sensor2); 
Sensor3Val = analogRead(Sensor3);
Sensor4Val = analogRead(Sensor4);
int output = (Sensor1Val + Sensor2Val + Sensor3Val + Sensor4Val) / 4 ;                                     // Note the average soil moisture level (low value = high moisture content)
//int output = min(Sensor1Val, Sensor2Val, Sensor3Val, Sensor4Val);                                     //Note maximum soil moisture level (low value = high moisture content)
return output;                                              
}


// SUB-FUNCTION TO MINMIZE SPIKES IN CURRENT DRAW BY LINEARLY INCREASING MOTOR DUTY CYCLE OVER DURATION OF "RAMPUP" MILLISECONDS
void softstart(int PinNum, int rampup){

dwell = max(floor(rampup / 254), 2);                                          // Compute time to dwell at each duty cycle step [~500Hz frequency = 2ms cycle time, 0-255 duty cycle range] 

for(ii = 1; ii <= 254; ii++){                                                // Linearly increase duty cycle over duration specified by "rampup" - minimum 2ms dwell time to allow at least 1 cycle per step
  analogWrite(PinNum,ii);
  delay(dwell);
}

digitalWrite(PinNum,HIGH);                                                   // Leave pin "on" (high)
}


// SUB-FUNCTION TO MINMIZE SPIKES IN VOLTAGE BY LINEARLY DECREASING MOTOR DUTY CYCLE OVER DURATION OF "RAMPDOWN" MILLISECONDS
void softstop(int PinNum, int rampdown){

dwell = max(floor(rampdown / 254),2);                                        // Compute time to dwell at each duty cycle step [~500Hz frequency = 2ms cycle time, 0-255 duty cycle range] 

for(ii = 254; ii >= 1; ii--){                                                // Linearly decrease duty cycle over duration specified by "rampdown" - minimum 2ms dwell time to allow at least 1 cycle per step
  analogWrite(PinNum,ii);
  delay(dwell);
}

digitalWrite(PinNum,LOW);                                                    // Leave pin "off" (low)
}


//  SUB-FUNCTION TO PUT MCU INTO DEEP-SLEEP MODE
void MCUsleep(){  
      digitalWrite(TimerTrigger,LOW);                                           // Start CSS555 timer
      digitalWrite(TimerTrigger,HIGH);                                          // Reset CSS555 timer start pin
    
      attachInterrupt(digitalPinToInterrupt(Wakeup),SleepRoutine,FALLING);      // Attach interrupt to wake up MCU
      SMCR = B00000100;                                                         // Set to Power Down mode
      SMCR |= 1;                                                                // Enable sleep operator
      asm  volatile("sleep");                                                   // Go to sleep 
}


//  SUB-FUNCTION TO WAKE UP MCU FROM DEEP-SLEEP MODE
void MCUwake(){
      //Wake Up via 555 timer, detach interrupt
      SMCR = 0;                                                                 // Reset Sleep Mode Control Register
      detachInterrupt(digitalPinToInterrupt(Wakeup));
      delay(100);                                                               // Give MCU time to wake up
}

void SleepRoutine() {
}
