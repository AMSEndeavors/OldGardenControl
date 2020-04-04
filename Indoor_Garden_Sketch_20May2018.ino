
/* Automated garden watering and water aeration system.
Created by: Tony Swanson
Date: 18 Nov 2017

Last Modified by: Tony Swanson
Date: 20 May 2018

Functions:
   *  Reads two capacitive moisture sensors (with outputs shifted and amplified to approximately a 0-5v range).  0v = wet = 100% humidity. 5v = dry = 0% humidity.
   *  Reads two input potentiometers (with 0-5v range) to set maximum and minimum soil moisture threshholds. 0v = Minimum value. 5v = Maximum value.
   *  Reads a debounced float switch to verify water reservoir level (analog input; >500 [of 1023] is full)
   *  Turns on a water pump as needed to maintain soil moisture level within bounds set by potentiometers
   *  Turns on air pumps and circulation pumps (on same circuit) if reservoir contains water
   *  Turns off all pumps (water, air, circulation) if reservoir is depleted
*/


// INITIALIZE GLOBAL VARIABLES
const float MaxPumpOff = 21.0;                  // Sets maximum threshhold to turn off water pump, currently set to 0.1V (21 of 1023) - "max" refers to at wettest point
const float MinPumpOff = 409.0;                 // Sets minimum threshhold to turn off water pump, currently set to 1.0V (205 of 1023)
const float MaxPumpOn = 327.0;                  // Sets maximum threshhold to turn on water pump, currently set to 0.5V (103 of 1023)
const float MinPumpOn = 818.0;                  // Sets minimum threshhold to turn on water pump, currently set to 2.5V (511 of 1023)


// INITIALIZE PIN NAMES
const int RXLED = 17;                           // sets RX led to pin 17 - used to blink as notifier program is running and has entered a new loop
const int Reservoir = A9;                       // sets pin 9 (aka "A9") to read the float sensor in the smaller reservoir (closed = empty = pin pulled above 2.5v, value >500)
const int WaterPump = 3;                        // sets pin 3 to actuate the water pump - HIGH is "On," LOW is "Off"
const int AirPump = 21;                         // sets pin 21 (aka "A3") to actuate the air pump - HIGH is "On," LOW is "Off"
const int OnPot = A7;                           // set pins 4 (aka "A6") & 6 (aka "A7") to read potentiometers
const int OffPot = A6;                  
const int MSensorBlu = A0;                      // sets pin A0 to read output from moisture sensor with blue input wire. 0 = wet , 1023 = dry
const int MSensorWht = A10;                     // sets pin 10 (aka "A10") to read output from moisture sensor with white input wire. 0 = wet , 1023 = dry


// INITIALIZE SUB-FUNCTION VARIABLES
int Sensor1Val = 0;                             // Reading from 1st moisture sensor called within "readSoilSensors" function
int Sensor2Val = 0;                             // Reading from 2nd moisture sensor called within "readSoilSensors" function
int MaxMoistureLevel = 0;                       // The "wettest" of the two moisture sensor values 
int dwell = 0;                                  // Duration of time spent at each duty cycle step while soft-starting the pump motor (254 steps, which excludes "0" [0% / off] and "255" [100% / on])
int ii = 0;                                     // Counter used by 'FOR' loop in the pump motor soft-start routine


void setup() {


//SET PIN INPUTS/OUTPUTS
pinMode(RXLED, OUTPUT);                         // Set pin as output, to control RX LED
pinMode(Reservoir,INPUT);                       // Set pin as input, to read float sensor
pinMode(WaterPump,OUTPUT);                      // Set pin as output, to control water pump; HIGH is "On," LOW is "Off"
pinMode(AirPump,OUTPUT);                        // Set pin as output, to control air pump; HIGH is "On," LOW is "Off"
pinMode(OnPot,INPUT);                           // Set pin as input, to read watering gap input potentiometer
pinMode(OffPot,INPUT);                          // Set pin as input, to read watering duration input potentiometer
pinMode(MSensorBlu,INPUT);                      // Set pin as input, to read moisture sensor with blue input wire.
pinMode(MSensorWht,INPUT);                      // Set pin as input, to read moisture sensor with white input wire.


//SET PIN STATES
digitalWrite (RXLED, HIGH);                      // Set pin as high (off)
digitalWrite(WaterPump,LOW);                     // Set pin as low (pump off)
digitalWrite(AirPump,LOW);                       // Set pin as low (pump off)
}


void loop() {
//INITIALIZE LOOP VARIABLES
long PumpOff = 0;                                // Threshhold to turn off the pump 
long PumpOn = 0;                                 // Threshhold to turn on the pump
int ReservoirLevel = 0;                          // Water level reading for reservoir, > 500 = full, =< 500 = empty
int OnVal = 0;                                   // Potentiometer reading for gap between waterings (input A7)
int OffVal = 0;                                  // Potenitometer reading for duration of waterings (input A6)
float PumpOnVal = 0.0;                           // Output of floating point calculation for threshhold to turn off water pump, above the minimum value 
float PumpOffVal = 0.0;                          // Output of floating point calculation for threshhold to turn on the water pump, above the minimum value


//BLINK LED TO SHOW NEW LOOP STARTED
   digitalWrite(RXLED,LOW);                      // Turn on RXLED
   delay(100);                                   // wait 
   digitalWrite(RXLED,HIGH);                     // Turn off RXLED
   delay(100);                                   // wait


//READ RESERVOIR SENSOR & PUMP STATES
ReservoirLevel = analogRead(Reservoir);


//READ POTENTIOMETERS & CALCULATE PUMPDUR, PUMPGAP
    OnVal = analogRead(OnPot);
    OffVal = analogRead(OffPot);

    PumpOnVal = (float)OnVal / 1023.0 * (MinPumpOn - MaxPumpOn);             // Calculate threshold at which the water pump will start
    PumpOn = long(PumpOnVal) + long(MaxPumpOn);

    PumpOffVal = (float)OffVal / 1023.0 * (MinPumpOff - MaxPumpOff);         // Calculate threshold at which the water pump will stop
    PumpOff = long(PumpOffVal) + long(MaxPumpOff);


//CONFIRM RESERVOIR HAS WATER
if(ReservoirLevel < 500){
  
    digitalWrite(AirPump,HIGH);

    
        //CHECK SOIL MOISTURE & TURN ON/OFF PUMPS ACCORDINGLY
    MaxMoistureLevel = readSoilSensors(MSensorBlu,MSensorWht);               // Note the maximum soil moisture level (low value = high moisture content)
    
          
    if (digitalRead(WaterPump) == 1){
      if (MaxMoistureLevel < PumpOff){                                       // If water pump is on, and the "wettest" sensor is above the "wetness" threshhold...
        delay(100);                                                          // Wait 0.1 second, then verify soil moisture levels again
        MaxMoistureLevel = readSoilSensors(MSensorBlu,MSensorWht);
        if(MaxMoistureLevel < PumpOff){                                      // If soil moisture is confirmed to be above the "wetness" threshhold            
          softstop(WaterPump,1024);                                          // Turn off water pump (~1s rampdown)
        }
      } 
    }
    
    else if (digitalRead(WaterPump) == 0){                                                                    
      if (MaxMoistureLevel > PumpOn){                                        // If water pump is off, and the "wettest" sensor is below the "dryness" threshhold...
        delay(100);                                                          // Wait 0.1 second, then verify soil moisture levels again
        MaxMoistureLevel = readSoilSensors(MSensorBlu,MSensorWht);
        if (MaxMoistureLevel > PumpOn){                                      // If soil moisture is confirmed to be below "dryness" threshhold
          digitalWrite(RXLED,LOW);                                           // Blink RXLED 2x to note pump is being turned on
          delay(200);                                                        
          digitalWrite(RXLED,HIGH);                                          
          delay(200);
          digitalWrite(RXLED,LOW);                                           
          delay(200);                                                        
          digitalWrite(RXLED,HIGH);
          delay(200);                     
          softstart(WaterPump,1024);                                         // Turn on water pump (~1s rampup)
        }
      }
    }
    
    else{                                                                    // Else, if the pump somehow is in another "state," default to off
      digitalWrite(WaterPump,LOW);
    }
}
  


//IF RESERVOIR IS LOW, TURN OFF PUMPS
else{
  delay(100);
  ReservoirLevel = analogRead(Reservoir);                                    // Wait 0.1 second, then confirm reservoir level
  if (ReservoirLevel > 500){
      if (digitalRead(WaterPump) == 1){
          softstop(WaterPump,1024);                                                // Turn off water pump (~1s rampdown), air pump
          digitalWrite(AirPump,LOW); 
      }
      
      else {
          digitalWrite(WaterPump,LOW);
          digitalWrite(AirPump,LOW);
      }
      
  }
}
}


//SUB-FUNCTION TO MEASURE TWO SOIL MOISTURE LEVELS, RETURNING THE VALUE OF THE "WETTER" SENSOR - I.E. THE MINIMUM VALUE
int readSoilSensors (int Sensor1, int Sensor2){
 
Sensor1Val = analogRead(Sensor1);                                            // Read soil moisture sensors
Sensor2Val = analogRead(Sensor2); 
int output = min(Sensor1Val,Sensor2Val);                                     // Note the maximum soil moisture level (low value = high moisture content)

return output;                                              
}


// SUB-FUNCTION TO MINMIZE SPIKES IN CURRENT DRAW BY LINEARLY INCREASING MOTOR DUTY CYCLE OVER DURATION OF "RAMPUP" MILLISECONDS
void softstart(int PinNum, int rampup){

dwell = max(floor(rampup / 254),2);                                          // Compute time to dwell at each duty cycle step [~500Hz frequency = 2ms cycle time, 0-255 duty cycle range] 

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

