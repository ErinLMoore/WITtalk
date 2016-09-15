/*

 Udp NTP Client

 Get the time from a Network Time Protocol (NTP) time server
 Demonstrates use of UDP sendPacket and ReceivePacket
 For more on NTP time servers and the messages needed to communicate with them,
 see http://en.wikipedia.org/wiki/Network_Time_Protocol

 created 4 Sep 2010
 by Michael Margolis
 modified 9 Apr 2012
 by Tom Igoe

 This code is in the public domain.
https://gusclass.com/blog/2014/10/30/programming-an-adafruit-led-matrix/
http://www.instructables.com/id/Arduino-Binary-clock-using-LED-Matrix/?ALLSTEPS
 */
 /*
 * Time_NTP.pde
 * Example showing time sync to NTP time source
 *
 * This sketch uses the Ethernet library
 */

#include<TimeLib.h>

#include <Adafruit_NeoPixel.h>
#define PIN 6
#define ROW_SIZE 8
Adafruit_NeoPixel strip = Adafruit_NeoPixel(32, PIN, NEO_GRB + NEO_KHZ800);
int brightness = 10;
uint32_t row_1_color = strip.Color(0,255,137);
uint32_t row_2_color = strip.Color(87,0,255);
uint32_t row_3_color = strip.Color(255,137,0);
uint32_t row_4_color = strip.Color(0,0,0);
uint32_t error_color_on = strip.Color(255,0,50);
uint32_t error_color_off = strip.Color(0,0,0);

#include <SPI.h>
#include <Adafruit_WINC1500.h>
#include <Adafruit_WINC1500Udp.h>

// Define the WINC1500 board connections below.
// If you're following the Adafruit WINC1500 board
// guide you don't need to modify these:
#define WINC_CS   8
#define WINC_IRQ  7
#define WINC_RST  4
#define WINC_EN   2     // or, tie EN to VCC and comment this out
// The SPI pins of the WINC1500 (SCK, MOSI, MISO) should be
// connected to the hardware SPI port of the Arduino.
// On an Uno or compatible these are SCK = #13, MISO = #12, MOSI = #11.
// On an Arduino Zero use the 6-pin ICSP header, see:
//   https://www.arduino.cc/en/Reference/SPI



// Setup the WINC1500 connection with the pins above and the default hardware SPI.
Adafruit_WINC1500 WiFi(WINC_CS, WINC_IRQ, WINC_RST);
char ssid[] = "NexusSeriesReplicant";  //  your network SSID (name)
char pass[] = "tearsintherain";       // your network password
int keyIndex = 0;            // your network key Index number (needed only for WEP)
unsigned int localPort = 2390;      // local port to listen for UDP packets
// A UDP instance to let us send and receive packets over UDP
Adafruit_WINC1500UDP Udp;
int status = WL_IDLE_STATUS;
Adafruit_WINC1500Client client;
////////////////////////////////////////////////////////////////////////////
IPAddress timeServer(129, 6, 15, 28); // time.nist.gov NTP server
const int NTP_PACKET_SIZE = 48; // NTP time stamp is in the first 48 bytes of the message
byte packetBuffer[ NTP_PACKET_SIZE]; //buffer to hold incoming and outgoing packets
//const int timeZone = -5;  // Eastern Standard Time (USA)
const int timeZone = -4;  // Eastern Daylight Time (USA)
////////////////////////////////////////////////////////////////////////////////////
//IPAddress server(141,101,112,175);  // numeric IP for test page (no DNS)
char weatherServer[] = "api.wunderground.com";   // domain name for test page (using DNS)
const String myKey = "473d7d4708a1fabf";  //See: http://www.wunderground.com/weather/api/d/docs (change here with your KEY)
const String myFeatures = "conditions";   //See: http://www.wunderground.com/weather/api/d/docs?d=data/index&MR=1
const String myCountry = "OH";        //See: http://www.wunderground.com/weather/api/d/docs?d=resources/country-to-iso-matching
const String myCity = "Columbus"; //See: http://www.wunderground.com/weather/api/d/docs?d=data/index&MR=1
String responseString;
boolean startCapture;
unsigned long lastConnectionTime = 0;            // last time you connected to the server, in milliseconds
const unsigned long postingInterval = (10L * 1000L) * 12; // delay between updates, in milliseconds
int tempf = 0;
//////////////////////////////////////////////////////////////////////////////////////////////////
void setup()
{
#ifdef WINC_EN
  pinMode(WINC_EN, OUTPUT);
  digitalWrite(WINC_EN, HIGH);
#endif

  strip.begin();
  strip.show(); // Initialize all pixels to 'off'

  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  
  // attempt to connect to Wifi network:
  while ( status != WL_CONNECTED) {
    status = WiFi.begin(ssid, pass);
    delay(10000);
  } 
  //printWifiStatus();

  Udp.begin(localPort);
  setSyncProvider(getNtpTime);
}

time_t prevDisplay = 0; // when the digital clock was displayed

byte display_mask[][ROW_SIZE] = {
    {1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1},    
    {1,1,1,1,1,1,1,1},
    {1,1,1,1,1,1,1,1},   
};
////////////////////////////////////////////////////////////////////////////////////
//The Loop!
////////////////////////////////////////////////////////////////////////////////
void loop()
{
  if (timeStatus() != timeNotSet) {
    if (now() != prevDisplay) { //update the display only if time has changed
      prevDisplay = now();
      updateDisplayMask();
      maskedColorWipe(display_mask);
      httpWeatherRequest();
      }
    }
    else{
      flashIfNoConnection();
    }
  while (client.available()) {
    char c = client.read();
    if(c == '{')
      startCapture=true; 
    if(startCapture)
      responseString += c;
  }

  // if ten seconds have passed since your last connection,
  // then connect again and send data:
  if (millis() - lastConnectionTime > postingInterval) {
    httpWeatherRequest();
    Serial.println(tempf);
  }
}

////////////////////////////////////////////////////////////////////////////////////////
//Update Display Functions
//////////////////////////////////////
void updateDisplayMask(){
  byte secondbyte = second();   
  byte minutebyte = minute(); 
  byte hourbyte = hour(); 
  byte tempbyte = tempf;
  
  for (byte i=0; i<ROW_SIZE; i++) {
    display_mask[0][i] = bitRead(hourbyte, i);
  }

  for (byte i=0; i<ROW_SIZE; i++) {
    display_mask[1][i] = bitRead(minutebyte, i);
  }
  
  for (byte i=0; i<ROW_SIZE; i++) {
    display_mask[2][i] = bitRead(secondbyte, i);
  }
  for (byte i=0; i<ROW_SIZE; i++) {
    display_mask[3][i] = bitRead(tempbyte, i);
  }
}

void flashIfNoConnection(){
      ColorWipePlain(error_color_on, display_mask);
      delay(500);
      ColorWipePlain(error_color_off, display_mask);
      delay(500);
}
 
///////////////////////////////////////////////////
//Neopixel Functions
////////////////////////////////////////////////
void maskedColorWipe(byte mask[][ROW_SIZE]) {
  uint16_t current_pixel;

  for(current_pixel=0; current_pixel<strip.numPixels(); current_pixel++) {
    if (drawGivenMask(current_pixel / ROW_SIZE, current_pixel % ROW_SIZE, mask)){
      strip.setBrightness(brightness);
      if (current_pixel<8){
      strip.setPixelColor(current_pixel, row_1_color);}
      if (current_pixel>8 && current_pixel<16){
        strip.setPixelColor(current_pixel, row_2_color);}
     if (current_pixel>16 && current_pixel<24){
        strip.setPixelColor(current_pixel, row_3_color);}
     if (current_pixel>24){
        strip.setPixelColor(current_pixel, row_4_color);}
    }else{
      strip.setPixelColor(current_pixel, 0);
    }
  }
   
  strip.show();
}

void ColorWipePlain(uint32_t color, byte mask[][ROW_SIZE]) {
  uint16_t current_pixel;
  for(current_pixel=0; current_pixel<strip.numPixels(); current_pixel++) {
    if (drawGivenMask(current_pixel / ROW_SIZE, current_pixel % ROW_SIZE, mask)){
      strip.setBrightness(brightness);
      strip.setPixelColor(current_pixel, color);
    }else{
      strip.setPixelColor(current_pixel, 0);
    }
  }
  strip.show();
}

boolean drawGivenMask(int row, int col, byte mask[][ROW_SIZE]){
  col = col % ROW_SIZE;
   
  if (mask[row][col] & 1){
    return true;
  }
  return false;
}
////////////////////////////////////////////////////////////////////////
//NTP Time Functions
///////////////////////////////////////////////////////////////////////
time_t getNtpTime(){
  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  sendNTPpacket(timeServer);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}


unsigned long sendNTPpacket(IPAddress& address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;

  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
/////////////////////////////////////////////////////////////////////////////////////////////////
//Weather HTTP Request
///////////////////////////////////////////////////////////
void httpWeatherRequest() {
  //Response from Server
  // close any connection before send a new request.
  // This will free the socket on the WiFi shield
  client.stop();
      
  // if there's a successful connection:
  if (client.connect(weatherServer, 80)) {
    Serial.println("connecting...");
    // Make a HTTP request:
    String html_cmd1 = "GET /api/" + myKey + "/" + myFeatures + "/q/" + myCountry + "/" + myCity + ".json HTTP/1.1";
    String html_cmd2 = "Host: " + (String)weatherServer;
    String html_cmd3 = "Connection: close";
    
    // Make a HTTP request:
    client.println(html_cmd1);
    client.println(html_cmd2);
    client.println(html_cmd3);
    client.println();
    tempf = (getValuesFromKey(responseString, "temp_f")).toInt();
    int green;
    if (tempf >= 0) {
      green = 0;
    }
    else {green = 100;}
    row_4_color = strip.Color(abs(tempf*2),green,255 - abs(tempf*2));
     responseString = "";
     startCapture = false; 
    // note the time that the connection was made:
    lastConnectionTime = millis();
  }
  else {
    // if you couldn't make a connection:
    Serial.println("connection failed");
  }
}

////////////////////////////////////////////////
//Weather Response Parser
///////////////////////////////////////////////
String getValuesFromKey(const String response, const String sKey)
{ 
  String sKey_ = sKey;  
  sKey_ = "\"" + sKey + "\":";  
  char key[sKey_.length()];  
  sKey_.toCharArray(key, sizeof(key));  
  int keySize = sizeof(key)-1;    
  String result = "";
  int n = response.length();  
  for(int i=0; i < (n-keySize-1); i++)
  {
    char c[keySize];    
    for(int k=0; k<keySize; k++)
    {
      c[k] = response.charAt(i+k);
    }        
    boolean isEqual = true;    
    for(int k=0; k<keySize; k++)
    {
      if(!(c[k] == key[k]))
      {
        isEqual = false;
        break;
      }
    }    
    if(isEqual)
    {     
      int j= i + keySize + 1;
      while(!(response.charAt(j) == ','))
      {
        result += response.charAt(j);        
        j++;
      }     
      //Remove char '"'
      result.replace("\"","");
      break;
    }
  } 
  return result;
}
////////////////////////////////////////////////
//Wifi Debugging Function
///////////////////////////////////////////

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

