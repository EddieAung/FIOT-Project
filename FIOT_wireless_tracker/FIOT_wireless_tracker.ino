//libs

#include <SoftwareSerial.h>
#include <wire.h>
#include
#include

//defs to change the names if needed
#define threeaxisSDA A4 //SDA
#define threeaxisSCL A5 //SCL
#define SD_MISO 12
#define SD_MOSI 11
#define SD_SCK 13
#define SD_CS 10
#define Buzzer 9
#define LED 8
#define DIP1 6
#define DIP2 7
#define Soft serial Tx (NEO-6M) 5
#define Soft serial Rx (NEO-6M) 4
#define Soft serial Tx (ESP01) 3
#define Soft serial Rx (ESP01) 2

void setup() {
  // put your setup code here, to run once:
  pinMode(8,OUTPUT);
  pinMode(9,OUTPUT);
  pinMode(7,INPUT);
  pinMode(6,INPUT);
}

void loop() {
  int alarm=0;//to turn 
//ALARM MODE
while(x==1)
{digitalWrite(LED,HIGH);
//digitalWrite(9,HIGH);
delay(500);
digitalWrite(LED,LOW);
//digitalWrite(9,LOW);
delay(500);
}
}
//to look at files and check for sample code for the various sensors. under downloads under the file name Arudiuno sample code for IO devices 
// GPS sample code
SoftwareSerial GPS_Serial(4, 5); // Rx, Tx

void setup()
{
  Serial.begin(9600);
  GPS_Serial.begin(9600);
}

void loop()
{
  while (GPS_Serial.available() > 0) {
    Serial.write(GPS_Serial.read()); // Output the raw GPS data to the serial monitor
  }
}
