// MAIN ECU
#include <CAN.h>

//______________BLYNK and WIFI
/* Comment this out to disable prints and save space */
//#define BLYNK_PRINT Serial
#include <SPI.h>
#include <WiFiNINA.h>
#include <BlynkSimpleWiFiNINA.h>
char auth[] = "RDyNQX1ZiETyEM_prEITRBL7TRYpYJQn";
char ssid[] = "Lukephone";
char pass[] = "123456789";

//______________LCR (Lane Change Revommendittion) VARIABLES
int LCR_minimum_speed = 80; // below this speed the feature is disabled
int LCR_speed_diff = 15; // at which speed differende to the leas it recommends a lane change
int LCR_lead_distance = 100; // minimum distance to the lead

//______________COUNTING LOOPS AND BOOBS
float       loop_count            = 0;
float       last_loop_calc        = 0;
float       ausgabe_loop_sec      = 0;
float       interrupt_counter     = 0;
float       ausgabe_int_sec       = 0;

//______________BUTTONS / SWITCHES / VALUES
int BlinkerPinLeft = 4;
int BlinkerPinRight = 5;
int button4 = 8;
int button3 = 7;
int button2 = 6;
int button1 = 9;
int CluchSwitch = A4;
boolean ClutchSwitchState = false;
int buttonstate4;
int lastbuttonstate4;
int buttonstate3;
int lastbuttonstate3;
int buttonstate2;
int lastbuttonstate2;
int buttonstate1;
int lastbuttonstate1;
boolean lastGAS_RELEASED = false;
boolean lastBRAKE_PRESSED = false;
long last_blinker_right;
long last_blinker_left;

//______________VALUES SEND ON CAN
boolean OP_ON = false;
boolean MAIN_ON = true;
uint8_t set_speed = 0x0;
double average = 0; 
boolean blinker_left_on = true;
boolean blinker_right_on = true;
float LEAD_LONG_DIST = 0;
float LEAD_REL_SPEED = 0;
float LEAD_LONG_DIST_RAW = 0;
float LEAD_REL_SPEED_RAW = 0;
boolean BRAKE_PRESSED = true;
boolean GAS_RELEASED = false;

//______________FOR SMOOTHING SPD
const int numReadings = 160;
float readings[numReadings];
int readIndex = 0; 
double total = 0;

//______________FOR READING VSS SENSOR
const byte interruptPin = 3;
int inc = 0;
int half_revolutions = 0;
int spd;
unsigned long lastmillis;
unsigned long duration;
uint8_t encoder = 0;

/*
//______________BLYNK
BlynkTimer timer;
// This function sends Arduino's up time every second to Virtual Pin (5).
// In the app, Widget's reading frequency should be set to PUSH. This means
// that you define how often to send data to Blynk App.
void myTimerEvent()
{
  // You can send any value at any time.
  // Please don't send more that 10 values per second.
  Blynk.virtualWrite(V0, average / 500);
  Blynk.virtualWrite(V1, LEAD_LONG_DIST / 500);
}
BLYNK_WRITE(V3)
{
  int pinValueV3 = param.asInt(); // assigning incoming value from pin V3 to a variable
  set_speed = pinValueV3;
}
BLYNK_WRITE(V2)
{
  boolean pinValueV2 = param.asInt(); // assigning incoming value from pin V2 to a variable
  OP_ON = pinValueV2;
}
*/
void setup() {
  
Serial.begin(9600);

CAN.begin(500E3);

//Blynk.begin(auth, ssid, pass);
 
/* 
// You can also specify server:
Blynk.begin(auth, ssid, pass, "blynk-cloud.com", 80);
Blynk.begin(auth, ssid, pass, IPAddress(192,168,1,100), 8080);
timer.setInterval(500L, myTimerEvent);
*/
  
//______________initialize pins 
pinMode(interruptPin, INPUT_PULLUP);
attachInterrupt(digitalPinToInterrupt(interruptPin), rpm, FALLING);
pinMode(button1, INPUT);
pinMode(button2, INPUT);
pinMode(button3, INPUT);
pinMode(button4, INPUT);
pinMode(BlinkerPinLeft, INPUT_PULLUP);
pinMode(BlinkerPinRight, INPUT_PULLUP);


//______________initialize smoothing inputs
  for (int thisReading = 0; thisReading < numReadings; thisReading++) {
    readings[thisReading] = 0;
  }
}

void loop() {

//______________READ SPD SENSOR
attachInterrupt(1,rpm, FALLING);

if (half_revolutions >= 1) {
    detachInterrupt(1);
    duration = (micros() - lastmillis);
    spd = half_revolutions * (0.000135 / (duration * 0.000001)) * 3600;
    lastmillis = micros(); 
    half_revolutions = 0;
    attachInterrupt(1, rpm, FALLING);
  }

//______________SMOOTH SPD TO AVERAGE
  total = total - readings[readIndex];
  // read from the sensor:
  readings[readIndex] = spd;
  // add the reading to the total:
  total = total + readings[readIndex];
  // advance to the next position in the array:
  readIndex = readIndex + 1;

  // if we're at the end of the array...
  if (readIndex >= numReadings) {
    // ...wrap around to the beginning:
    readIndex = 0;
  }

  // calculate the average:
  average = total / numReadings;
  // send it to the computer as ASCII digits
  
 
//______________READING BUTTONS AND SWITCHES
ClutchSwitchState = digitalRead(CluchSwitch);
buttonstate4 = digitalRead(button4);
buttonstate3 = digitalRead(button3);
buttonstate2 = digitalRead(button2);
buttonstate1 = digitalRead(button1);

//______________READING BLINKERS & LOGIC
boolean blinker_left = digitalRead(BlinkerPinLeft); // Left Blinker
  
  if (blinker_left){
    last_blinker_left = millis();
  }
  if (last_blinker_left + 500 < millis()){
      blinker_left_on = false;
  }else{
      blinker_left_on =  true;
  }

boolean blinker_right = digitalRead(BlinkerPinRight); // Right Blinker
  
  if (blinker_right){
    last_blinker_right = millis();
  }
  if (last_blinker_right + 500 < millis()){
      blinker_right_on = false;
  }else{
      blinker_right_on =  true;
  }
     
    

//______________SET OP OFF WHEN BRAKE IS PRESSED
       if (BRAKE_PRESSED == true)
       {
       OP_ON = false;
       }
 
    
//______________SET OP OFF WHEN GAS IS PRESSED
       if (GAS_RELEASED == false)
       {
       OP_ON = false;
       }
  
//______________SET BUTTON NR4
if (buttonstate4 != lastbuttonstate4)
    {
       if (buttonstate4 == LOW)
       {
          if (OP_ON == true)
          {
          OP_ON = false;
          }
          else
          {
          OP_ON = true;
          set_speed = (average + 3);
          }
        }
     }
     
//______________SET BUTTON NR3
if (buttonstate3 != lastbuttonstate3)
    {
       if (buttonstate3 == LOW)
       {
       set_speed = set_speed + 5;
       }
    }
//______________SET BUTTON NR2
if (buttonstate2 != lastbuttonstate2)
   {
       if (buttonstate2 == LOW)
       {
       set_speed = set_speed - 5;
       }
    }
    
//______________LIMIT FOR SETSPEED
if (set_speed > 200)
    { 
      set_speed = 0;
    }
    
//______________SET BUTTON NR1
if (buttonstate1 != lastbuttonstate1)
   {
       if (buttonstate1 == LOW)
       {
       OP_ON = false;
       }
   }

//______________SET CLUTCH SWITCH
if (ClutchSwitchState == LOW)
   {
  //  ("Clutch Pedal is pressed");
   }

//______________RESET BUTTONS & VALUES
lastbuttonstate1 = buttonstate1;
lastbuttonstate2 = buttonstate2;
lastbuttonstate3 = buttonstate3;
lastbuttonstate4 = buttonstate4;
lastBRAKE_PRESSED = BRAKE_PRESSED;
lastGAS_RELEASED = GAS_RELEASED;

//______________SENDING_CAN_MESSAGES
  //0x1d2 msg PCM_CRUISE
  uint8_t dat_1d2[8];
  dat_1d2[0] = (OP_ON << 5) & 0x20 | (GAS_RELEASED << 4) & 0x10;
  dat_1d2[1] = 0x0;
  dat_1d2[2] = 0x0;
  dat_1d2[3] = 0x0;
  dat_1d2[4] = 0x0;
  dat_1d2[5] = 0x0;
  dat_1d2[6] = (OP_ON << 7) & 0x80;
  dat_1d2[7] = can_cksum(dat_1d2, 7, 0x1d2);
  CAN.beginPacket(0x1d2);
  for (int ii = 0; ii < 8; ii++) {
    CAN.write(dat_1d2[ii]);
  }
  CAN.endPacket();

  //0x1d3 msg PCM_CRUISE_2
  uint8_t dat_1d3[8];
  dat_1d3[0] = 0x0;
  dat_1d3[1] = (MAIN_ON << 7) & 0x80 | 0x28;
  dat_1d3[2] = set_speed;
  dat_1d3[3] = 0x0;
  dat_1d3[4] = 0x0;
  dat_1d3[5] = 0x0;
  dat_1d3[6] = 0x0;
  dat_1d3[7] = can_cksum(dat_1d3, 7, 0x1d3);
  CAN.beginPacket(0x1d3);
  for (int ii = 0; ii < 8; ii++) {
    CAN.write(dat_1d3[ii]);
  }
  CAN.endPacket();

  //0xaa msg defaults 1a 6f WHEEL_SPEEDS
  uint8_t dat_aa[8];
  uint16_t wheelspeed = 0x1a6f + (average * 100);
  dat_aa[0] = (wheelspeed >> 8) & 0xFF;
  dat_aa[1] = (wheelspeed >> 0) & 0xFF;
  dat_aa[2] = (wheelspeed >> 8) & 0xFF;
  dat_aa[3] = (wheelspeed >> 0) & 0xFF;
  dat_aa[4] = (wheelspeed >> 8) & 0xFF;
  dat_aa[5] = (wheelspeed >> 0) & 0xFF;
  dat_aa[6] = (wheelspeed >> 8) & 0xFF;
  dat_aa[7] = (wheelspeed >> 0) & 0xFF;
  CAN.beginPacket(0xaa);
  for (int ii = 0; ii < 8; ii++) {
    CAN.write(dat_aa[ii]);
  }
  CAN.endPacket();
  
/* ************we are sending this message from BRAKE ECU for safetyness
  //0x3b7 msg ESP_CONTROL
  uint8_t dat_3b7[8];
  dat_3b7[0] = 0x0;
  dat_3b7[1] = 0x0;
  dat_3b7[2] = 0x0;
  dat_3b7[3] = 0x0;
  dat_3b7[4] = 0x0;
  dat_3b7[5] = 0x0;
  dat_3b7[6] = 0x0;
  dat_3b7[7] = 0x08;
  CAN.beginPacket(0x3b7);
  for (int ii = 0; ii < 8; ii++) {
    CAN.write(dat_3b7[ii]);
  }
  CAN.endPacket();
*/
  //0x620 msg STEATS_DOORS
  uint8_t dat_620[8];
  dat_620[0] = 0x10;
  dat_620[1] = 0x0;
  dat_620[2] = 0x0;
  dat_620[3] = 0x1d;
  dat_620[4] = 0xb0;
  dat_620[5] = 0x40;
  dat_620[6] = 0x0;
  dat_620[7] = 0x0;
  CAN.beginPacket(0x620);
  for (int ii = 0; ii < 8; ii++) {
    CAN.write(dat_620[ii]);
  }
  CAN.endPacket();

  // 0x3bc msg GEAR_PACKET
  uint8_t dat_3bc[8];
  dat_3bc[0] = 0x0;
  dat_3bc[1] = 0x0;
  dat_3bc[2] = 0x0;
  dat_3bc[3] = 0x0;
  dat_3bc[4] = 0x0;
  dat_3bc[5] = 0x80;
  dat_3bc[6] = 0x0;
  dat_3bc[7] = 0x0;
  CAN.beginPacket(0x3bc);
  for (int ii = 0; ii < 8; ii++) {
    CAN.write(dat_3bc[ii]);
  }
  CAN.endPacket();

  //0x614 msg steering_levers
  uint8_t dat_614[8];
  dat_614[0] = 0x29;
  dat_614[1] = 0x0;
  dat_614[2] = 0x01;
  dat_614[3] = (blinker_left_on << 5) & 0x20 |(blinker_right_on << 4) & 0x10;
  dat_614[4] = 0x0;
  dat_614[5] = 0x0;
  dat_614[6] = 0x76;
  dat_614[7] = can_cksum(dat_614, 7, 0x614);
  CAN.beginPacket(0x614);
  for (int ii = 0; ii < 8; ii++) {
    CAN.write(dat_614[ii]);
  }
  CAN.endPacket();

//______________READING CAN
  CAN.parsePacket();

  //128x2e6 msg LEAD_INFO
  if (CAN.packetId() == 0x2e6)
      {
      uint8_t dat_2e6[8];
      for (int ii = 0; ii <= 7; ii++) {
        dat_2e6[ii]  = (char) CAN.read();
        }
        LEAD_LONG_DIST_RAW = (dat_2e6[0] << 8 | dat_2e6[1] << 3); 
        LEAD_REL_SPEED_RAW = (dat_2e6[2] << 8 | dat_2e6[3] << 4);
        }
  //CONVERTING INTO RIGHT VALUE USING DBC SCALE
  LEAD_LONG_DIST = (LEAD_LONG_DIST_RAW * 0.005);
  LEAD_REL_SPEED = (LEAD_REL_SPEED_RAW * 0.009);

  
  //0x3b7 msg ESP_CONTROL --- WE are sending the 0x3b7 message from Brake_ECU, to reduce traffic on the can and improve safety
    if (CAN.packetId() == 0x3b7)
      {
      uint8_t dat_3b7[8];
      for (int ii = 0; ii <= 7; ii++) {
        dat_3b7[ii]  = (char) CAN.read();
        }
        BRAKE_PRESSED = (dat_3b7[0] << 5);
        }
  
    //0x2c1 msg GAS_PEDAL
    if (CAN.packetId() == 0x2c1)
      {
      uint8_t dat_2c1[8];
      for (int ii = 0; ii <= 7; ii++) {
        dat_2c1[ii]  = (char) CAN.read();
        }
        GAS_RELEASED = (dat_2c1[0] << 3);
        }
  
//______________LOGIC FOR LANE CHANGE RECOMENDITION
  if ((average * 100) >= LCR_minimum_speed){
  if (set_speed >= ((average * 100) + LCR_speed_diff))
   {
      if (LEAD_LONG_DIST <= LCR_lead_distance)
         {    
         blinker_left_on = false;
         }
      }
   }
  
/* //______________COUNTING LOOPS AND BOOBS
if((last_loop_calc +5000) < millis()){
  last_loop_calc  = millis() - last_loop_calc;
  
  ausgabe_loop_sec  = (loop_count        *1000)   / last_loop_calc;
  ausgabe_int_sec   = (interrupt_counter *1000)   / last_loop_calc;
  loop_count        = 0;
  interrupt_counter = 0;
  last_loop_calc    = millis();
  }
  loop_count++;
//  Serial.println (ausgabe_loop_sec);
*/

/* 
//______________RUN BLYNK APP
Blynk.run();
timer.run(); // Initiates BlynkTimer
*/  
} //______________END OF LOOP


void rpm() {
  half_revolutions++;
  if (encoder > 255)
  {
    encoder = 0;
  }
  encoder++;
}

//TOYOTA CAN CHECKSUM
int can_cksum (uint8_t *dat, uint8_t len, uint16_t addr) {
  uint8_t checksum = 0;
  checksum = ((addr & 0xFF00) >> 8) + (addr & 0x00FF) + len + 1;
  //uint16_t temp_msg = msg;
  for (int ii = 0; ii < len; ii++) {
    checksum += (dat[ii]);
  }
  return checksum;
}
