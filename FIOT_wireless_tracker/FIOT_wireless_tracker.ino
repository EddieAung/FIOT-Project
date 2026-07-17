//libs
#include <SoftwareSerial.h>
#include <wire.h>
#include <SPI.h>
#include <SD.h>

//defs 
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
SoftwareSerial GPS_Serial(GPS_RX, GPS_TX);


void setup() {
  // pinModes 
  pinMode(8,OUTPUT);
  pinMode(9,OUTPUT);
  pinMode(7,INPUT);
  pinMode(6,INPUT);
  //Serial monitor
  Serial.begin(9600);
  GPS_Serial.begin(9600);
}

void loop() {
  int alarm=0;//to change
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
//to look at files and check for sample code for the various sensors. Under the file name Arudiuno sample code for IO devices 
// GPS sample code
void loop()
{
  while (GPS_Serial.available() > 0) {
    Serial.write(GPS_Serial.read()); // Output the raw GPS data to the serial monitor
  }
}


//sd card sample code

const int chipSelect = 10;

void setup() {
  Serial.begin(9600);
  while (!Serial); // wait for Serial Monitor to open
  Serial.print("Initializing SD card...");
  if (!SD.begin(chipSelect)) {
    Serial.println("Card failed, or not present");
    return;
  }
  Serial.println("card initialized.");
}

void loop() {
  int sensor = analogRead(A0);
  String dataString = String(sensor);

  File dataFile = SD.open("datalog.txt", FILE_WRITE); // use 8.3 file name
  if (dataFile) {
    dataFile.println(dataString);
    dataFile.close();
    Serial.println(dataString); // echo on Serial Monitor
  }
  else {
    Serial.println("error opening datalog.txt");
  }

  delay(5000);
}

