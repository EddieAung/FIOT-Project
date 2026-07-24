//libs
#include <SoftwareSerial.h>
#include <Wire.h>
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
#define GPS_TX 5
#define GPS_RX 4
#define Wifi_TX 3
#define Wifi_RX 2
SoftwareSerial GPS_Serial(GPS_RX, GPS_TX);
SoftwareSerial WiFi_Serial(Wifi_RX, Wifi_TX);

//3axis 
#define DEVICE (0x53) // Device address as specified in data sheet 
byte _buff[6];

char POWER_CTL = 0x2D;  //Power Control Register
char DATA_FORMAT = 0x31;
char DATAX0 = 0x32; //X-Axis Data 0
char DATAX1 = 0x33; //X-Axis Data 1
char DATAY0 = 0x34; //Y-Axis Data 0
char DATAY1 = 0x35; //Y-Axis Data 1
char DATAZ0 = 0x36; //Z-Axis Data 0
char DATAZ1 = 0x37; //Z-Axis Data 1

int x,y,z;
int refX, refY, refZ; // Stored stationary values
bool referenceSaved = false;

// Function Prototype
void readGPS();
void readAccel();
void writeTo(byte address, byte val);
void readFrom(byte address, int num, byte buffer[]);

void setup() {
  // pinModes 
  pinMode(LED,OUTPUT);
  pinMode(Buzzer,OUTPUT);
  pinMode(DIP2,INPUT);
  pinMode(DIP1,INPUT);
  //Serial monitor
  Serial.begin(9600);
  GPS_Serial.begin(9600);
  WiFi_Serial.begin(19200);
  GPS_Serial.listen(); // make the GPS port the active listening one so that it will 
  //3-Axis
  Wire.begin();        // join i2c bus (address optional for master)
  Serial.print("init");
  //Put the ADXL345 into +/- 4G range by writing the value 0x01 to the DATA_FORMAT register.
  writeTo(DATA_FORMAT, 0x01);
  //Put the ADXL345 into Measurement Mode by writing 0x08 to the POWER_CTL register.
  writeTo(POWER_CTL, 0x08);
}

void loop() {
int mode,stationarymode;
mode= digitalRead(DIP1); //DIP1 is DIP switch 2 on board
stationarymode=digitalRead(DIP2);//DIP2 is DIP Switch 1 on board  

if(digitalRead(DIP1)==HIGH)
{
readGPS();
}

if(stationarymode==HIGH)
{
  readAccel();
  if (referenceSaved==false){ 
  refX=x;
  refY=y;
  refZ=z;
  referenceSaved=true;
  }
}
if(stationarymode == LOW)
{
    referenceSaved = false;
}

//ALARM MODE
if(referenceSaved)
{
    readAccel();

    if(abs(x-refX) > 20 ||
       abs(y-refY) > 20 ||
       abs(z-refZ) > 20)
    {
        digitalWrite(LED,HIGH);
        digitalWrite(Buzzer,HIGH);
        digitalWrite(LED,LOW);
        digitalWrite(Buzzer,HIGH);
    }
}

}

//functions 
void readAccel() {
  uint8_t howManyBytesToRead = 6;
  readFrom( DATAX0, howManyBytesToRead, _buff); //read the acceleration data from the ADXL345

  // each axis reading comes in 10 bit resolution, ie 2 bytes.  Least Significat Byte first!!
  // thus we are converting both bytes in to one int
  x = (((int)_buff[1]) << 8) | _buff[0];   
  y = (((int)_buff[3]) << 8) | _buff[2];
  z = (((int)_buff[5]) << 8) | _buff[4];
  Serial.print("x: ");
  Serial.print( x );
  Serial.print(" y: ");
  Serial.print( y );
  Serial.print(" z: ");
  Serial.println( z );
}

void writeTo(byte address, byte val) {
  Wire.beginTransmission(DEVICE); // start transmission to device 
  Wire.write(address);             // send register address
  Wire.write(val);                 // send value to write
  Wire.endTransmission();         // end transmission
}

// Reads num bytes starting from address register on device in to _buff array
void readFrom(byte address, int num, byte _buff[]) {
  Wire.beginTransmission(DEVICE); // start transmission to device 
  Wire.write(address);             // sends address to read from
  Wire.endTransmission();         // end transmission

  Wire.beginTransmission(DEVICE); // start transmission to device
  Wire.requestFrom(DEVICE, num);    // request 6 bytes from device

  int i = 0;
  while(Wire.available())         // device may send less than requested (abnormal)
  { 
    _buff[i] = Wire.read();    // receive a byte
    i++;
  }
  Wire.endTransmission();         // end transmission
}

void readGPS()
{
  //GPS
while (GPS_Serial.available() > 0)
    {
    Serial.write(GPS_Serial.read()); // Output the raw GPS data to the serial monitor
    delay(50000);
    }
}

//to look at files and check for sample code for the various sensors. Under the file name Arudiuno sample code for IO devices

//to take a look at thinkspeak code as well 
//to do sd,wifi app and thinkspeak
