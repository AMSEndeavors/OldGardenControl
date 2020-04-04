/*
Author: Tony Swanson
Date 24 May 2018

Firmware to operate fertilizer dispenser:
  1) Sleep (at user's command)
  2) Prime tanks
  3) Dispsense fertilizer based on
     a) volume of water
     b) mix of fertilizers
     c) desired concentration levels

  Uses SPI1 pins for OLED screen control. D1 = Reset
*/


/***************************************************
  This is a library for the 1.5" 16-bit Color OLED with SSD1351 driver chip
  Pick one up today in the adafruit shop!
  ------> http://www.adafruit.com/products/684

  These displays use SPI to communicate, 4 or 5 pins are required to
  interface

  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!
  Written by Limor Fried/Ladyada for Adafruit Industries.
  BSD license, all text above must be included in any redistribution
 ****************************************************/
 #include <math.h>
 #include <Adafruit_mfGFX.h>
 #include <Adafruit_SSD1351.h>


// INITIALIZE GLOBAL VARIABLES
#define sclk D4
#define mosi D2
#define cs   D5
#define rst  D1
#define dc   D3


// Color definitions
#define	BLACK           0x0000
#define	BLUE            0x001F
#define	RED             0xF800
#define	GREEN           0x07E0
#define CYAN            0x07FF
#define MAGENTA         0xF81F
#define YELLOW          0xFFE0
#define WHITE           0xFFFF
#define GREY            0x738E

Adafruit_SSD1351 tft = Adafruit_SSD1351(cs, dc, mosi, sclk, rst);


/* someday, see if there's a way to use SPI harware pins such as with this option
// Option 2: must use the hardware SPI pins
// (for UNO thats sclk = 13 and sid = 11) and pin 10 must be
// an output. This is much faster - also required if you want
// to use the microSD card (see the image drawing example)
//Adafruit_SSD1351 display = Adafruit_SSD1351(cs, dc, rst);
*/


// INITIALIZE PIN NAMES
const int Tank1 = D0;                                           // Controls tank #1 solenoid. On = high = open.
const int Tank2 = DAC;
const int Tank3 = A5;
const int Tank4 = A4;
const int Tank5 = A3;
const int Tank6 = A2;
const int Tank7 = A1;
const int Tank8 = RX;                                           // Used for water tank (to clear lines)
const int Pump = TX;
const int Select = D6;                                          // Set pin D6 as Green, N/O push-button switch used to enter data
const int InPot = A0;                                           // Set pin A0 to read potentiometer
const int Tanks[] = {Tank1, Tank2, Tank3, Tank4, Tank5, Tank6, Tank7, Tank8};  // Tank 1 = , Tank 2 = , Tank 3 = , Tank 4 = , Tank 5 = , Tank 6 = , Tank 7 = , Tank 8 = Water;
const int TanksLen = sizeof(Tanks)/sizeof(Tanks[0]);
const int Mixture[4][7] = {                                     // Rows = tanks used for a given "mix," columns = binary y/n
    {0, 1, 0, 0, 0, 0, 0},                                          // "Clean" mix
    {1, 0, 0, 0, 0, 1, 1},                                          // "Seed" mix
    {1, 0, 0, 1, 0, 1, 1},                                          // "Grow" mix
    {1, 0, 0, 0, 1, 1, 1}                                           // "Bloom" mix
};
const int Potency[3][7] = {                                     // Rows = concentration level (above base), columns = mL/gal per tank
    {2, 10, 10, 15, 25, 2, 2},                                      // Low (base level) concentration
    {5, 10, 10, 18, 28, 5, 2},                                      // Medium concentration
    {8, 10, 10, 20, 30, 8, 2}                                       // High concentration
};
const int msPermL = 254;                                        //Time in milliseconds the peristaltic pump needs to run to deliver 1 ml from a tank
const int flush = 20000;                                        //Time in milliseconds the peristaltic pump needs to flush the lines after delivering a dose


// INITIALIZE SUB-FUNCTION VARIABLES
int dwell = 0;                                                  // Duration of time (in microseconds) spent at each duty cycle step while soft-starting the pump motor (254 steps, which excludes "0" [0% / off] and "255" [100% / on])
int ii = 0;                                                     // Counter used by 'FOR' loop in the pump motor soft-start/soft-stop routines
int jj = 0;                                                     // Counter used in 'FOR' loops to set pin modes/states


//INITIALIZE MENU VARIABLES
int Menu = 0;
int MenuLast = 100;                                             // set as non-feasible menu number to ensure initial menu option is rendered
int MenuInput = 0;
int MenuInputLast = 100;                                        // set as non-feasible menu number to ensure initial menu option is rendered
char buffer[10];                                                // buffer for displaying tank number in Menu case 2
int Volume = 0;                                                 // Volume (in gallons) of solution to be made
int Mix = 0;                                                    // Row of the "Mixture" matrix to be used
int Conc = 0;                                                   // Row of the "Potency" matrix to be used
int PumpDur = 0;                                                // Duration of time to run peristaltic pump while dispensing from a tank (in milliseconds)


void setup()
{
    tft.begin();
    //DISPLAY TEST PATTERN TO SHOW WE'VE ENTERED SETUP
    lcdTestPattern();
    STARTUP(WiFi.selectAntenna(ANT_EXTERNAL));
    delay(1000);

    //SET PIN INPUTS/OUTPUTS & STATES
    for (jj = 0; jj<TanksLen; jj++)
    {
        pinMode(Tanks[jj], OUTPUT);                                  // Set tank pins as output, to control LEDs
        digitalWrite(Tanks[jj], LOW);                                // Confirm all outputs are off
    }

    pinMode(Pump, OUTPUT);
    digitalWrite(Pump, LOW);
    pinMode(Select, INPUT_PULLDOWN);                                // Set pin as input, to read debounced, n/o pushbutton switch. High = closed
    pinMode(InPot, INPUT);                                          // Set pin as input, to read Potentiometer.

    delay(500);
    tft.fillScreen(BLACK);

}

void loop()
{
    switch (Menu)
    {
        case 0:                                                     // Main Menu
            if (Menu != MenuLast) {MenuLast = Menu;}                // Only render menu header text if it's new

            if (digitalRead(Select) == 1)                           // If button is pushed:
            {
                Menu = ceil(((float)analogRead(InPot) + 1.0) / 4097.0 * 3.0);    // Advance to next menu
                case0select (Menu);
                MenuInputLast = 100;                                // set as non-feasible menu number to ensure subsequent menu option is rendered
                delay (2000);
                tft.fillScreen(BLACK);
            }

            else
            {
                MenuInput = ceil(((float)analogRead(InPot) + 1.0) / 4097.0 * 3.0);     // Determine which menu option to display
                case0menu(MenuInput, MenuInputLast);                // Display menu option
                MenuInputLast = MenuInput;                          // Reset previous menu value
            }
            break;

        case 1:                                                     // Sleep Menu
            if (Menu != MenuLast)                                   // Only render menu header text if it's new
            {
                tft.fillRect(0, 0, 128, 22, GREY);
                tft.setCursor(4,4);                                 // Render menu header text
                tft.setTextColor(YELLOW);
                tft.setTextSize(2);
                tft.print("Sleep?");
                MenuLast = Menu;                                    // Save menu display status
            }


            if (digitalRead(Select) == 1)
            {
                MenuInput = floor((float)(analogRead(InPot)) / 4096.0 * 2.0);
                case1select(MenuInput);
                Menu = 0;
                MenuInputLast = 100;
                delay(2000);
                tft.fillScreen(BLACK);

                if (MenuInput == 1)
                {
                    tft.fillScreen(BLACK);
                    tft.setCursor(5, 52);
                    tft.setTextColor(WHITE);
                    tft.print("G'night!");
                    delay(2000);
                    tft.fillScreen(BLACK);
                    System.sleep(SLEEP_MODE_DEEP, 32000000);
                }
            }

            else
            {
                MenuInput = floor((float)analogRead(InPot) / 4096.0 * 2.0);
                case1menu(MenuInput, MenuInputLast);
                MenuInputLast = MenuInput;
            }
            break;

        case 2:                                                     // Prime Pump Menu
            if (Menu != MenuLast)                                   // Only render menu header text if it's new
            {
                tft.fillRect(0, 0, 128, 22, GREY);
                tft.setCursor(4,4);                                 // Render menu header text
                tft.setTextColor(YELLOW);
                tft.setTextSize(2);
                tft.print("Prime:");
                MenuLast = Menu;
            }

            if (digitalRead(Select) == 1)
            {
               MenuInput = floor((float)analogRead(InPot) / 4096.0 * 9.0);
                if (MenuInput == 0)
                {
                    case2select(MenuInput);
                    Menu = 0;
                    MenuInputLast = 100;

                    for (jj = 0; jj<TanksLen; jj++)                 // Confirm all outputs are off
                    {
                        digitalWrite(Tanks[jj], LOW);
                    }

                    if (digitalRead(Pump) == HIGH) {softstop(Pump, 100);}
                    delay(2000);
                    tft.fillScreen(BLACK);
                }

                else
                {
                    case2select(MenuInput);
                    digitalWrite(Tanks[MenuInput - 1], HIGH);
                    if (digitalRead(Pump) != HIGH) {softstart(Pump, 100);}
                }
            }

            else if (digitalRead(Select) == 0)
            {
                if (digitalRead(Pump) == HIGH)
                {
                    softstop(Pump, 100);
                    MenuInputLast = 100;
                }

                for (jj = 0; jj<TanksLen; jj++)                     // Confirm all outputs are off
                {
                    digitalWrite(Tanks[jj], LOW);
                }
                MenuInput = floor((float)analogRead(InPot) / 4096 * 9);
                case2menu(MenuInput, MenuInputLast);
                MenuInputLast = MenuInput;
            }
            break;

        case 3:                                                       // Dispense menu L0 - Volume
            if (Menu != MenuLast)                                     // Only render menu header text if it's new
            {
                tft.fillRect(0, 0, 32, 16, GREY);
                tft.drawRect(0, 0, 32, 16, YELLOW);
                tft.drawRect(37, 0, 32, 16, GREY);
                tft.drawRect(74, 0, 32, 16, GREY);
                tft.drawTriangle(111, 0, 127, 8, 111, 15, GREY);
                tft.setCursor(13, 5);
                tft.setTextColor(YELLOW);
                tft.setTextSize(1);
                tft.print("V");
                tft.setCursor(50, 5);
                tft.setTextColor(GREY);
                tft.print("M");
                tft.setCursor(87, 5);
                tft.print("P");
                tft.setCursor(113, 5);
                tft.print("D");
                tft.setTextColor(YELLOW);
                tft.setTextSize(2);
                tft.setCursor(13, 18);
                tft.print("Volume?");
                MenuLast = Menu;
            }

            if (digitalRead(Select) == 1)
            {
                MenuInput = floor((float)analogRead(InPot) / 4096 * 21);
                case3select(MenuInput);

                if (MenuInput != 0)
                {
                    Volume = MenuInput;
                    Menu = 4;
                    MenuInputLast = 100;
                    delay(2000);
                    tft.fillScreen(BLACK);
                }

                else
                {
                    Menu = 0;
                    MenuInputLast = 100;
                    delay(2000);
                    tft.fillScreen(BLACK);
                }
            }

            else
            {
                MenuInput = floor((float)analogRead(InPot) / 4096 * 21);
                case3menu(MenuInput, MenuInputLast);
                MenuInputLast = MenuInput;
            }
            break;

        case 4:                                                 // Dispense Menu L1 - Mix
            if (Menu != MenuLast)                                   // Only render menu header text if it's new
            {
                tft.fillRect(0, 0, 32, 16, WHITE);
                tft.drawRect(0, 0, 32, 16, RED);
                tft.fillRect(37, 0, 32, 16, GREY);
                tft.drawRect(37, 0, 32, 16, YELLOW);
                tft.drawRect(74, 0, 32, 16, GREY);
                tft.drawTriangle(111, 0, 127, 8, 111, 15, GREY);
                tft.setCursor(13, 5);
                tft.setTextColor(RED);
                tft.setTextSize(1);
                tft.print("V");
                tft.setTextColor(YELLOW);
                tft.setCursor(50, 5);
                tft.print("M");
                tft.setCursor(87, 5);
                tft.setTextColor(GREY);
                tft.print("P");
                tft.setCursor(113, 5);
                tft.print("D");
                tft.setTextColor(YELLOW);
                tft.setTextSize(2);
                tft.setCursor(13, 18);
                tft.print("Mix?");
                MenuLast = Menu;
            }

            if (digitalRead(Select) == 1)
            {
                MenuInput = floor((float)analogRead(InPot) / 4096 * 5);
                case4select(MenuInput);

                if (MenuInput != 0)                                 // Save mixture row index & advance to next menu
                {
                    Mix = MenuInput - 1;
                    Menu = 5;
                    MenuInputLast = 100;
                    delay(2000);
                    tft.fillScreen(BLACK);
                }

                else                                                // Go back
                {
                    Menu = 3;
                    MenuInputLast = 100;
                    delay(2000);
                    tft.fillScreen(BLACK);
                }
            }

            else
            {
                MenuInput = floor((float)analogRead(InPot) / 4096 * 5);
                case4menu(MenuInput, MenuInputLast);
                MenuInputLast = MenuInput;
            }
            break;

        case 5:                                                 // Dispense Menu L2 - Potency
            if (Menu != MenuLast)                                   // Only render menu header text if it's new
            {
                tft.fillRect(0, 0, 32, 16, WHITE);
                tft.drawRect(0, 0, 32, 16, RED);
                tft.fillRect(37, 0, 32, 16, WHITE);
                tft.drawRect(37, 0, 32, 16, RED);
                tft.fillRect(74, 0, 32, 16, GREY);
                tft.drawRect(74, 0, 32, 16, YELLOW);
                tft.drawTriangle(111, 0, 127, 8, 111, 15, GREY);
                tft.setCursor(13, 5);
                tft.setTextColor(RED);
                tft.setTextSize(1);
                tft.print("V");
                tft.setCursor(50, 5);
                tft.print("M");
                tft.setCursor(87, 5);
                tft.setTextColor(YELLOW);
                tft.print("P");
                tft.setTextColor(GREY);
                tft.setCursor(113, 5);
                tft.print("D");
                tft.setTextColor(YELLOW);
                tft.setTextSize(2);
                tft.setCursor(13, 18);
                tft.print("Potency?");
                MenuLast = Menu;
            }

            if (digitalRead(Select) == 1)
            {
                MenuInput = floor((float)analogRead(InPot) / 4096 * 4);
                case5select(MenuInput);

                if (MenuInput != 0)                                 // Save concentration row index & advance to next menu
                {
                    Conc = MenuInput - 1;
                    Menu = 6;
                    MenuInputLast = 100;
                    delay(2000);
                    tft.fillScreen(BLACK);
                }

                else                                                // Go back
                {
                    Menu = 4;
                    MenuInputLast = 100;
                    delay(2000);
                    tft.fillScreen(BLACK);
                }
            }

            else
            {
                MenuInput = floor((float)analogRead(InPot) / 4096 * 4);
                case5menu(MenuInput, MenuInputLast);
                MenuInputLast = MenuInput;
            }
            break;

        case 6:                                                 // Dispense Menu L3 - Dispense
            if (Menu != MenuLast)                                   // Only render menu header text if it's new
            {
                tft.fillRect(0, 0, 32, 16, WHITE);
                tft.drawRect(0, 0, 32, 16, RED);
                tft.fillRect(37, 0, 32, 16, WHITE);
                tft.drawRect(37, 0, 32, 16, RED);
                tft.fillRect(74, 0, 32, 16, WHITE);
                tft.drawRect(74, 0, 32, 16, RED);
                tft.fillTriangle(111, 0, 127, 8, 111, 15, GREY);
                tft.drawTriangle(111, 0, 127, 8, 111, 15, YELLOW);
                tft.setCursor(13, 5);
                tft.setTextColor(RED);
                tft.setTextSize(1);
                tft.print("V");
                tft.setCursor(50, 5);
                tft.print("M");
                tft.setCursor(87, 5);
                tft.print("P");
                tft.setCursor(113, 5);
                tft.setTextColor(YELLOW);
                tft.print("D");
                tft.setTextSize(2);
                tft.setCursor(13, 18);
                tft.print("Dispense?");
                MenuLast = Menu;
            }

            if (digitalRead(Select) == 1)
            {
                MenuInput = floor((float)analogRead(InPot) / 4096 * 3);
                case6select(MenuInput);

                switch (MenuInput)
                {
                    case 0:                                         // Go back
                        Menu = 5;
                        MenuInputLast = 100;
                        delay(2000);
                        tft.fillScreen(BLACK);
                        break;

                    case 1:                                         // Dispense mixture & return to main menu
                        Menu = 0;
                        MenuInputLast = 100;
                        delay(2000);
                        tft.fillScreen(BLACK);
                        tft.setCursor(0, 15);
                        tft.setTextColor(WHITE);
                        tft.print("DISPENSING");

                        for (jj = 0; jj < (TanksLen - 1); jj++)
                        {
                            PumpDur = msPermL * Volume * Mixture[Mix][jj] * Potency[Conc][jj];  // Dispense from tanks 1-7
                            if (PumpDur > 0)
                            {
                                tft.fillRect(0, 50, 128, 78, BLACK);
                                tft.setCursor(30, 50);
                                tft.print("Tank ");
                                tft.print(jj + 1);
                                digitalWrite(Tanks[jj], HIGH);                //open solenoid valve & dispense
                                softstart(Pump, 100);
                                delay(PumpDur);
                                softstop(Pump, 100);
                                digitalWrite(Tanks[jj], LOW);
                            }
                        }
                        digitalWrite(Tank8, HIGH);                    // Clear the line
                        softstart(Pump, 100);
                        delay(flush);
                        softstop(Pump, 100);
                        digitalWrite(Tank8, LOW);

                        tft.fillScreen(BLACK);
                        tft.setCursor(30, 57);
                        tft.print("DONE!");
                        delay(2000);
                        tft.fillScreen(BLACK);
                        break;

                    case 2:                                         // Return to Main Menu
                        Menu = 0;
                        MenuInputLast = 100;
                        delay(2000);
                        tft.fillScreen(BLACK);
                        break;
                }
            }

            else
            {
                MenuInput = floor((float)analogRead(InPot) / 4096 * 3);
                case6menu(MenuInput, MenuInputLast);
                MenuInputLast = MenuInput;
            }
            break;
    }
}





//DISPLAY TEST PATTERN
void lcdTestPattern(void)
{
  uint32_t i,j;
  tft.goTo(0, 0);

  for(i=0;i<128;i++)
  {
    for(j=0;j<128;j++)
    {
      if(i<16){tft.writeData(RED>>8);tft.writeData(RED);}
      else if(i<32){tft.writeData(YELLOW>>8);tft.writeData(YELLOW);}
      else if(i<48){tft.writeData(GREEN>>8);tft.writeData(GREEN);}
      else if(i<64){tft.writeData(CYAN>>8);tft.writeData(CYAN);}
      else if(i<80){tft.writeData(BLUE>>8);tft.writeData(BLUE);}
      else if(i<96){tft.writeData(MAGENTA>>8);tft.writeData(MAGENTA);}
      else if(i<112){tft.writeData(BLACK>>8);tft.writeData(BLACK);}
      else {tft.writeData(WHITE>>8);tft.writeData(WHITE);}
    }
  }
}


//MENU-CASE 0 [MAIN MENU] MENU TEXT RENDERING SUB-FUNCTION
void case0menu(int input, int caselast)
{
    if (input != caselast)                                //skip rendering unless it's necessary
    {
        switch (MenuInput)
        {
            case 1:
                tft.fillScreen(BLACK);
                tft.setTextColor(YELLOW);
                tft.setTextSize(2);
                tft.fillCircle(12, 64, 12, GREY);
                tft.drawCircle(12, 64, 12, YELLOW);
                tft.setCursor(8, 57);
                tft.print("1");
                tft.setCursor(32, 57);
                tft.print("Sleep");

                break;

            case 2:
                tft.fillScreen(BLACK);
                tft.setTextColor(YELLOW);
                tft.setTextSize(2);
                tft.fillCircle(12, 64, 12, GREY);
                tft.drawCircle(12, 64, 12, YELLOW);
                tft.setCursor(8, 57);
                tft.print("2");
                tft.setCursor(32, 48);
                tft.print("Prime");
                tft.setCursor(32, 67);
                tft.print("Pump");
                break;

            case 3:
                tft.fillScreen(BLACK);
                tft.setTextColor(YELLOW);
                tft.setTextSize(2);
                tft.fillCircle(12, 64, 12, GREY);
                tft.drawCircle(12, 64, 12, YELLOW);
                tft.setCursor(8, 57);
                tft.print("3");
                tft.setCursor(32, 57);
                tft.print("Dispense");
                break;
        }
    }
}


//MENU-CASE 0 [MAIN MENU] SELECTION RENDERING SUB-FUNCTION (RENDERS TEXT AFTER SELECTION HAS BEEN MADE)
void case0select (int input)
{
    switch (input)
    {
        case 1:
            tft.fillScreen(BLACK);
            tft.setTextColor(RED);
            tft.setTextSize(2);
            tft.fillCircle(12, 64, 12, WHITE);
            tft.drawCircle(12, 64, 12, RED);
            tft.setCursor(8, 57);
            tft.print("1");
            tft.setCursor(32, 57);
            tft.print("Sleep");
            break;

        case 2:
            tft.fillScreen(BLACK);
            tft.setTextColor(RED);
            tft.setTextSize(2);
            tft.fillCircle(12, 64, 12, WHITE);
            tft.drawCircle(12, 64, 12, RED);
            tft.setCursor(8, 57);
            tft.print("2");
            tft.setCursor(32, 48);
            tft.print("Prime");
            tft.setCursor(32, 67);
            tft.print("Pump");
            break;

        case 3:
            tft.fillScreen(BLACK);
            tft.setTextColor(RED);
            tft.setTextSize(2);
            tft.fillCircle(12, 64, 12, WHITE);
            tft.drawCircle(12, 64, 12, RED);
            tft.setCursor(8, 57);
            tft.print("3");
            tft.setCursor(32, 57);
            tft.print("Dispense");
            break;
    }
}


//MENU-CASE 1 (SLEEP MENU) SUB-FUNCTION TO RENDER MENU TEXT
void case1menu(int input, int caselast)
{
    if (input != caselast)                                 //skip rendering unless needed
    {
        switch (input)
        {
            case 0:
                tft.fillRect(0, 24, 128, 104, BLACK);
                tft.setTextColor(YELLOW);
                tft.setTextSize(2);
                tft.fillCircle(12, 64, 12, GREY);
                tft.drawCircle(12, 64, 12, YELLOW);
                tft.setCursor(8, 57);
                tft.print("1");
                tft.setCursor(68, 48);
                tft.print("No!");
                tft.setCursor(32, 67);
                tft.print("<- Back");
                break;

            case 1:
                tft.fillRect(0, 24, 128, 104, BLACK);
                tft.setTextColor(YELLOW);
                tft.setTextSize(2);
                tft.fillCircle(12, 64, 12, GREY);
                tft.drawCircle(12, 64, 12, YELLOW);
                tft.setCursor(8, 57);
                tft.print("2");
                tft.setCursor(32, 57);
                tft.print("YES");
                break;
        }
    }
}


//MENU-CASE 1 (SLEEP MENU) SUB-FUNCTION TO RENDER MENU TEXT AFTER A SELECTION HAS BEEN MADE
void case1select (int input)
{
    switch (input) {
        case 0:
            tft.fillRect(0, 24, 128, 104, BLACK);
            tft.setTextColor(RED);
            tft.setTextSize(2);
            tft.fillCircle(12, 64, 12, WHITE);
            tft.drawCircle(12, 64, 12, RED);
            tft.setCursor(8, 57);
            tft.print("1");
            tft.setCursor(68, 48);
            tft.print("No!");
            tft.setCursor(32, 67);
            tft.print("<- Back");
            break;

        case 1:
            tft.fillRect(0, 24, 128, 104, BLACK);
            tft.setTextColor(RED);
            tft.setTextSize(2);
            tft.fillCircle(12, 64, 12, WHITE);
            tft.drawCircle(12, 64, 12, RED);
            tft.setCursor(8, 57);
            tft.print("2");
            tft.setCursor(32,57);
            tft.print("YES");
            break;
    }
}


//MENU-CASE 2 (PRIME PUMP MENU) SUB-FUNCTION TO RENDER MENU TEXT
void case2menu(int input, int caselast)
{
    if (input != caselast)                              //skip rendering unless it's needed
    {
        if (input != 0) {
            tft.fillRect(0, 24, 128, 104, BLACK);
            tft.setTextColor(YELLOW);
            tft.setTextSize(2);
            tft.setCursor(10, 48);
            tft.print("Tank ");
            tft.print(input);
        }

        else {
            tft.fillRect(0, 24, 128, 104, BLACK);
            tft.setTextColor(YELLOW);
            tft.setTextSize(2);
            tft.setCursor(47, 48);
            tft.print("DONE.");
            tft.setCursor(10, 67);
            tft.print("<- BACK");
        }
    }
}

//MENU-CASE 2 (PRIME PUMP MENU) SUB-FUNCTION TO RENDER MENU TEXT AFTER A SELECTION HAS BEEN MADE
void case2select (int input)
{
    if (input != 0) {
        tft.fillRect(0, 24, 128, 104, BLACK);
        tft.setTextColor(RED);
        tft.setTextSize(2);
        tft.setCursor(10, 48);
        tft.print("Tank ");
        tft.print(input);
    }

    else {
        tft.fillRect(0, 24, 128, 104, BLACK);
        tft.setTextColor(RED);
        tft.setTextSize(2);
        tft.setCursor(47, 48);
        tft.print("DONE.");
        tft.setCursor(10, 67);
        tft.print("<- BACK");
    }
}


//MENU-CASE 3 (DISPENSE MENU L0 - VOLUME) SUB-FUNCTION TO RENDER MENU TEXT
void case3menu (int input, int caselast)
{
    if (input != caselast)                             //skip rendering unless it's needed
    {
        if (input != 0) {
            tft.fillRect(0, 34, 128, 94, BLACK);
            tft.setTextColor(YELLOW);
            tft.setTextSize(2);
            tft.setCursor(32, 57);
            tft.print(input);
            tft.print(" Gal");
        }

        else {
            tft.fillRect(0, 34, 128, 94, BLACK);
            tft.setTextColor(YELLOW);
            tft.setTextSize(2);
            tft.setCursor(22, 58);
            tft.print("Go Back!");
            tft.setCursor(22, 77);
            tft.print("   <-  ");
        }
    }
}


//MENU-CASE 3 (DISPENSE MENU L0 - VOLUME) SUB-FUNCTION TO RENDER MENU TEXT AFTER A SELECTION HAS BEEN MADE
void case3select (int input)
{
    if (input != 0) {
        tft.fillRect(0, 34, 128, 94, BLACK);
        tft.setTextColor(RED);
        tft.setTextSize(2);
        tft.setCursor(32, 57);
        tft.print(input);
        tft.print(" Gal");
    }

    else {
        tft.fillRect(0, 34, 128, 94, BLACK);
        tft.setTextColor(RED);
        tft.setTextSize(2);
        tft.setCursor(22, 58);
        tft.print("Go Back!");
        tft.setCursor(22, 77);
        tft.print("   <-  ");
    }
}


//MENU-CASE 4 (DISPENSE MENU L1 - MIX) SUB-FUNCTION TO RENDER MENU TEXT
void case4menu (int input, int caselast)
{
    if (input != caselast)                               //skip rendering unless it's needed
    {
        switch (input)
        {
            case 0:
                tft.fillRect(0, 34, 128, 94, BLACK);
                tft.setTextColor(YELLOW);
                tft.setTextSize(2);
                tft.fillCircle(12, 64, 12, GREY);
                tft.drawCircle(12, 64, 12, YELLOW);
                tft.setCursor(8, 57);
                tft.print("1");
                tft.setCursor(32, 58);
                tft.print("Go Back!");
                tft.setCursor(32, 77);
                tft.print("   <-  ");
                break;

            case 1:
                tft.fillRect(0, 34, 128, 94, BLACK);
                tft.setTextColor(YELLOW);
                tft.setTextSize(2);
                tft.fillCircle(12, 64, 12, GREY);
                tft.drawCircle(12, 64, 12, YELLOW);
                tft.setCursor(8, 57);
                tft.print("2");
                tft.setCursor(32, 57);
                tft.print("Clean");
                break;

            case 2:
                tft.fillRect(0, 34, 128, 94, BLACK);
                tft.setTextColor(YELLOW);
                tft.setTextSize(2);
                tft.fillCircle(12, 64, 12, GREY);
                tft.drawCircle(12, 64, 12, YELLOW);
                tft.setCursor(8, 57);
                tft.print("3");
                tft.setCursor(32, 57);
                tft.print("Seed");
                break;

            case 3:
                tft.fillRect(0, 34, 128, 94, BLACK);
                tft.setTextColor(YELLOW);
                tft.setTextSize(2);
                tft.fillCircle(12, 64, 12, GREY);
                tft.drawCircle(12, 64, 12, YELLOW);
                tft.setCursor(8, 57);
                tft.print("4");
                tft.setCursor(32, 57);
                tft.print("Grow");
                break;

            case 4:
                tft.fillRect(0, 34, 128, 94, BLACK);
                tft.setTextColor(YELLOW);
                tft.setTextSize(2);
                tft.fillCircle(12, 64, 12, GREY);
                tft.drawCircle(12, 64, 12, YELLOW);
                tft.setCursor(8, 57);
                tft.print("5");
                tft.setCursor(32, 57);
                tft.print("Bloom");
                break;
        }
    }
}


//MENU-CASE 4 (DISPENSE MENU L1 - MIX) SUB-FUNCTION TO RENDER MENU TEXT AFTER A SELECTION HAS BEEN MADE
void case4select (int input)
{
    switch (input)
    {
        case 0:
            tft.fillRect(0, 34, 128, 94, BLACK);
            tft.setTextColor(RED);
            tft.setTextSize(2);
            tft.fillCircle(12, 64, 12, WHITE);
            tft.drawCircle(12, 64, 12, RED);
            tft.setCursor(8, 57);
            tft.print("1");
            tft.setCursor(32, 58);
            tft.print("Go Back!");
            tft.setCursor(32, 77);
            tft.print("   <-  ");
            break;

        case 1:
            tft.fillRect(0, 34, 128, 94, BLACK);
            tft.setTextColor(RED);
            tft.setTextSize(2);
            tft.fillCircle(12, 64, 12, WHITE);
            tft.drawCircle(12, 64, 12, RED);
            tft.setCursor(8, 57);
            tft.print("2");
            tft.setCursor(32, 57);
            tft.print("Clean");
            break;

        case 2:
            tft.fillRect(0, 34, 128, 94, BLACK);
            tft.setTextColor(RED);
            tft.setTextSize(2);
            tft.fillCircle(12, 64, 12, WHITE);
            tft.drawCircle(12, 64, 12, RED);
            tft.setCursor(8, 57);
            tft.print("3");
            tft.setCursor(32, 57);
            tft.print("Seed");
            break;

        case 3:
            tft.fillRect(0, 34, 128, 94, BLACK);
            tft.setTextColor(RED);
            tft.setTextSize(2);
            tft.fillCircle(12, 64, 12, WHITE);
            tft.drawCircle(12, 64, 12, RED);
            tft.setCursor(8, 57);
            tft.print("4");
            tft.setCursor(32, 57);
            tft.print("Grow");
            break;

        case 4:
            tft.fillRect(0, 34, 128, 94, BLACK);
            tft.setTextColor(RED);
            tft.setTextSize(2);
            tft.fillCircle(12, 64, 12, WHITE);
            tft.drawCircle(12, 64, 12, RED);
            tft.setCursor(8, 57);
            tft.print("5");
            tft.setCursor(32, 57);
            tft.print("Bloom");
            break;
    }

}


//MENU-CASE 5 (DISPENSE MENU L2 - CONCENTRATION) SUB-FUNCTION TO RENDER MENU TEXT
void case5menu (int input, int caselast)
{
    if (input != caselast)                               //skip rendering unless it's needed
    {
        switch (input)
        {
            case 0:
                tft.fillRect(0, 34, 128, 94, BLACK);
                tft.setTextColor(YELLOW);
                tft.setTextSize(2);
                tft.fillCircle(12, 64, 12, GREY);
                tft.drawCircle(12, 64, 12, YELLOW);
                tft.setCursor(8, 57);
                tft.print("1");
                tft.setCursor(32, 58);
                tft.print("Go Back!");
                tft.setCursor(32, 77);
                tft.print("   <-  ");
                break;

            case 1:
                tft.fillRect(0, 34, 128, 94, BLACK);
                tft.setTextColor(YELLOW);
                tft.setTextSize(2);
                tft.fillCircle(12, 64, 12, GREY);
                tft.drawCircle(12, 64, 12, YELLOW);
                tft.setCursor(8, 57);
                tft.print("2");
                tft.setCursor(32, 57);
                tft.print("Low");
                break;

            case 2:
                tft.fillRect(0, 34, 128, 94, BLACK);
                tft.setTextColor(YELLOW);
                tft.setTextSize(2);
                tft.fillCircle(12, 64, 12, GREY);
                tft.drawCircle(12, 64, 12, YELLOW);
                tft.setCursor(8, 57);
                tft.print("3");
                tft.setCursor(32, 57);
                tft.print("Moderate");
                break;

            case 3:
                tft.fillRect(0, 34, 128, 94, BLACK);
                tft.setTextColor(YELLOW);
                tft.setTextSize(2);
                tft.fillCircle(12, 64, 12, GREY);
                tft.drawCircle(12, 64, 12, YELLOW);
                tft.setCursor(8, 57);
                tft.print("4");
                tft.setCursor(32, 57);
                tft.print("High");
                break;
        }
    }
}


//MENU-CASE 5 (DISPENSE MENU L2 - CONCENTRATION) SUB-FUNCTION TO RENDER MENU TEXT AFTER A SELECTION HAS BEEN MADE
void case5select (int input)
{
    switch (input)
    {
       case 0:
            tft.fillRect(0, 34, 128, 94, BLACK);
            tft.setTextColor(RED);
            tft.setTextSize(2);
            tft.fillCircle(12, 64, 12, WHITE);
            tft.drawCircle(12, 64, 12, RED);
            tft.setCursor(8, 57);
            tft.print("1");
            tft.setCursor(32, 58);
            tft.print("Go Back!");
            tft.setCursor(32, 77);
            tft.print("   <-  ");
            break;

        case 1:
            tft.fillRect(0, 34, 128, 94, BLACK);
            tft.setTextColor(RED);
            tft.setTextSize(2);
            tft.fillCircle(12, 64, 12, WHITE);
            tft.drawCircle(12, 64, 12, RED);
            tft.setCursor(8, 57);
            tft.print("2");
            tft.setCursor(32, 57);
            tft.print("Low");
            break;

        case 2:
            tft.fillRect(0, 34, 128, 94, BLACK);
            tft.setTextColor(RED);
            tft.setTextSize(2);
            tft.fillCircle(12, 64, 12, WHITE);
            tft.drawCircle(12, 64, 12, RED);
            tft.setCursor(8, 57);
            tft.print("3");
            tft.setCursor(32, 57);
            tft.print("Moderate");
            break;

        case 3:
            tft.fillRect(0, 34, 128, 94, BLACK);
            tft.setTextColor(RED);
            tft.setTextSize(2);
            tft.fillCircle(12, 64, 12, WHITE);
            tft.drawCircle(12, 64, 12, RED);
            tft.setCursor(8, 57);
            tft.print("4");
            tft.setCursor(32, 57);
            tft.print("High");
            break;
    }

}


//MENU-CASE 6 (DISPENSE MENU L3 - CONFIRM) SUB-FUNCTION TO RENDER MENU TEXT
void case6menu (int input, int caselast)
{
    if (input != caselast)                                //skip rendering unless it's needed
    {
        switch (input)
        {
            case 0:
                tft.fillRect(0, 34, 128, 94, BLACK);
                tft.setTextColor(YELLOW);
                tft.setTextSize(2);
                tft.fillCircle(12, 64, 12, GREY);
                tft.drawCircle(12, 64, 12, YELLOW);
                tft.setCursor(8, 57);
                tft.print("1");
                tft.setCursor(32, 58);
                tft.print("Go Back!");
                tft.setCursor(32, 77);
                tft.print("   <-  ");
                break;

            case 1:
                tft.fillRect(0, 34, 128, 94, BLACK);
                tft.setTextColor(YELLOW);
                tft.setTextSize(2);
                tft.fillCircle(12, 64, 12, GREY);
                tft.drawCircle(12, 64, 12, YELLOW);
                tft.setCursor(8, 57);
                tft.print("2");
                tft.setCursor(32, 57);
                tft.print("Dispense");
                break;

            case 2:
                tft.fillRect(0, 34, 128, 94, BLACK);
                tft.setTextColor(YELLOW);
                tft.setTextSize(2);
                tft.fillCircle(12, 64, 12, GREY);
                tft.drawCircle(12, 64, 12, YELLOW);
                tft.setCursor(8, 57);
                tft.print("3");
                tft.setCursor(32, 58);
                tft.print("Cancel");
                tft.setCursor(32, 77);
                tft.print("-> Main");
                break;
        }
    }
}


//MENU-CASE 6 (DISPENSE MENU L3 - CONFIRM) SUB-FUNCTION TO RENDER MENU TEXT AFTER A SELECTION HAS BEEN MADE
void case6select (int input)
{
    switch (input)
    {
       case 0:
            tft.fillRect(0, 34, 128, 94, BLACK);
            tft.setTextColor(RED);
            tft.setTextSize(2);
            tft.fillCircle(12, 64, 12, WHITE);
            tft.drawCircle(12, 64, 12, RED);
            tft.setCursor(8, 57);
            tft.print("1");
            tft.setCursor(32, 58);
            tft.print("Go Back!");
            tft.setCursor(32, 77);
            tft.print("   <-  ");
            break;

        case 1:
            tft.fillRect(0, 34, 128, 94, BLACK);
            tft.setTextColor(RED);
            tft.setTextSize(2);
            tft.fillCircle(12, 64, 12, WHITE);
            tft.drawCircle(12, 64, 12, RED);
            tft.setCursor(8, 57);
            tft.print("2");
            tft.setCursor(32, 57);
            tft.print("Dispense");
            break;

        case 2:
            tft.fillRect(0, 34, 128, 94, BLACK);
            tft.setTextColor(RED);
            tft.setTextSize(2);
            tft.fillCircle(12, 64, 12, WHITE);
            tft.drawCircle(12, 64, 12, RED);
            tft.setCursor(8, 57);
            tft.print("3");
            tft.setCursor(32, 58);
            tft.print("Cancel");
            tft.setCursor(32, 77);
            tft.print("-> Main");
            break;
    }
}


// SUB-FUNCTION TO MINMIZE SPIKES IN CURRENT DRAW BY LINEARLY INCREASING MOTOR DUTY CYCLE OVER DURATION OF "RAMPUP" MILLISECONDS
void softstart(int PinNum, float rampup){

dwell = max(floor(rampup * 1000.0 / 254.0), 200.0);                               // Compute time (in us) to dwell at each duty cycle step [5000Hz frequency = 200us cycle time, 0-255 duty cycle range]

for(ii = 1; ii < 255; ii++){                                                // Linearly increase duty cycle over duration specified by "rampup" - minimum 1ms dwell time to allow at least 1 cycle per step
  analogWrite(PinNum, ii, 5000);
  delayMicroseconds(dwell);
}

digitalWrite(PinNum, HIGH);                                                   // Leave pin "on" (high)
}


// SUB-FUNCTION TO MINMIZE SPIKES IN VOLTAGE BY LINEARLY DECREASING MOTOR DUTY CYCLE OVER DURATION OF "RAMPDOWN" MILLISECONDS
void softstop(int PinNum, float rampdown){

dwell = max(floor(rampdown * 1000.0 / 254.0), 200.0);                             // Compute time (in us) to dwell at each duty cycle step [5000Hz frequency = 200ms cycle time, 0-255 duty cycle range]

for(ii = 254; ii > 0; ii--){                                                // Linearly decrease duty cycle over duration specified by "rampdown" - minimum 1ms dwell time to allow at least 1 cycle per step
  analogWrite(PinNum, ii, 5000);
  delayMicroseconds(dwell);
}

digitalWrite(PinNum, LOW);                                                    // Leave pin "off" (low)
}
