// ==========================================================
//  FIoT Project - GPS tracker + 3-axis motion alarm
//  Arduino UNO + ADXL345 + GPS + ESP8266 (ESP01) -> ThingSpeak
//
//  MODES (board SW1 = 3-axis switch, board SW2 = GPS switch)
//    00  OFF                       - idle, nothing runs, no uploads
//    01  GPS sensor                - satellite position, upload lat/lon
//    10  Move-away detection       - arm alarm, upload movement flag
//    11  Location via WiFi module  - TO BE DONE
//
//  REMOTE ALERT (field4)
//    After every upload, field4 is read back from the channel.
//    If the app has written a 1 there, the device writes it back to 0
//    and then sounds the alarm: three bursts of beeping, ~10s total.
//    Everything else is paused while it sounds - that is deliberate.
//
//  THINGSPEAK CHANNEL FIELDS
//    field1 = Sudden Movement?   0 = stable, 1 = moved
//    field2 = Latitude           0.000000 until the GPS gets a fix
//    field3 = Longitude          0.000000 until the GPS gets a fix
//    field4 = Find Me            app writes 1, device beeps and clears it
// ==========================================================

//libs
#include <SoftwareSerial.h>
#include <Wire.h>
#include <TinyGPS++.h>

TinyGPSPlus gps;


// ==========================================================
// ============  EDIT YOUR SETTINGS HERE  ===================
// ==========================================================

// --- WiFi network ---
#define WIFI_SSID   "Flippty floppity fosh"
#define WIFI_PASS   "patrickthestarfish"

// --- ThingSpeak ---
#define TS_API_KEY    "AXC34E1LMIQAR55C"    // WRITE API Key
#define TS_CHANNEL_ID "3429448"             // Actuator Control channel
#define TS_READ_KEY   "JFRLRV0NU31L0LQK"    // READ API Key (channel is public,
                                            // so this is optional)
#define TS_HOST       "api.thingspeak.com"
#define TS_PORT       "80"
#define TS_PATH       "/update"

// --- How often to upload ---
// ThingSpeak FREE tier rejects updates sent less than 15s apart.
const unsigned long SEND_INTERVAL = 20000;

// --- Alert sound shape ---
const int  ALERT_BURSTS     = 3;      // how many bursts
const long ALERT_BURST_MS   = 3000;   // length of each burst
const long ALERT_GAP_MS     = 400;    // silence between bursts
const int  ALERT_PULSE_MS   = 150;    // on/off pulse inside a burst

// --- How often to print the "no GPS fix" message ---
const unsigned long GPS_MSG_INTERVAL = 5000;

// --- How often to print the x/y/z readings ---
const unsigned long ACCEL_MSG_INTERVAL = 2000;

// --- How long the switches must sit still before a mode change counts ---
const unsigned long MODE_SETTLE_TIME = 250;

// --- Movement threshold, in raw ADXL345 counts (~125 counts = 1g) ---
const int ALARM_THRESHOLD = 100;

// ==========================================================
// ==========================================================


//mode values
#define MODE_OFF   0   // 00
#define MODE_GPS   1   // 01
#define MODE_MOVE  2   // 10
#define MODE_WIFI  3   // 11

//defs
#define threeaxisSDA A4 //SDA  (fixed by Wire library on UNO)
#define threeaxisSCL A5 //SCL  (fixed by Wire library on UNO)
#define SD_MISO 12
#define SD_MOSI 11
#define SD_SCK  13
#define SD_CS   10
#define Buzzer  9
#define LED     8
#define DIP1    6
#define DIP2    7
#define GPS_TX  5
#define GPS_RX  4
#define Wifi_TX 3
#define Wifi_RX 2
SoftwareSerial GPS_Serial(GPS_RX, GPS_TX);
SoftwareSerial WiFi_Serial(Wifi_RX, Wifi_TX);
#define DEBUG true

//3axis
#define DEVICE (0x53) // Device address as specified in data sheet
byte _buff[6];

char POWER_CTL   = 0x2D;  //Power Control Register
char DATA_FORMAT = 0x31;
char DATAX0 = 0x32; //X-Axis Data 0
char DATAX1 = 0x33; //X-Axis Data 1
char DATAY0 = 0x34; //Y-Axis Data 0
char DATAY1 = 0x35; //Y-Axis Data 1
char DATAZ0 = 0x36; //Z-Axis Data 0
char DATAZ1 = 0x37; //Z-Axis Data 1

float latitude = 0, longitude = 0;
int x, y, z;
int xref, yref, zref;
bool ref = false;

// LATCHED movement flag.
bool movedFlag = false;

// Remote alert bookkeeping.
// alertPending stays true if we sounded the alarm but could not clear
// field4 (usually the ThingSpeak rate limit). We keep retrying the
// clear, and we do NOT beep again for the same trigger.
bool alertPending = false;

//timers (non-blocking)
unsigned long lastSend      = 0;
unsigned long lastGpsMsg    = 0;
unsigned long lastAccelMsg  = 0;
unsigned long alarmToggle   = 0;
bool alarmOn = false;

//mode tracking
int  currentMode = -1;          // the mode we are actually running
int  pendingMode = -1;          // what the switches currently say
unsigned long modeChangedAt = 0;

// ----------------------------------------------------------
void setup() {
  // pinModes
  pinMode(LED, OUTPUT);
  pinMode(Buzzer, OUTPUT);
  pinMode(DIP1, INPUT);
  pinMode(DIP2, INPUT);

  //Serial monitor
  Serial.begin(9600);
  Serial.println(F("Starting..."));

  GPS_Serial.begin(9600);   // GPS init
  WiFi_Serial.begin(9600);  // WiFi init

  //3-Axis
  Wire.begin();
  Serial.println(F("init ADXL345"));
  writeTo(DATA_FORMAT, 0x01);   // +/- 4G range
  writeTo(POWER_CTL, 0x08);     // measurement mode

  // ---- ESP8266 bring-up ----
  WiFi_Serial.listen();

  sendData(F("AT+RST\r\n"), 8000, DEBUG);
  delay(2000);                                   // let the ESP finish booting
  sendData(F("AT\r\n"), 2000, DEBUG);            // sanity check - expect OK
  sendData(F("AT+CWMODE=1\r\n"), 3000, DEBUG);

  Serial.println(F("Joining WiFi, please wait..."));
  String r = sendData("AT+CWJAP=\"" WIFI_SSID "\",\"" WIFI_PASS "\"\r\n", 30000, DEBUG);
  if (r.indexOf("FAIL") != -1 || r.indexOf("ERROR") != -1) {
    Serial.println(F("!! WiFi join FAILED - check SSID/password"));
  }

  sendData(F("AT+CIPMUX=0\r\n"), 3000, DEBUG);
  sendData(F("AT+CIFSR\r\n"), 3000, DEBUG);      // show our IP address

  Serial.print(F("Free RAM: "));
  Serial.println(freeRam());

  GPS_Serial.listen();                           // hand the ear back to the GPS
}

// ----------------------------------------------------------
void loop() {

  // ---- READ THE SWITCHES AND BUILD THE 2-BIT MODE ----
  // board SW1 (DIP2) is the high bit, board SW2 (DIP1) is the low bit
  int bitHigh = (digitalRead(DIP2) == HIGH) ? 1 : 0;
  int bitLow  = (digitalRead(DIP1) == HIGH) ? 1 : 0;
  int readMode = (bitHigh << 1) | bitLow;

  // ---- DEBOUNCE / SETTLE ----
  if (readMode != pendingMode) {
    pendingMode   = readMode;
    modeChangedAt = millis();
  }
  if (pendingMode != currentMode &&
      millis() - modeChangedAt > MODE_SETTLE_TIME) {
    currentMode = pendingMode;
    announceMode(currentMode);
    enterMode(currentMode);
  }

  // ---- RUN THE ACTIVE MODE ----
  switch (currentMode) {

    case MODE_OFF:
      // nothing runs, nothing uploads
      break;

    case MODE_GPS:
      ReadGPS();
      if (millis() - lastSend > SEND_INTERVAL) {
        lastSend = millis();
        Wifi(false, true);          // upload lat/lon only
        checkRemoteAlert();         // then see if the app rang us
      }
      break;

    case MODE_MOVE:
      runMoveDetection();
      if (millis() - lastSend > SEND_INTERVAL) {
        lastSend = millis();
        Wifi(true, false);          // upload movement flag only
        checkRemoteAlert();         // then see if the app rang us
      }
      break;

    case MODE_WIFI:
      // TO BE DONE - location from nearby WiFi routers
      break;
  }
}


// ==========================================================
//  REMOTE ALERT - "find my bag" from the app
//
//  Runs straight after every upload. Reads field4 back off the
//  channel; if it is 1, clears it and sounds the alarm.
// ==========================================================
void checkRemoteAlert() {

  // Still owe the server a clear from last time? Try again first.
  if (alertPending) {
    if (clearAlertField()) {
      alertPending = false;
      Serial.println(F("field4 cleared (retry)"));
    }
    return;                          // do not re-beep for the same trigger
  }

  int v = readAlertField();

  if (v == 1) {
    Serial.println(F("!! REMOTE ALERT from app"));

    // Write it back to 0 first, as asked. This often fails because
    // it lands inside ThingSpeak's ~15s minimum gap between writes -
    // hence alertPending, which retries next cycle.
    if (clearAlertField()) {
      Serial.println(F("field4 cleared"));
    } else {
      alertPending = true;
      Serial.println(F("Could not clear field4 - will retry"));
    }

    soundAlert();
  }
  else if (v == 0) {
    // normal - nothing to do
  }
  else {
    Serial.println(F("Could not read field4"));
  }
}

// Reads /channels/<id>/fields/4/last.txt - the reply body is just
// the bare value, so there is almost nothing to parse.
// Returns 0, 1, or -1 if it could not be read.
int readAlertField() {
  WiFi_Serial.listen();
  sendData(F("AT+CIPCLOSE\r\n"), 2000, false);

  String r = sendData("AT+CIPSTART=\"TCP\",\"" TS_HOST "\"," TS_PORT "\r\n", 8000, false);
  if (r.indexOf("CONNECT") == -1 && r.indexOf("ALREADY") == -1) {
    GPS_Serial.listen();
    return -1;
  }

  String get = F("GET /channels/" TS_CHANNEL_ID "/fields/4/last.txt");
  if (strlen(TS_READ_KEY) > 0) {
    get += F("?api_key=" TS_READ_KEY);
  }
  get += F("\r\n\r\n");

  WiFi_Serial.print(F("AT+CIPSEND="));
  WiFi_Serial.println(get.length());

  if (!waitForPrompt(5000)) {
    sendData(F("AT+CIPCLOSE\r\n"), 2000, false);
    GPS_Serial.listen();
    return -1;
  }

  WiFi_Serial.print(get);

  // The ESP wraps the reply as  +IPD,<len>:<data>
  // Scan for that marker and read the payload, rather than buffering
  // the whole HTTP response.
  int  result = -1;
  char tail[8] = {0};
  bool inPayload = false;
  int  payLen = 0, payGot = 0;
  bool readingLen = false;

  unsigned long start = millis();
  while (millis() - start < 10000) {
    while (WiFi_Serial.available()) {
      char c = WiFi_Serial.read();

      if (!inPayload) {
        // slide a 5-char window along looking for "+IPD,"
        for (int i = 0; i < 4; i++) tail[i] = tail[i + 1];
        tail[4] = c;
        tail[5] = '\0';
        if (strcmp(tail, "+IPD,") == 0) {
          readingLen = true;
          payLen = 0;
          continue;
        }
        if (readingLen) {
          if (c >= '0' && c <= '9') {
            payLen = payLen * 10 + (c - '0');
          } else if (c == ':') {
            readingLen = false;
            inPayload  = true;
            payGot     = 0;
          }
        }
      } else {
        payGot++;
        // the body is the very last thing in the packet; keep the
        // last digit we see
        if (c == '0') result = 0;
        if (c == '1') result = 1;
        if (payGot >= payLen) {
          start = 0;                    // force the outer loop to end
          break;
        }
      }
    }
    if (start == 0) break;
  }

  sendData(F("AT+CIPCLOSE\r\n"), 2000, false);
  GPS_Serial.listen();

  Serial.print(F("field4 = "));
  Serial.println(result);
  return result;
}

// Writes field4=0. Returns true only if ThingSpeak accepted it.
bool clearAlertField() {
  WiFi_Serial.listen();
  sendData(F("AT+CIPCLOSE\r\n"), 2000, false);

  String r = sendData("AT+CIPSTART=\"TCP\",\"" TS_HOST "\"," TS_PORT "\r\n", 8000, false);
  if (r.indexOf("CONNECT") == -1 && r.indexOf("ALREADY") == -1) {
    GPS_Serial.listen();
    return false;
  }

  String get = F("GET " TS_PATH "?api_key=" TS_API_KEY "&field4=0\r\n\r\n");

  WiFi_Serial.print(F("AT+CIPSEND="));
  WiFi_Serial.println(get.length());

  bool ok = false;
  if (waitForPrompt(5000)) {
    String resp = sendData(get, 6000, false);
    // ThingSpeak answers with the new entry ID. A bare "0" means it
    // rejected the write - almost always the rate limit.
    ok = (resp.indexOf("SEND OK") != -1);
  }

  sendData(F("AT+CIPCLOSE\r\n"), 2000, false);
  GPS_Serial.listen();
  return ok;
}

// Three bursts of beeping, about 10 seconds in total.
// Deliberately blocking - the alarm is meant to interrupt everything.
void soundAlert() {
  Serial.println(F("*** SOUNDING ALERT ***"));

  for (int b = 0; b < ALERT_BURSTS; b++) {
    unsigned long burstStart = millis();
    while (millis() - burstStart < (unsigned long)ALERT_BURST_MS) {
      digitalWrite(Buzzer, HIGH);
      digitalWrite(LED, HIGH);
      delay(ALERT_PULSE_MS);
      digitalWrite(Buzzer, LOW);
      digitalWrite(LED, LOW);
      delay(ALERT_PULSE_MS);
    }
    delay(ALERT_GAP_MS);
  }

  digitalWrite(Buzzer, LOW);
  digitalWrite(LED, LOW);
  Serial.println(F("*** ALERT DONE ***"));

  // The alert ate about 10 seconds. Push the next upload out so we
  // do not immediately trip the rate limit.
  lastSend = millis();
}


// ==========================================================
//  MODE HANDLING
// ==========================================================
void announceMode(int m) {
  Serial.println();
  Serial.print(F(">>> MODE "));
  switch (m) {
    case MODE_OFF:  Serial.println(F("00 - OFF"));                    break;
    case MODE_GPS:  Serial.println(F("01 - GPS SENSOR"));             break;
    case MODE_MOVE: Serial.println(F("10 - MOVE AWAY DETECTION"));    break;
    //case MODE_WIFI: Serial.println(F("11 - WIFI LOCATION (to be done)")); break;
  }
}

// Runs once when a mode is entered - clean up whatever the last mode left on
void enterMode(int m) {
  digitalWrite(LED, LOW);
  digitalWrite(Buzzer, LOW);
  alarmOn   = false;
  ref       = false;      // next entry into MOVE mode re-captures reference
  movedFlag = false;      // start each arming session clean

  if (m == MODE_GPS) {
    GPS_Serial.listen();
  }
}

// ---- 10: move-away detection ----
void runMoveDetection() {
  readAccel();

  // capture the reference position the first time round
  if (ref == false) {
    xref = x;  yref = y;  zref = z;
    ref = true;
    Serial.println(F("ARMED - reference captured"));
    return;
  }

  if (abs(x - xref) > ALARM_THRESHOLD ||
      abs(y - yref) > ALARM_THRESHOLD ||
      abs(z - zref) > ALARM_THRESHOLD) {

    if (!movedFlag) {
      movedFlag = true;                       // latch it for the next upload
      Serial.println(F("!! MOVEMENT DETECTED"));
    }

    // non-blocking beep
    if (millis() - alarmToggle > 500) {
      alarmToggle = millis();
      alarmOn = !alarmOn;
      digitalWrite(LED,    alarmOn ? HIGH : LOW);
      digitalWrite(Buzzer, alarmOn ? HIGH : LOW);
    }
  } else {
    digitalWrite(LED, LOW);
    digitalWrite(Buzzer, LOW);
    alarmOn = false;
  }
}


// ==========================================================
//  ADXL345
// ==========================================================
void readAccel() {
  uint8_t howManyBytesToRead = 6;
  readFrom(DATAX0, howManyBytesToRead, _buff);

  // each axis reading comes in 10 bit resolution, ie 2 bytes, LSB first
  x = (((int)_buff[1]) << 8) | _buff[0];
  y = (((int)_buff[3]) << 8) | _buff[2];
  z = (((int)_buff[5]) << 8) | _buff[4];

  // throttled so it does not bury the mode messages
  if (millis() - lastAccelMsg > ACCEL_MSG_INTERVAL) {
    lastAccelMsg = millis();
    Serial.print(F("x: "));  Serial.print(x);
    Serial.print(F(" y: ")); Serial.print(y);
    Serial.print(F(" z: ")); Serial.println(z);
  }
}

void writeTo(byte address, byte val) {
  Wire.beginTransmission(DEVICE);
  Wire.write(address);
  Wire.write(val);
  Wire.endTransmission();
}

// Reads num bytes starting from address register on device in to _buff array
void readFrom(byte address, int num, byte _buff[]) {
  Wire.beginTransmission(DEVICE);
  Wire.write(address);
  Wire.endTransmission();

  Wire.beginTransmission(DEVICE);
  Wire.requestFrom(DEVICE, num);

  int i = 0;
  while (Wire.available() && i < num) {
    _buff[i] = Wire.read();
    i++;
  }
  Wire.endTransmission();
}


// ==========================================================
//  GPS
// ==========================================================
void ReadGPS() {
  GPS_Serial.listen();

  while (GPS_Serial.available()) {
    char c = GPS_Serial.read();
    gps.encode(c);

    if (gps.location.isUpdated()) {
      latitude  = gps.location.lat();
      longitude = gps.location.lng();

      Serial.print(F("Latitude: "));
      Serial.println(latitude, 6);
      Serial.print(F("Longitude: "));
      Serial.println(longitude, 6);
    }
  }

  // Status message, throttled - NO delay() here, it would overflow
  // the SoftwareSerial buffer and destroy every NMEA sentence.
  if (millis() - lastGpsMsg > GPS_MSG_INTERVAL) {  
    lastGpsMsg = millis();
    if (gps.location.isValid()) {
      Serial.println(F("GPS FIX!"));
    } else {
      Serial.print(F("No fix yet. Chars: "));
      Serial.print(gps.charsProcessed());
      Serial.print(F("  Sats: "));
      Serial.println(gps.satellites.value());
    }
  }
}


// ==========================================================
//  WiFi -> ThingSpeak
//    sendMoved = true -> field1 gets the latched movement flag
//    sendGps   = true -> field2/3 get lat/lon
// ==========================================================
void Wifi(bool sendMoved, bool sendGps) {
  WiFi_Serial.listen();

  // Make sure no stale socket is open
  sendData(F("AT+CIPCLOSE\r\n"), 2000, false);

  // ---- Open TCP connection, and CHECK it worked ----
  String r = sendData("AT+CIPSTART=\"TCP\",\"" TS_HOST "\"," TS_PORT "\r\n", 8000, DEBUG);

  if (r.indexOf("busy") != -1) {
    Serial.println(F("ESP busy - skipping this round"));
    GPS_Serial.listen();
    return;                                   // flag stays latched, retry next time
  }
  if (r.indexOf("CONNECT") == -1 && r.indexOf("ALREADY") == -1) {
    Serial.println(F("TCP connect FAILED"));
    sendData(F("AT+CIPCLOSE\r\n"), 2000, false);
    GPS_Serial.listen();
    return;                                   // flag stays latched, retry next time
  }

  // ---- Build the HTTP GET request ----
  // field1 = Sudden Movement?, field2 = Latitude, field3 = Longitude
  String getStr = F("GET " TS_PATH "?api_key=" TS_API_KEY);

  if (sendMoved) {
    getStr += F("&field1=");
    getStr += movedFlag ? 1 : 0;
  }

  // Always sent, even with no fix - they read 0.000000 until the
  // first lock, then hold the last known position.
  if (sendGps) {
    getStr += F("&field2=");  getStr += String(latitude, 6);
    getStr += F("&field3=");  getStr += String(longitude, 6);
  }

  getStr += F("\r\n\r\n");   // terminator MUST be last

  Serial.print(F("Sending: "));
  Serial.println(getStr);

  // ---- Tell ESP how many bytes are coming ----
  WiFi_Serial.print(F("AT+CIPSEND="));
  WiFi_Serial.println(getStr.length());

  // ---- Wait for the ">" prompt ----
  if (waitForPrompt(5000)) {
    String resp = sendData(getStr, 6000, DEBUG);
    if (resp.indexOf("SEND OK") != -1) {
      Serial.println(F("--- Upload OK ---"));
      // Only clear the latch once the data has actually gone out,
      // otherwise a failed upload would silently lose the event.
      if (sendMoved) movedFlag = false;
    } else {
      Serial.println(F("--- Upload may have failed ---"));
    }
  } else {
    Serial.println(F("CIPSEND Failed (no '>' prompt)"));
  }

  sendData(F("AT+CIPCLOSE\r\n"), 3000, DEBUG);
  GPS_Serial.listen();   // give the ear back to the GPS
}

// Waits for the ESP's ">" prompt without blocking for a fixed time
bool waitForPrompt(int timeout) {
  long start = millis();
  while (millis() - start < timeout) {
    while (WiFi_Serial.available()) {
      if (WiFi_Serial.read() == '>') return true;
    }
  }
  return false;
}


// ==========================================================
//  sendData
//
//  Only stops early when the reply ENDS with a final status line.
//  Matching "OK" anywhere in the buffer used to fire the next
//  command while the ESP was still working - that produced "busy p...".
// ==========================================================
bool replyFinished(String &r) {
  return r.endsWith("OK\r\n")        ||   // covers OK and SEND OK
         r.endsWith("ERROR\r\n")     ||
         r.endsWith("FAIL\r\n")      ||
         r.endsWith("CLOSED\r\n")    ||
         r.endsWith("ready\r\n")     ||   // after AT+RST
         r.endsWith("SEND FAIL\r\n") ||
         r.indexOf("busy p") != -1;
}

String sendData(String command, const int timeout, boolean debug) {
  while (WiFi_Serial.available()) WiFi_Serial.read();   // drop stale bytes

  String response = "";
  WiFi_Serial.print(command);

  unsigned long startTime = millis();
  while ((millis() - startTime) < (unsigned long)timeout) {
    while (WiFi_Serial.available()) {
      response += (char)WiFi_Serial.read();
    }
    if (replyFinished(response)) break;
  }

  if (debug) Serial.println(response);

  delay(150);          // small gap so the ESP is ready for the next command
  return response;
}

// Overload so F() strings can be passed in too
String sendData(const __FlashStringHelper *command, const int timeout, boolean debug) {
  return sendData(String(command), timeout, debug);
}

int freeRam() {
  extern int __heap_start, *__brkval;
  int v;
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval);
}