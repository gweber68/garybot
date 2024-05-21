#include <Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define SENSOR_TRIG_PIN 2
#define SENSOR_ECHO_PIN 3
#define SENSOR_MIN_DIST 30

/********************************************************* 
 *  CONTROL VARIABLES
 *********************************************************/
 
// Normal operation:  debug=0, immobile = 0, wait = 1
int debug = 1;
int immobile = 0;
int wait = 1;

// motor stuff:
int E1 = 5; //M1 Speed Control
int E2 = 6; //M2 Speed Control
int M1 = 4; //M1 Direction Control
int M2 = 7; //M1 Direction Control

char forward_right_motor = 180; // Speed for forward movement
char forward_left_motor = 198; // Speed for forward movement
char turn_rate = 180; // Speed for turns
char backward_rate = 180; // Speed for reverse

char left_back_L_motor = 255;
char left_back_R_motor = 50;
char right_back_L_motor = 50;
char right_back_R_motor = 255;
int backup_turn_duration = 1250;

// servo stuff:
int servoCenter = 91;
int servoRight = 151; // how far right? Ma x is 180, full right.
int servoLeft = 31; // how far left? Min is 0, full left.
Servo myservo; // create servo object to control a servo

// LCD stuff
// Set the LCD address to 0x27 (Could be 0x20 if solder jumpers are bridged!)
// Set the pins on the I2C chip used for LCD connections:
//                    addr,en,rw,rs,d4,d5,d6,d7,bl,blpol
LiquidCrystal_I2C LCD(0x27, 2, 1, 0, 4, 5, 6, 7, 3, POSITIVE);  // Set the LCD I2C address

// button stuff
char msgs[5][15] = {
  "Right Key OK ", 
  "Up Key OK    ", 
  "Down Key OK  ", 
  "Left Key OK  ", 
  "Select Key OK" };
char start_msg[15] = {
  "Start loop "};                    
int  adc_key_val[5] ={
  30, 150, 360, 535, 760 };
int NUM_KEYS = 5;
int adc_key_in;
int key=-1;
int oldkey=-1;
byte randNum;

void setup() {

  // Sensor setup (HC SR04)
  pinMode(SENSOR_TRIG_PIN, OUTPUT);
  pinMode(SENSOR_ECHO_PIN, INPUT);

  // Servo setup
  myservo.attach(9);  // attaches the servo on pin 9 to the servo object
  myservo.write(servoCenter); // center it by default

  // Motor setup
  pinMode(M1, OUTPUT);   
  pinMode(M2, OUTPUT); 

  // LCD setup
  LCD.begin(16, 2);  // 16 lines by 2 rows
  LCD.clear();
  LCD.backlight();

  Serial.begin(9600);
  randomSeed(analogRead(0));

  if ( debug == 1 ) {
    /* Print that we made it here */
    Serial.println("setup()");
    Serial.println("Wait loop - press button to start robot");
  }
}

void loop() {
  // delay(50);

  int dir;

  if ( wait ) {
    adc_key_in = analogRead(7);    // read the value from the sensor  
    /* get the key */
    key = get_key(adc_key_in);    // convert into key press
    if (key != oldkey) {   // if keypress is detected
      delay(50);      // wait for debounce time
      adc_key_in = analogRead(7);    // read the value from the sensor  
      key = get_key(adc_key_in);    // convert into key press
      if (key != oldkey) {         
        oldkey = key;
        if (key >= 0){
          wait = 0;
          if ( debug == 1 ) Serial.println("Wait loop - done!");
        }
      }
    }
  } else {
    if (debug) Serial.print("Distance forward: ");
    float ping = ping_distance();
    if (debug) Serial.println(ping);
  
    if ( ping > SENSOR_MIN_DIST ) {
      go_forward();
    }
  
    if ( ping <= SENSOR_MIN_DIST ) {

      LCD.setCursor(0,0);
      LCD.print("Obstruction!");  
      
      brake();
      delay(1000);
      
      dir = go_look();
      delay(20);
  
      // Decide which way is most open

      LCD.clear();

      // dir = 1 means that left is either blocked or less open than right
      if (dir == 1) {
        if (debug) Serial.println("Turning right!");
        LCD.setCursor(0,0);
        LCD.print("Turning right!");  
        /*
        go_backwards();
        delay(20);
        turn_right();
        */
        backup_right();      
      }

      // dir = 2 means that right is either blocked or less open than left
      if (dir == 2) {
        if (debug) Serial.println("Turning left!");
        LCD.setCursor(0,0);
        LCD.print("Turning left!");  
        /*
        go_backwards();
        delay(20);
        turn_left();
        */
        backup_left();
      }

      // dir = 3 means that both ways are blocked, we should back up and try a random direction
      if (dir == 3) {
        LCD.setCursor(0,0);
        LCD.print("Gotta back up!");
        go_backwards();
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

    //delay(125);
  
  } // wait
}

int go_look () {

  LCD.clear();
  // look both ways for better path
  
  // Look left
  if (debug) Serial.print("Left distance: ");
  myservo.write(servoLeft);
  delay(1000);
  // read the distance from servoLeft direction
  float distance_left = ping_distance();  
  if (debug) Serial.println(distance_left);
  String tmpStr = "Left: ";
  tmpStr = tmpStr + distance_left;
  LCD.setCursor(0,0);
  LCD.print(tmpStr);  
  
  // Look right
  if (debug) Serial.print("Right distance: ");
  myservo.write(servoRight);
  delay(1000);
  // read the distance from servoRight direction
  float distance_right = ping_distance();
  if (debug) Serial.println(distance_right);
  tmpStr = "Right: ";
  tmpStr = tmpStr + distance_right;
  LCD.setCursor(0,1);
  LCD.print(tmpStr);  
  
  // Return to center
  myservo.write(servoCenter);
  delay(2000);

  int retval = 0;
  
  // Both ways are blocked, backup!
  if (( distance_left <= SENSOR_MIN_DIST ) && ( distance_right <= SENSOR_MIN_DIST )) {
    if (debug) Serial.print("Both left & right are blocked! ");
    return 3;
  }
  
  if ( distance_left <= SENSOR_MIN_DIST ) {
    if (debug) Serial.print("Left is blocked! ");
    return 1;
  }
  if ( distance_right <= SENSOR_MIN_DIST ) {
    if (debug) Serial.print("Right is blocked! ");
    return 2;
  }

  // Both seem okay, but which way is better (larger ping return)?
  if ( distance_left > distance_right ) {
    if (debug) Serial.println("Left is more open!!!");
    return 2;
  } else {
    if (debug) Serial.println("Right is more open!!!");
    return 1;
  }
  
}

float ping_distance()
{
  float cm;
  digitalWrite(SENSOR_TRIG_PIN, LOW); //Low high and low level take a short time to pulse
  delayMicroseconds(2);
  digitalWrite(SENSOR_TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(SENSOR_TRIG_PIN, LOW);
  
  cm = pulseIn(SENSOR_ECHO_PIN, HIGH) / 58.0; //Echo time conversion into cm
  cm = abs((int(cm * 100.0)) / 100.0); //Keep two decimal places, also convert to positive if needed

  return(cm);
}

void go_forward() {
  if (debug) Serial.println("Going forward…");
  
  if (! immobile) digitalWrite(M1,HIGH);
  if (! immobile) digitalWrite(M2,HIGH); 
  if (! immobile) analogWrite (E1,forward_right_motor);
  if (! immobile) analogWrite (E2,forward_left_motor);
}

void go_backwards() {
  if (debug) Serial.println("Reverse!!!");
  
  if (! immobile) digitalWrite(M1,LOW);
  if (! immobile) digitalWrite(M2,LOW);
  if (! immobile) analogWrite (E1,backward_rate);
  if (! immobile) analogWrite (E2,backward_rate);
  delay(2000); 
}

void turn_left() {
  if (debug) Serial.println("Turning left…");
  
  if (! immobile) digitalWrite(M1,HIGH);
  if (! immobile) digitalWrite(M2,LOW);
  if (! immobile) analogWrite (E1,turn_rate);
  if (! immobile) analogWrite (E2,turn_rate);
  delay(1200);
}

void turn_right() {
  if (debug) Serial.println("Turning right…");
  
  if (! immobile) digitalWrite(M1,LOW);
  if (! immobile) digitalWrite(M2,HIGH);
  if (! immobile) analogWrite (E1,turn_rate);
  if (! immobile) analogWrite (E2,turn_rate);
  delay(1200);
}

void backup_left() {
  if (debug) Serial.println("Backup left…");
  
  if (! immobile) digitalWrite(M1,LOW);
  if (! immobile) digitalWrite(M2,LOW);
  if (! immobile) analogWrite (E1,left_back_R_motor);
  if (! immobile) analogWrite (E2,left_back_L_motor);
  delay(backup_turn_duration);
}

void backup_right() {
  if (debug) Serial.println("Backup right…");
  
  if (! immobile) digitalWrite(M1,LOW);
  if (! immobile) digitalWrite(M2,LOW);
  if (! immobile) analogWrite (E1,right_back_R_motor);
  if (! immobile) analogWrite (E2,right_back_L_motor);
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

