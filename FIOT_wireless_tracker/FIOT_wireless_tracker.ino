//libs
#include <SoftwareSerial.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <TinyGPS++.h>

TinyGPSPlus gps;

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
#define DEBUG true

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
float latitude,longitude;
int x,y,z;
int xref,yref,zref;
bool ref = false ;


// replace with your Thingspeak channel's API key!!!
String apiKey = "N5XSN9TXBMCARV84";

void setup() {
  // pinModes 
  pinMode(LED,OUTPUT);
  pinMode(Buzzer,OUTPUT);
  pinMode(DIP2,INPUT);
  pinMode(DIP1,INPUT);
  //Serial monitor
  Serial.begin(9600);
  GPS_Serial.begin(9600);
  Serial.println("Starting...");
  WiFi_Serial.begin(9600);
  //3-Axis
  Wire.begin();        // join i2c bus (address optional for master)
  Serial.print("init");
  //Put the ADXL345 into +/- 4G range by writing the value 0x01 to the DATA_FORMAT register.
  writeTo(DATA_FORMAT, 0x01);
  //Put the ADXL345 into Measurement Mode by writing 0x08 to the POWER_CTL register.
  writeTo(POWER_CTL, 0x08);

    // Reset ESP8266
    sendData("AT+RST\r\n", 3000, DEBUG);

    // Set WiFi mode
    sendData("AT+CWMODE=1\r\n", 2000, DEBUG);

    // Connect to WiFi
    sendData("AT+CWJAP=\"Flippty floppity fosh\",\"patrickthestarfish\"\r\n", 30000, DEBUG);

    // Single connection mode
    sendData("AT+CIPMUX=0\r\n", 2000, DEBUG);

}

void loop() {
  int mode,mode2;
  mode= digitalRead(DIP1); //DIP1 is switch 2 on board
  mode2=digitalRead(DIP2);//DIP2 is Switch 1 on board  

  if(mode==HIGH)
  {
    ReadGPS();

    if (gps.location.isValid())
    {
   // Wifi();
    }
  }
  //stationary mode
  if(mode2==HIGH)
  {
    readAccel();
    if(ref==false){
      xref=x;
      yref=y;
      zref=z;
      ref=true;
    }
  }
  //ALARM MODE
  if(ref)
  {
    if(abs(x-xref) > 100 ||
       abs(y-yref) > 100 ||
       abs(z-zref) > 100)
      {
    digitalWrite(LED,HIGH);
    digitalWrite(Buzzer,HIGH);
    delay(500);
    digitalWrite(LED,LOW);
    digitalWrite(Buzzer,LOW);
    delay(500);
    }
  }
  if(mode2==LOW)
  {ref=false;}
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

//GPS
void ReadGPS()
{
    GPS_Serial.listen();

    while (GPS_Serial.available())
    {
        char c = GPS_Serial.read();

        gps.encode(c);

        if (gps.location.isUpdated())
        {
            latitude = gps.location.lat();
            longitude = gps.location.lng();

            Serial.print("Latitude: ");
            Serial.println(gps.location.lat(), 6);

            Serial.print("Longitude: ");
            Serial.println(gps.location.lng(), 6);
        }
    }
if (gps.location.isValid())
{
    Serial.println("GPS FIX!");
}
else
{
    delay(5000);
    Serial.println("I think we are lost guys GGS!");
}
}

//Wifi module. 
void Wifi()
{
    WiFi_Serial.listen();

    // Connect to ThingSpeak
    sendData("AT+CIPSTART=\"TCP\",\"api.thingspeak.com\",80\r\n", 5000, DEBUG);

    // Create HTTP GET request
   String getStr = "GET /update?api_key=";
  //getStr += apiKey;
  //getStr += "&field1=";
  //getStr += String(latitude, 6);
  //getStr += "&field2=";
  //getStr += String(longitude, 6);
  getStr += "\r\n\r\n";
  getStr += "1.290270";      // Singapore latitude
getStr += "&field2=";
getStr += "103.851959";    // Singapore longitude

    // Tell ESP how many bytes will be sent
    WiFi_Serial.print("AT+CIPSEND=");
    WiFi_Serial.println(getStr.length());

    delay(1000);

    if (WiFi_Serial.find(">"))
    {
        sendData(getStr, 5000, DEBUG);
    }
    else
    {
        Serial.println("CIPSEND Failed");
    }

    // Close TCP connection
    sendData("AT+CIPCLOSE\r\n", 2000, DEBUG);
    GPS_Serial.listen();
}

String sendData(String command, const int timeout, boolean debug)
{
    String response = "";

    WiFi_Serial.print(command);

    long startTime = millis();

    while ((millis() - startTime) < timeout)
    {
        while (WiFi_Serial.available())
        {
            char c = WiFi_Serial.read();
            response += c;
        }
    }

    if (debug)
    {
        Serial.println(response);
    }

    return response;
}
//to look at files and check for sample code for the various sensors. Under the file name Arudiuno sample code for IO devices

//to take a look at thinkspeak code as well 
//to do sd,wifi app and thinkspeak




