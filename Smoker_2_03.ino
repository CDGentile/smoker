//#include <TimerOne.h>

//#include <EEPROM.h>

#include <max6675.h>

#include <SerLCD.h>

#include <SoftwareSerial.h>

#include "Arduino.h"

//Smoker 2.02
//C Gentile
//Updated 20 Jul 2013
//using prototype HW June 2013
//target board: Leonardo

//define pins
#define AMB      A0

#define BUZZER   A5

#define RX        0
#define TX        1
#define ENC_A     2
#define ENC_B     4
#define ENC_PRESS 3

#define FAN_PIN   6

#define LCD_PIN   8

#define CS_1      10
#define CS_2      11
#define DO        12
#define CLK       13

#define DOT		  0xA5
#define ARR		  0x7E
#define DEG		  0xDF

const int FOOD[2] = {19,20};  //allow for future food temp expansion

//define screen location addresses
//main screen:
#define pTIME     0x01
#define pTIMEC    0x00
#define pSET      0x45
#define pSETC     0x40
#define pSETCC	  0x44
#define pPIT1     0x4D
#define pPIT2     0x51
#define pFOODA    0x21
#define pFOODB    0x25
#define pAMB      0x0F
#define pFAN      0x64
#define pFANC	  0x5F
#define pMENU     0x14
#define pMODE     0x59
#define pMENUL	  0x11

//menu screen:
//fault screen:
//cal screen:

//Library classes
SerLCD LCD(LCD_PIN);
MAX6675 Pit[2] = {MAX6675(CLK, CS_1, DO), MAX6675(CLK, CS_2, DO)};  //allow for future pit expansion

//Global Variables
  //initialize every time;
volatile int ambient = 69;
volatile int time[] = {0,0,0};          //HH, MM, SS
volatile int food_temp[] = {175,78};
volatile int pit_temp[] = {200,69,0,0};		//1, 2, avg, old
volatile int set = 225;
volatile int error[] = {0,0,0};			//pro, int, der
volatile int temp_ct = 5;                //initialize for 5 cycle/sec
int door_ct = 0;                //initialize as closed (timer expired)
  //read from EEPROM
float k[] = {1.0,1.0,1.0};
int door_time = 60;
int door_rate = 10;				//
int PID_time = 10;				// (in seconds)
int pit_avg_ct = 3;				//number of readings to average
int food_avg_ct = 3;			
volatile int focus = 1;
String menu[] = {" Change Mode", " Faults", " Set Alarms", " Set Parameters", " Screen Settings", " Save Settings", " WiFi Options", " Exit"};
volatile int page = 1;					// 1:home, 2: menu, 3: faults
char menu_lines[] = {LINE1, LINE2, LINE3, LINE4};
volatile int fan = 69;
int bright = 5;
int cnst = 40;
int fAlarm = 195;
int pAlarm = 260;
boolean alarm_enable = false;
boolean alarm = false;
boolean buzz = true;
int fan_time = 100;


boolean faultActive = false;
String fault_list[] = {"PIT 1 FAULT", "PIT 2 FAULT", "PIT FAIL",
                   "OVERTEMP", "FOOD A FAULT", "FOOD B FAULT",
                   "AMBIENT FAULT", "WIFI FAIL", "TEMP LOW"};
boolean faults[9];                 
char mode = 0;

boolean warming = false;

//procedures
void Read_Params() {
}

void Write_Params() {
}

void Home_Screen(){   //draws the home screen legends (no Fault)
//modify to include variables
	LCD.clear();
	LCD.dirPos(LINE1);
	LCD.write(ARR);
	LCD.print("00:00:00  Amb:xxx");
	LCD.write(DEG);
	LCD.dirPos(LINE2);
	LCD.write(DOT);
	LCD.print("SET:xxx Pit xxx/xxx");
	LCD.dirPos(LINE3);
	LCD.write(DOT);
	LCD.print("MENU   Food xxx/xxx");
	LCD.dirPos(LINE4);
	LCD.print("Mode:INIT   Fan:xxx%");
	Write_Time();
	Write_Set_Temp();
	Write_Mode();
	Write_Amb_Temp();
	Write_Pit_Temps();
	Write_Food_Temps();
	Write_Fan();
}
void Write_Fan() {
	LCD.dirPos(pFAN);
	if (fan < 100) {
		LCD.print(" ");
	}
	if (fan < 10) {
		LCD.print(" ");
	}
	LCD.print(fan);
}


void Write_Food_Temps() {
	LCD.dirPos(pFOODA);
	if (food_temp[0] < 100) {
		LCD.print("0");
	}
	if (food_temp[0] < 10) {
		LCD.print("0");
	}
	LCD.print(food_temp[0]);
	LCD.dirPos(pFOODB);
	if (food_temp[1] < 100) {
		LCD.print("0");
	}
	if (food_temp[1] < 10) {
		LCD.print("0");
	}
	LCD.print(food_temp[1]);

}  

void Write_Pit_Temps() {
	LCD.dirPos(pPIT1);
	if (pit_temp[0] < 100) {
		LCD.print("0");
	}
	if (pit_temp[0] < 10) {
		LCD.print("0");
	}
	LCD.print(pit_temp[0]);
	LCD.dirPos(pPIT2);
	if (pit_temp[1] < 100) {
		LCD.print("0");
	}
	if (pit_temp[1] < 10) {
		LCD.print("0");
	}
	LCD.print(pit_temp[1]);
}

void Write_Set_Temp() {
	LCD.dirPos(pSET);
	if (set < 100) {
		LCD.print(" ");
	}
	LCD.print(set);
}

void Write_Amb_Temp() {
	LCD.dirPos(pAMB);
	if (ambient < 100) {
		LCD.print(" ");
	}
	LCD.print(ambient);
}

void Write_Mode() {
  LCD.dirPos(pMODE);
  LCD.print("     ");
  LCD.dirPos(pMODE);
  switch (mode) {
		case 0:
			LCD.print("PSIV");
		break;
		case 1:
			LCD.print("WARM");
		break;
		case 2:
			LCD.print("AUTO");
		break;
		case 3:
			LCD.print("MAN");
		break;
  }
}

/*void Write_Fault() {
  if (faultActive) {
    LCD.dirPos(pMODE);
    LCD.print("FAULT");
  }
  else {
    LCD.dirPos(pFAULT);
    LCD.print("     ");
  }
}*/
  
int Read_Ambient() {
  return ((int) (((((analogRead(AMB)*5.0)/1024.0)-0.5)*100.0) * 1.8 +32.0));
}

int Read_Pit(int pit_pin) {				//change to function, take 3 readings, return 0 if fault on any
  pit_temp[pit_pin] = (int) Pit[pit_pin].readFarenheit(); 
}

int Read_Food(int food_pin) {
  	double R, T;
        int aval;

        aval = analogRead(FOOD[food_pin]);
	// These were calculated from the thermister data sheet
	//	A = 2.3067434E-4;
	//	B = 2.3696596E-4;
	//	C = 1.2636414E-7;
	//
	// This is the value of the other half of the voltage divider
	//	Rknown = 22200;

	// Do the log once so as not to do it 4 times in the equation
	//	R = log(((1024/(double)aval)-1)*(double)22200);

	R = log((1 / ((1024 / (double) aval) - 1)) * (double) 22200);
	//lcd.print("A="); lcd.print(aval); lcd.print(" R="); lcd.print(R);
	// Compute degrees C
	T = (1 / ((2.3067434E-4) + (2.3696596E-4) * R + (1.2636414E-7) * R * R * R)) - 273.25;
	// return degrees F
	return ((int) ((T * 9.0) / 5.0 + 32.0));
}

void Calc_Temps() {                 //only average if not zero
    if ((pit_temp[0] > 0) && (pit
	pit_temp[2] = (int) ((pit_temp[0] + pit_temp[1])/2);
}

void Draw_Focus() {
	switch (page){
		case 1:
		  switch (focus) {
			case 1:
			  LCD.dirPos(pTIMEC);
			  break;
			case 2:
			  LCD.dirPos(pSETC);
			  break;
			case 3:
			  LCD.dirPos(pMENU);
			  break;
			case 4:
			  LCD.dirPos(pSETCC);
			  break;
		  }
		  LCD.write(ARR);
		  delay(3);
		break;
		case 2:									//menu page
			LCD.dirPos(LINE4);
			if (focus <=3) {
				LCD.dirPos(menu_lines[focus]);
			}
			LCD.write(ARR);
		break;
		case 5:									//alarm page
			switch (focus){
				case 0:
					LCD.dirPos(LINE1);
					LCD.write(ARR);
				break;
				case 1:
					LCD.dirPos(LINE2);
					LCD.write(ARR);
				break;
				case 2:
					LCD.dirPos(LINE3);
					LCD.write(ARR);
				break;
				case 3:
					LCD.dirPos(LINE4);
					LCD.write(ARR);
				break;
				case 4:
					LCD.dirPos(0x0D);
					LCD.write(ARR);
				break;
				case 5:
					LCD.dirPos(0x4D);
					LCD.write(ARR);
				break;
			}
		break;
		case 6:				//param page
			switch (focus){
				case 0:
					LCD.dirPos(0x41);
					LCD.write(ARR);
				break;
				case 1:
					LCD.dirPos(0x45);
					LCD.write(ARR);
				break;
				case 2:
					LCD.dirPos(0x49);
					LCD.write(ARR);
				break;
				case 3:
					LCD.dirPos(0x4D);
					LCD.write(ARR);
				break;
				case 4:
					LCD.dirPos(0x54);
					LCD.write(ARR);
				break;
				case 5:
					LCD.dirPos(0x15);
					LCD.write(ARR);
				break;
				case 6:
					LCD.dirPos(0x19);
					LCD.write(ARR);
				break;
				case 7:
					LCD.dirPos(0x1D);
					LCD.write(ARR);
				break;
				case 8:
					LCD.dirPos(0x21);
					LCD.write(ARR);
				break;
			}
		break;
		case 7:				//screen page
			switch (focus){
				case 0:
					LCD.dirPos(LINE2);
					LCD.write(ARR);
				break;
				case 1:
					LCD.dirPos(LINE3);
					LCD.write(ARR);
				break;
				case 2:
					LCD.dirPos(LINE4);
					LCD.write(ARR);
				break;
				case 3:
					LCD.dirPos(0x4C);
					LCD.write(ARR);
				break;
				case 4:
					LCD.dirPos(0x20);
					LCD.write(ARR);
				break;
			}
		break;
	}
}



void Clear_Focus() {
	switch (page){
		case 1:
			switch (focus) {
				case 1:
					LCD.dirPos(pTIMEC);
					LCD.write(DOT);
				break;
				case 2:
					LCD.dirPos(pSETC);
					LCD.write(DOT);
				break;
				case 3:
					LCD.dirPos(pMENU);
					LCD.write(DOT);
				break;
				case 4:
					LCD.dirPos(pSETCC);
					LCD.print(":");
				break;
			}
		break;
		case 2:
			LCD.dirPos(LINE4);
			if (focus <=3) {
				LCD.dirPos(menu_lines[focus]);
			}
			LCD.print(" ");
		break;
		case 5:									//alarm page
			switch (focus){
				case 0:
					LCD.dirPos(LINE1);
					LCD.write(" ");
				break;
				case 1:
					LCD.dirPos(LINE2);
					LCD.write(" ");
				break;
				case 2:
					LCD.dirPos(LINE3);
					LCD.write(" ");
				break;
				case 3:
					LCD.dirPos(LINE4);
					LCD.write(" ");
				break;
				case 4:
					LCD.dirPos(0x0D);
					LCD.write(" ");
				break;
				case 5:
					LCD.dirPos(0x4D);
					LCD.write(" ");
				break;
			}
		case 6:				//param page
			switch (focus){
				case 0:
					LCD.dirPos(0x41);
					LCD.write(" ");
				break;
				case 1:
					LCD.dirPos(0x45);
					LCD.write(" ");
				break;
				case 2:
					LCD.dirPos(0x49);
					LCD.write(" ");
				break;
				case 3:
					LCD.dirPos(0x4D);
					LCD.write(" ");
				break;
				case 4:
					LCD.dirPos(0x54);
					LCD.write(" ");
				break;
				case 5:
					LCD.dirPos(0x15);
					LCD.write(" ");
				break;
				case 6:
					LCD.dirPos(0x19);
					LCD.write(" ");
				break;
				case 7:
					LCD.dirPos(0x1D);
					LCD.write(" ");
				break;
				case 8:
					LCD.dirPos(0x21);
					LCD.write(" ");
				break;
			}
		break;
		case 7:				//screen page
			switch (focus){
				case 0:
					LCD.dirPos(LINE2);
					LCD.print(" ");
				break;
				case 1:
					LCD.dirPos(LINE3);
					LCD.print(" ");
				break;
				case 2:
					LCD.dirPos(LINE4);
					LCD.print(" ");
				break;
				case 3:
					LCD.dirPos(0x4C);
					LCD.print(" ");
				break;
				case 4:
					LCD.dirPos(0x20);
					LCD.print(" ");
				break;
			}
		break;
	}
}

void Draw_Menu() {
	int start = 0;
	if (focus > 3) {
		start = (focus-3);
	}
	LCD.clear();
	LCD.dirPos(LINE1);
	LCD.print(menu[start]);
	LCD.dirPos(LINE2);
	LCD.print(menu[start+1]);
	LCD.dirPos(LINE3);
	LCD.print(menu[start+2]);
	LCD.dirPos(LINE4);
	LCD.print(menu[start+3]);
	LCD.dirPos(pMENUL);
	LCD.print(focus+1);
	LCD.print("/8");
}

void Draw_Screen_Page() {
	LCD.clear();
	LCD.dirPos(LINE1);
	LCD.print("  Screen Settings");
	LCD.dirPos(LINE2);
	LCD.print(" Brightness: ");
	LCD.print(bright);
	LCD.dirPos(LINE3);
	LCD.print(" Contrast:   ");
	LCD.print(cnst);
	LCD.dirPos(LINE4);
	LCD.print(" Exit");
}

void Draw_Alarm_Page(){
    LCD.clear();
    LCD.dirPos(LINE1);
    LCD.print("  Pit Alarm:  ");
    LCD.print(pAlarm);
    LCD.dirPos(LINE2);
    LCD.print(" Food Alarm:  ");
    LCD.print(fAlarm);
    LCD.dirPos(LINE3);
    LCD.print(" Alarm Buzzer:");
    if (alarm_enable) {
        LCD.print("On ");
    }
    else {
        LCD.print("Off");
    }
    LCD.dirPos(LINE4);
    LCD.print(" Exit");
}

void Draw_Param_Page() {
	LCD.clear();
	LCD.dirPos(LINE1);
	LCD.print("   Set Parameters:");
	LCD.dirPos(LINE2);
	LCD.print("  Kp  Ki  Kd  Time");
	LCD.dirPos(LINE3);
	LCD.print("  ");
	LCD.print(k[0],1);
	LCD.print(" ");
	LCD.print(k[1],1);
	LCD.print(" ");
	LCD.print(k[2],1);
	LCD.print(" ");
	LCD.print(PID_time);
	LCD.dirPos(LINE4);
	LCD.print(" Exit");
}

//Encoder ISR
//Reads encoder and updates cursor or setting
void Enc_ISR() {
  int delta = 0;
  
  if (digitalRead(ENC_B) == 0) {        //we know A went low to high, check B for direction
    delta = 1;
  }
  else {
    delta = -1;
  } 
  
    switch (page) {        //build functionality for multiple pages
		case 1:				//home page
			switch (focus){          //on home page, use encoder to cycle focus[1] and set temp
				case 1:
					Clear_Focus();
					focus += delta;
					if (focus==0) {      //check bounds
					focus = 3;
					}
					Draw_Focus();
				break;
				case 2:
					Clear_Focus();
					focus += delta;
					Draw_Focus();
				break;
				case 3:
					Clear_Focus();
					focus += delta;
					if (focus == 4) {
					  focus = 1;
					}
					Draw_Focus();
				break;
				case 4:
					set += delta;
					if (set < ambient) {        //cannot set pit temp below ambient temp
					  set = ambient;
					}
					Write_Set_Temp();
				break; 
			}
		break;
		case 2:			//menu page:
			Clear_Focus();
			focus+=delta;
			if (focus<0) {
				focus=0;
			}
			if (focus>7) {
				focus=7;
			}
			Draw_Menu();
			Draw_Focus();
		break;
		case 3:			//fault page
		break;
		case 5:			//alarms page
			switch (focus){
				case 0:
					Clear_Focus();
					focus += delta;
					if (focus<0) {      //check bounds
					focus = 3;
					}
					Draw_Focus();
				break;
				case 1:
					Clear_Focus();
					focus += delta;
					Draw_Focus();
				break;
				case 2:
					Clear_Focus();
					focus += delta;
					Draw_Focus();
				break;
				case 3:
					Clear_Focus();
					focus += delta;
					if (focus>3) {      //check bounds
					focus = 0;
					}
					Draw_Focus();
				break;
				case 4:
					pAlarm += delta;
					if (pAlarm==0){
						pAlarm = 1;
					}
					Draw_Alarm_Page();
					Draw_Focus();
				break;
				case 5:
					fAlarm += delta;
					if (fAlarm==0){
						fAlarm = 1;
					}
					Draw_Alarm_Page();
					Draw_Focus();
				break;
			}
		break;
		case 6:							//param page
			switch (focus){
				case 0:
					Clear_Focus();
					focus += delta;
					if (focus<0) {      //check bounds
					focus = 4;
					}
					Draw_Focus();
				break;
				case 1:
					Clear_Focus();
					focus += delta;
					Draw_Focus();
				break;
				case 2:
					Clear_Focus();
					focus += delta;
					Draw_Focus();
				break;
				case 3:
					Clear_Focus();
					focus += delta;
					Draw_Focus();
				break;
				case 4:
					Clear_Focus();
					focus += delta;
					if (focus>4) {      //check bounds
					focus = 0;
					}
					Draw_Focus();
				break;
				case 5:
					k[0] += (delta*0.1);
					if (k[0]<0.0){
						k[0] = 0.0;
					}
					Draw_Param_Page();
					Draw_Focus();
				break;
				case 6:
					k[1] += (delta*0.1);
					if (k[1]<0.0){
						k[1] = 0.0;
					}
					Draw_Param_Page();
					Draw_Focus();
				break;
				case 7:
					k[2] += (delta*0.1);
					if (k[2]<0.0){
						k[2] = 0.0;
					}
					Draw_Param_Page();
					Draw_Focus();
				break;
				case 8:
					PID_time += delta;
					if (PID_time==0){
						PID_time = 1;
					}
					Draw_Param_Page();
					Draw_Focus();
				break;
			}
		break;
		case 7:			//screen page
			switch (focus){
				case 0:
					Clear_Focus();
					focus += delta;
					if (focus<0) {      //check bounds
					focus = 2;
					}
					Draw_Focus();
				break;
				case 1:
					Clear_Focus();
					focus += delta;
					Draw_Focus();
				break;
				case 2:
					Clear_Focus();
					focus += delta;
					if (focus>2) {      //check bounds
					focus = 0;
					}
					Draw_Focus();
				break;
				case 3:
					bright += delta;
					if (bright==0){
						bright = 1;
					}
					if (bright==9){
						bright=8;
					}
					Draw_Screen_Page();
					Draw_Focus();
					LCD.setBright(bright);
				break;
				case 4:
					cnst += delta;
					if (cnst==0){
						cnst = 1;
					}
					if (cnst == 51){
						cnst = 50;
					}
					Draw_Screen_Page();
					Draw_Focus();
					LCD.setContrast(cnst);
				break;
			}
		break;
	} 
}

void Enc_Press() {                  //ISR handler for encoder button press
	switch (page){
		case 1:
			switch (focus){                      //handle main page button presses
				case 1:
					time[0]=0;
					time[1]=0;
					time[2]=0;
					Write_Time();
				break;
				case 2:
					Clear_Focus();
					focus = 4;
					Draw_Focus();
				break;
				case 3:
					page = 2;
					focus = 0;
					Draw_Menu();
					Draw_Focus();
				break;
				case 4:
					Clear_Focus();
					focus = 2;
					Draw_Focus();
				break;
			}
		break;
		case 2:							//MENU page
			switch (focus){
				case 0:					//Change mode
				break;
				case 1:					//Faults
				break;
				case 2:					//select alarm page
					page = 5;
					focus = 0;
					Draw_Alarm_Page();
					Draw_Focus();
				break;
				case 3:					//Set Parameters
					page = 6;
					focus = 0;
					Draw_Param_Page();
					Draw_Focus();
				break;
				case 4:					//screen settings
					page = 7;
					focus = 0;
					Draw_Screen_Page();
					Draw_Focus();
				break;
				case 5:
				break;
				case 6:

				break;
				case 7:					//exit
					page = 1;
					focus = 1;
					Home_Screen();
					//Write_Time();
				break;
			}
		break;
		case 3:
		break;
		case 5:							//alarm page
			switch (focus){
				case 0:
					Clear_Focus();
					focus = 4;
					Draw_Focus();
				break;
				case 1:
					Clear_Focus();
					focus = 5;
					Draw_Focus();
				break;
				case 2:
					alarm_enable = !(alarm_enable);
					Draw_Alarm_Page();
					Draw_Focus();
				break;
				case 3:
					page = 2;
					focus = 2;
					Draw_Menu();
					Draw_Focus();
				break;
				case 4:
					Clear_Focus();
					focus = 0;
					Draw_Focus();
				break;
				case 5:
					Clear_Focus();
					focus = 1;
					Draw_Focus();
				break;
			}
		break;
		case 6:							//param page
			switch (focus){
				case 0:
					Clear_Focus();
					focus = 5;
					Draw_Focus();
				break;
				case 1:
					Clear_Focus();
					focus = 6;
					Draw_Focus();
				break;
				case 2:
					Clear_Focus();
					focus = 7;
					Draw_Focus();
				break;
				case 3:
					Clear_Focus();
					focus = 8;
					Draw_Focus();
				break;
				case 4:
					page = 2;
					focus = 0;
					Draw_Menu();
					Draw_Focus();
				break;
				case 5:
					Clear_Focus();
					focus = 0;
					Draw_Focus();
				break;
				case 6:
					Clear_Focus();
					focus = 1;
					Draw_Focus();
				break;
				case 7:
					Clear_Focus();
					focus = 2;
					Draw_Focus();
				break;
				case 8:
					Clear_Focus();
					focus = 3;
					Draw_Focus();
				break;
			}
		break;
		case 7:							//screen page
			switch (focus){
				case 0:
					Clear_Focus();
					focus = 3;
					Draw_Focus();
				break;
				case 1:
					Clear_Focus();
					focus = 4;
					Draw_Focus();
				break;
				case 2:
					page = 2;
					focus = 4;
					Draw_Menu();
					Draw_Focus();
				break;
				case 3:
					Clear_Focus();
					focus = 0;
					Draw_Focus();
				break;
				case 4:
					Clear_Focus();
					focus = 1;
					Draw_Focus();
				break;
			}
		break;
	}
}


void Write_Time() {
	LCD.dirPos(pTIME);
	if (time[0]<10) {
		LCD.print("0");
	}
	LCD.print(time[0]);
	LCD.print(":");
	if (time[1]<10) {
		LCD.print("0");
	}
	LCD.print(time[1]);
	LCD.print(":");
	if (time[2]<10) {
		LCD.print("0");
	}
	LCD.print(time[2]);
}

void Fan_Set() {
    fan_time--;
    if (fan_time==) {
        fan_time=100;
    }
    if (fan > 15) {
        analogWrite(FAN_PIN, int (fan * 2.54));
    }
    else {                                //below 15% run at 90% for pulses of time (x/100 sec)
        if (fan_time < fan) {
            analogWrite(FAN_PIN, 230);
        }
        else {
            analogWrite(FAN_PIN, 0);
        }
    }
}

void Do_PID() {
    
}

//Main temperature and PID ISR function
void Timer_Handler(){
  int temp_reading = 0;            //temp variable for readings
  
  temp_ct--;                       //decrement timer every 200ms
  switch (temp_ct) {
    case 4:                        //read ambient temp
      temp_reading = Read_Ambient();
      if (temp_reading != 0) {
        ambient = temp_reading;
        //clear flag if set
      }
      else {
        //set invalid flag, leave reading unchanged
      }
	  temp_reading = Read_Food(0);
      if (temp_reading > 20) {
        food_temp[0] = temp_reading;
        //clear flag if set
      }
      else {
        //set invalid flag, leave reading unchanged
		food_temp[0] = 0;
      }
      break;
      
      case 3:                        //read pit_1 temp
      temp_reading = Read_Pit(1);
      if (temp_reading > 20) {
          pit_temp[1] = temp_reading;
          //clear flag if set
      }
      else {
          //set invalid flag, leave reading unchanged
      }

      break;
      
      case 2:                        //read food_1 temp
      temp_reading = Read_Food(1);
      if (temp_reading > 20) {
        food_temp[1] = temp_reading;
        //clear flag if set
      }
      else {
        //set invalid flag, leave reading unchanged
		food_temp[1] = 0;
      }
      break;
      
      case 1:
	  temp_reading = Read_Pit(0);
      if (temp_reading > 20) {
          pit_temp[0] = temp_reading;
          //clear flag if set
      }
      else {
          //set invalid flag, leave reading unchanged
      }
      
      break;
      
      case 0:
        temp_ct = 5;                    //reset time interrupt counter
        
        time[2]++;                      //add 1 second and convert to hh:mm:ss
        if (time[2] == 60) {
            time[1]++;
            time[2] -= 60;
        }
        if (time[1] == 60) {
            time[0]++;
            time[1] -= 60;
        }
        
        PID_count--;                    //increment PID counter and 
        if (PID_Count == 0) {
            PID_Count = PID_Time;
            
                                        //calc errors
                                        
        }
        
        if (page == 1){                 //update temps and times
	        Write_Time();
	        Write_Pit_Temps();
	        Write_Food_Temps();
	        Write_Amb_Temp();
	    }   
	    
	                                    //check alarms
		if ((food_temp[0]>fAlarm) || (food_temp[1]>fAlarm) || (pit_temp[0]>pAlarm) || (pit_temp[1]>pAlarm)) {
			alarm = true;		
		}
		else {
			alarm = false;
		}
		if (alarm && alarm_enable) {
			buzz = !buzz;
		}
		else {
			buzz = true;
		}
		digitalWrite(BUZZER, buzz);
		
      break;
  }
  
}

void setup(){
	pinMode(2,INPUT);
	pinMode(3,INPUT);
	pinMode(4,INPUT);
	digitalWrite(2, HIGH);
	digitalWrite(3, HIGH);
	digitalWrite(4, HIGH);
	pinMode(BUZZER, OUTPUT);
	digitalWrite(BUZZER, LOW);
	delay(1000);     //Pause for initialization
	attachInterrupt(1,Enc_ISR,RISING);           //functions on ENCA low-high transition (should be 1 per click)
	attachInterrupt(0,Enc_Press,FALLING);        //functions when button goes LOW (assumes pull-high resistor)
	LCD.clear();
	delay(500);
	Home_Screen();                              //Setup default screen legends
	//Write_Set_Temp();
	Clear_Focus();
	page=1;
	focus = 1;
        Draw_Focus();
	delay(1000);
	//Write_Time();
}

void loop(){
//  ambient = Read_Ambient();
//  Write_Amb_Temp();
//  Read_Pit(1);
 // Read_Pit(2);
//  Calc_Temps();
//  Write_Pit_Temps();
 // Draw_Focus();
    delay(200);
    Timer_Handler();
 // focus++;
 // if (focus==6) {
 //   focus=1;
  //Serial.print("Loop");
}


