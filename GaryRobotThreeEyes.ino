// #include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

/********************************************************* 
 *  CONTROL CONSTANTS & VARIABLES
 *********************************************************/

// Normal operation:  debug=0, immobile=0, wait_to_start=1

// Constants
const int debug = 1;
const int immobile = 0;

// Variable default values
int wait_to_start = 1;

/********************************************************* 
 *  GLOBAL CONSTANTS
 *********************************************************/

#define SENSOR1_TRIG_PIN 2
#define SENSOR1_ECHO_PIN 3
#define SENSOR2_TRIG_PIN 8
#define SENSOR2_ECHO_PIN 9
#define SENSOR3_TRIG_PIN 10
#define SENSOR3_ECHO_PIN 11
#define SENSOR_MIN_DIST 30
#define SENSOR_AVOID_DIST 50

// Various display delay settings
const int WAIT_before_changing_dir = 500;
const int WAIT_go_look_display = 1000;
const int WAIT_after_go_look = 20;

// Motor pins
int E1 = 5; //M1 Speed Control
int E2 = 6; //M2 Speed Control
int M1 = 4; //M1 Direction Control
int M2 = 7; //M1 Direction Control

/****************************************
 * Motor Speed Offset Multiplier
 ***************************************/
float motor_offset = .5;

// Forward movement rates
int forward_motorL = 210 * motor_offset;
int forward_motorR = 175 * motor_offset;

// Forward slight turn rates
int forward_left_motorL = 75 * motor_offset;
int forward_left_motorR = 220 * motor_offset;
int forward_right_motorL = 220 * motor_offset;
int forward_right_motorR = 75 * motor_offset;

// Backward movement rates
int backward_rate_motorL = 180 * motor_offset;
int backward_rate_motorR = 180 * motor_offset;

// Backward-turn rates
int left_back_motorL = 190;
int left_back_motorR = 75;
int right_back_motorL = 75;
int right_back_motorR = 190;
int backup_turn_duration = 1000;

//// Servo stuff: Currently unused, as we no longer use a movable eye to look around.
//// (Instead we now have three separate eyes to look in three directions at once.)
// int servoCenter = 91;
// int servoRight = 151; // how far right? Ma x is 180, full right.
// int servoLeft = 31; // how far left? Min is 0, full left.
// Servo myservo; // create servo object to control a servo

// LCD stuff
// Set the LCD address to 0x27 (Could be 0x20 if solder jumpers are bridged!)
// Set the pins on the I2C chip used for LCD connections:
//                    addr,en,rw,rs,d4,d5,d6,d7,bl,blpol
LiquidCrystal_I2C LCD(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);  // Set the LCD I2C address

// Button stuff
int adc_key_val[5] = { 30, 150, 360, 535, 760 };
int NUM_KEYS = 5;
int key_in;
int key=-1;
int oldkey=-1;
byte randNum;

void setup() {

  // Sensor setup (HC SR04)
  pinMode(SENSOR1_TRIG_PIN, OUTPUT);
  pinMode(SENSOR1_ECHO_PIN, INPUT);
  pinMode(SENSOR2_TRIG_PIN, OUTPUT);
  pinMode(SENSOR2_ECHO_PIN, INPUT);
  pinMode(SENSOR3_TRIG_PIN, OUTPUT);
  pinMode(SENSOR3_ECHO_PIN, INPUT);

  //// Servo setup
  //myservo.attach(9);  // attaches the servo on pin 9 to the servo object
  //myservo.write(servoCenter); // center it by default

  // Motor setup
  pinMode(M1, OUTPUT);   
  pinMode(M2, OUTPUT); 

  // LCD setup
  LCD.begin(16, 2);  // 16 lines by 2 rows
  LCD.clear();
  LCD.backlight();

  Serial.begin(9600);
  randomSeed(analogRead(0));

  if (debug) {
    /* Print that we made it here */
    Serial.println("setup()");
    Serial.println("Wait loop - press button to start robot");
    LCD.setCursor(0,0);
    LCD.print("System Booted!");
    delay(1000);
    LCD.setCursor(0,0);
    LCD.print("Press button to");
    LCD.setCursor(5,1);
    LCD.print("start!");
  }
}

void loop() {
  // delay(50);

  int dir;
  float ping1, ping2, ping3;
  String tmpStr;

  if ( wait_to_start ) {
    key_in = analogRead(7);   // read the button value  
    key = get_key(key_in);    // convert into key press

    if (key != oldkey) {      // if keypress is detected
      delay(50);              // wait for debounce time
      
      key_in = analogRead(7); // read the button value again
      key = get_key(key_in);  // convert into key press

      if (key != oldkey) {         
        oldkey = key;
        if (key >= 0){
          wait_to_start = 0;
          if (debug) Serial.println("Wait loop - done!");
          LCD.clear();
        }
      }
    }

  } else {
    
    ping1 = ping_distance(SENSOR1_TRIG_PIN, SENSOR1_ECHO_PIN);
    tmpStr = "    Fwd:";
    tmpStr = tmpStr + (int)ping1 + "      ";
    LCD.setCursor(0,0);
    LCD.print(tmpStr);  
    if (debug) Serial.println(tmpStr);
   
    ping2 = ping_distance(SENSOR2_TRIG_PIN, SENSOR2_ECHO_PIN);
    LCD.setCursor(0,1);
    tmpStr = "Lft:";
    tmpStr = tmpStr + (int)ping2 + "  ";
    LCD.print(tmpStr);
    LCD.print("  ");
    if (debug) Serial.println(tmpStr);
  
    ping3 = ping_distance(SENSOR3_TRIG_PIN, SENSOR3_ECHO_PIN);
    tmpStr = "Rgt:";
    tmpStr = tmpStr + (int)ping3 + "    ";
    LCD.setCursor(9,1);
    LCD.print(tmpStr);  
    if (debug) Serial.println(tmpStr);
  
    if (( ping1 >= SENSOR_MIN_DIST ) && ( ping2 < SENSOR_AVOID_DIST )) {
      // Is forward okay, but left direction starting to get obstructed?
      // Begin slight right turn
      forward_right();
    } else if (( ping1 >= SENSOR_MIN_DIST ) && ( ping3 < SENSOR_AVOID_DIST )) {
      // Is forward okay, but right direction starting to get obstructed?
      // Begin slight left turn
      forward_left();
    } else if ( ping1 >= SENSOR_MIN_DIST ) {
      // Is forward direction still okay?
      go_forward();
    }
     
    // Is forward direction actually getting obstructed?
    if ( ping1 < SENSOR_MIN_DIST ) {

      LCD.setCursor(0,0);
      LCD.print("Blocked: ");
      LCD.print(ping1);
      
      brake();
      delay(WAIT_before_changing_dir);
      
      /**********************************************************
       ** Decide which way is most open
       **********************************************************/

      // Get a suggested direction to go 
      dir = go_look();
      
      delay(WAIT_after_go_look);
      LCD.clear();

      // dir = 1 means that left is either blocked or less open than right
      if (dir == 1) {
        if (debug) Serial.println("Turning right!");
        LCD.setCursor(0,0);
        LCD.print("Turning right!");  
        backup_right();      
      }

      // dir = 2 means that right is either blocked or less open than left
      if (dir == 2) {
        if (debug) Serial.println("Turning left!");
        LCD.setCursor(0,0);
        LCD.print("Turning left!");  
        backup_left();
      }
      
      // dir = 3 means that both ways are blocked, we should back up and try a random direction
      if (dir == 3) {
        LCD.setCursor(0,0);
        LCD.print("Gotta back up!");
        backup();
        randNum = random(1,3);
        Serial.print("randNum: ");
        Serial.println(randNum);
        if (randNum == 1) {
          LCD.setCursor(0,0);
          LCD.print("Turning right!");  
          backup_right();
        } else {
          LCD.setCursor(0,0);
          LCD.print("Turning left!");  
          backup_left();
        }
      }

      brake();
      LCD.clear();
      delay(20);
    }

    delay(250);
  
  } // wait
}

int go_look () {

  // look both ways for better path

  String tmpStr;
  LCD.clear();
  
  // Read the distance from the left sensor
  float distance_left = ping_distance(SENSOR2_TRIG_PIN, SENSOR2_ECHO_PIN);
  tmpStr = " Left: ";
  tmpStr = tmpStr + distance_left;
  LCD.setCursor(0,0);
  LCD.print(tmpStr);  
  if (debug) Serial.println(tmpStr);
  
  // Read the distance from right sensor
  float distance_right = ping_distance(SENSOR3_TRIG_PIN, SENSOR3_ECHO_PIN);
  tmpStr = "Right: ";
  tmpStr = tmpStr + distance_right;
  LCD.setCursor(0,1);
  LCD.print(tmpStr);  
  if (debug) Serial.println(tmpStr);

  // Delay after displaying values
  delay(WAIT_go_look_display);

  // Both ways are blocked, backup!
  if (( distance_left <= SENSOR_MIN_DIST ) && ( distance_right <= SENSOR_MIN_DIST )) {
    if (debug) Serial.print("Both left & right are blocked! ");
    return 3;
  }
  
  // Right is actually blocked, let's turn left for sure
  if ( distance_left <= SENSOR_MIN_DIST ) {
    if (debug) Serial.print("Left is blocked! ");
    return 1;
  }

  // Left is blocked, gotta turn right for sure
  if ( distance_right <= SENSOR_MIN_DIST ) {
    if (debug) Serial.print("Right is blocked! ");
    return 2;
  }

  // Both seem okay, but which way is better (longer open distance)?
  if ( distance_left > distance_right ) {
    if (debug) Serial.println("Left is more open!!!");
    return 2;
  } else {
    if (debug) Serial.println("Right is more open!!!");
    return 1;
  }
  
}

float ping_distance(int trigger, int echo)
{
  float cm;
  digitalWrite(trigger, LOW); //Low high and low level take a short time to pulse
  delayMicroseconds(2);
  digitalWrite(trigger, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigger, LOW);
  
  cm = pulseIn(echo, HIGH) / 58.0; //Echo time conversion into cm
  cm = abs((int(cm * 100.0)) / 100.0); //Keep two decimal places, also convert to positive if needed

  return(cm);
}

void go_forward() {
  if (debug) Serial.println("Forward...");

  if (debug) Serial.print("---motorL: ");
  if (debug) Serial.println(forward_motorL);
  if (debug) Serial.print("---motorR: ");
  if (debug) Serial.println(forward_motorR);
  
  if (! immobile) digitalWrite(M1,HIGH);
  if (! immobile) digitalWrite(M2,HIGH); 
  if (! immobile) analogWrite (E1,forward_motorR);
  if (! immobile) analogWrite (E2,forward_motorL);
}

void forward_left() {

  String tmpStr = "<<< Slight left ";
  if (debug) Serial.println(tmpStr);
  LCD.setCursor(0,0);
  LCD.print(tmpStr);

  if (debug) Serial.print("---motorL: ");
  if (debug) Serial.println(forward_motorL);
  if (debug) Serial.print("---motorR: ");
  if (debug) Serial.println(forward_motorR);
  
  if (! immobile) digitalWrite(M1,HIGH);
  if (! immobile) digitalWrite(M2,HIGH); 
  if (! immobile) analogWrite (E1,forward_left_motorR);
  if (! immobile) analogWrite (E2,forward_left_motorL);
}

void forward_right() {
  String tmpStr = ">>> Slight right";
  if (debug) Serial.println(tmpStr);
  
  LCD.setCursor(0,0);
  LCD.print(tmpStr);

  if (debug) Serial.print("---motorL: ");
  if (debug) Serial.println(forward_motorL);
  if (debug) Serial.print("---motorR: ");
  if (debug) Serial.println(forward_motorR);
  
  if (! immobile) digitalWrite(M1,HIGH);
  if (! immobile) digitalWrite(M2,HIGH); 
  if (! immobile) analogWrite (E1,forward_right_motorR);
  if (! immobile) analogWrite (E2,forward_right_motorL);
}

void backup() {
  if (debug) Serial.println("Reverse!!!");
  
  if (! immobile) digitalWrite(M1,LOW);
  if (! immobile) digitalWrite(M2,LOW);
  if (! immobile) analogWrite (E1,backward_rate_motorL);
  if (! immobile) analogWrite (E2,backward_rate_motorR);
  delay(2000); 
}

void backup_left() {
  if (debug) Serial.println("Backup left…");

  if (debug) Serial.print("---motorL: ");
  if (debug) Serial.println(left_back_motorL);
  if (debug) Serial.print("---motorR: ");
  if (debug) Serial.println(left_back_motorR);
    
  if (! immobile) digitalWrite(M1,LOW);
  if (! immobile) digitalWrite(M2,LOW);
  if (! immobile) analogWrite (E1,left_back_motorR);
  if (! immobile) analogWrite (E2,left_back_motorL);
  delay(backup_turn_duration);
}

void backup_right() {
  if (debug) Serial.println("Backup right…");
  if (debug) Serial.println(left_back_motorL);
  if (debug) Serial.println(left_back_motorR);
  
  if (debug) Serial.print("---motorL: ");
  if (debug) Serial.println(right_back_motorL);
  if (debug) Serial.print("---motorR: ");
  if (debug) Serial.println(right_back_motorR);
    
  if (! immobile) digitalWrite(M1,LOW);
  if (! immobile) digitalWrite(M2,LOW);
  if (! immobile) analogWrite (E1,right_back_motorR);
  if (! immobile) analogWrite (E2,right_back_motorL);
  delay(backup_turn_duration);
}

void brake() {
  if (debug) Serial.println("Braking…");
  
  if (! immobile) digitalWrite(E1,LOW);
  if (! immobile) digitalWrite(E2,LOW); 
}

int get_key(unsigned int input)
{   
  int k;
  for (k = 0; k < NUM_KEYS; k++)
  {
    if (input < adc_key_val[k])
    {  
      return k;  
    }
  }
  
  if (k >= NUM_KEYS)
    k = -1;     // No valid key pressed

  return k;
}
