/*
  CoAP emoncms client
 
 This sketch connects to an emoncms server and sends sensor readings
 using CoAP protocol.  For more information on CoAP, please have a 
 look at http://en.wikipedia.org/wiki/Constrained_Application_Protocol

 
 Circuit:
 * Ethernet shield attached to pins 10, 11, 12, 13
 * DHT22 temperature and humidity sensor attached to port A0
 * Light sensor attached to port A1
 
 created 1 Mar 2013
 by Tom Igoe
 modified 3 Apr 2013
 by Baptiste Gaultier
 
 This code is in the public domain.

 */

#include <SPI.h>         
#include <Ethernet.h>
#include <EthernetUdp.h>
#include <DHT.h>

// These constants won't change
const int DHT22SensorPin = A0;
const int lightSensorPin = A1;
const int threshold = 600;

DHT dht(DHT22SensorPin, DHT22);

// humidity and temperature
float temperature = 0, humidity = 0;

// the follow variables are long because the time, measured in miliseconds,
// will quickly become a bigger number than can be stored in an int.
long pulseCount = 0;   

// used to measure power
unsigned long pulseTime,lastTime;

// power and energy
float power = 0;

// number of readings we made since the last packet sent :
byte readings = 0;          

// Enter a MAC address for your controller below.
// Newer Ethernet shields have a MAC address printed on a sticker on the shield
byte mac[] = {  
  0x90, 0xA2, 0xAD, 0x0D, 0x6F, 0xC0 };

// fill in an available IP address on your network here,
// for auto configuration:
IPAddress ip(169, 254, 0, 64);

IPAddress coapServer(192, 108, 119, 4);            // adress of your favorite emoncms CoAP server
const unsigned int coapPort = 5683;                // CoAP packets are sent 5683 by default
const unsigned int localPort = 56552;              // local port to listen for UDP packets

unsigned long lastConnectionTime = 0;              // last time you connected to the server, in milliseconds
const unsigned long postingInterval = 4*1000;      // delay between updates, in milliseconds

const int COAP_PACKET_SIZE = 64;                   // CoAP packet should be less than 64 bytes in this example
byte packetBuffer[ COAP_PACKET_SIZE ];             // Buffer to hold outgoing packets
byte packetReceivedBuffer[UDP_TX_PACKET_MAX_SIZE]; // Buffer to hold incoming packets
word messageID;

// A UDP instance to let us send and receive packets over UDP
EthernetUDP Udp;

void setup() {
  // Open serial communications and wait for port to open:
  Serial.begin(9600);
  dht.begin();
  
  // Display a welcome message
  Serial.println("CoAP emoncms client v0.1 starting...");
  
  // attempt a DHCP connection:
  Serial.println("Attempting to get an IP address using DHCP:");
  if (!Ethernet.begin(mac)) {
    // if DHCP fails, start with a hard-coded address:
    Serial.println("failed to get an IP address using DHCP, trying manually");
    Ethernet.begin(mac, ip);
  }
  Serial.print("My address:");
  Serial.println(Ethernet.localIP());
  
  // needed for Message ID later
  randomSeed(analogRead(0));
  messageID = random(0, 65000);
  Udp.begin(localPort);
}

void loop() {
  // if the meter flash, increment the counter
  if (analogRead(lightSensorPin) > threshold) {
    while (analogRead(lightSensorPin) > threshold) {}
    //used to measure time between pulses.
    lastTime = pulseTime;
    pulseTime = millis();
    
    //pulseCounter
    pulseCount++;
    
    // we don't want to miss a flash during sending
    readings++;
    
    // calculate power
    power = 3600000.0 / (pulseTime - lastTime);
    
    //Print the values
    Serial.print("Power : ");
    Serial.print(power,2);
    Serial.print("W");
  }
  
  /* sendCoAPpacket function arguments :
    * IPAddress& address : IP adress of the CoAP server
    * String uri         : URI you want to access ('/' are not needed)
    * String query       : URI query optionnal, example : ?query=1
    * String query       : URI query optionnal, example : &query=2
    * String payload     : the actual payload
  */
  
  // emoncms API
  /*
    * uri → input name
    * query → apikey (you have to specify your Read & Write API key
    * query → node number (optionnal)
    * payload → sensor reading you want to send
  */
  
  if( ((unsigned long)(millis() - lastConnectionTime) >= postingInterval) && power >= 0 && readings >= 2) {
    temperature = dht.readTemperature();
    humidity = dht.readHumidity();
    char buffer [16];
    dtostrf(power, 16, 2, buffer);
    String powerString = String(buffer);
    sendCoAPpacket(coapServer, "power", "apikey=<your_api_key>", "node=1", powerString);
    dtostrf(temperature, 16, 2, buffer);
    String temperatureString = String(buffer);
    sendCoAPpacket(coapServer, "temperature", "apikey=<your_api_key>", "node=1", temperatureString);
    dtostrf(humidity, 16, 2, buffer);
    String humidityString = String(buffer);
    sendCoAPpacket(coapServer, "humidity", "apikey=<your_api_key>", "node=1", humidityString);
    
    readings = 0;
  } 
  
  // if there's data available, read a packet
  int packetSize = Udp.parsePacket();
  if(packetSize) {
    Serial.print("Received CoAP packet of size ");
    Serial.print(packetSize);
    Serial.print(" from ");
    IPAddress remote = Udp.remoteIP();
    for (int i =0; i < 4; i++) {
      Serial.print(remote[i], DEC);
      if (i < 3)
      {
        Serial.print(".");
      }
    }
    Serial.print(", port ");
    Serial.println(Udp.remotePort());
    // read the packet into packetReceivedBuffer
    Udp.read(packetReceivedBuffer,UDP_TX_PACKET_MAX_SIZE);
    Serial.print("Version : ");
    Serial.print(bitRead(packetReceivedBuffer[0], 7)); 
    Serial.print(bitRead(packetReceivedBuffer[0], 6));
    Serial.print(" (");
    Serial.print(bitRead(packetReceivedBuffer[0], 7) << 1 | bitRead(packetReceivedBuffer[0], 6), DEC);
    Serial.println(")");
    Serial.print("Type : ");
    Serial.print(bitRead(packetReceivedBuffer[0], 5)); 
    Serial.print(bitRead(packetReceivedBuffer[0], 4));
    Serial.print(" (");
    switch(bitRead(packetReceivedBuffer[0], 5) << 1 | bitRead(packetReceivedBuffer[0], 4)) {
      case 0:
        Serial.print("Confirmable");
        break;
      case 1:
        Serial.print("Non-Confirmable");
        break;
      case 2:
        Serial.print("Acknowledgement");
        break;
      case 3:
        Serial.print("Reset");
        break;
      default:
        Serial.print("Unknown");
    }
    Serial.println(")");
    Serial.print("Token Length : ");
    Serial.print(bitRead(packetReceivedBuffer[0], 3)); 
    Serial.print(bitRead(packetReceivedBuffer[0], 2));
    Serial.print(bitRead(packetReceivedBuffer[0], 1)); 
    Serial.print(bitRead(packetReceivedBuffer[0], 0));
    Serial.print(" (");
    Serial.print(bitRead(packetReceivedBuffer[0], 3) << 3 |
                 bitRead(packetReceivedBuffer[0], 2) << 2 |
                 bitRead(packetReceivedBuffer[0], 1) << 1 |
                 bitRead(packetReceivedBuffer[0], 0), DEC);
    Serial.println(")");
    Serial.print("Code : ");
    for (int i = 7; i >= 0; i--) {
      Serial.print(bitRead(packetReceivedBuffer[1], i)); 
    }
    Serial.print(" (");
    switch(packetReceivedBuffer[1])
    {
      case 65:
        Serial.print("2.01 Created");
        break;
      case 66:
        Serial.print("2.02 Deleted");
        break;
      case 67:
        Serial.print("2.03 Valid");
        break;
      case 68:
        Serial.print("2.04 Changed");
        break;
      case 69:
        Serial.print("2.05 Content");
        break;
      case 128:
        Serial.print("4.00 Bad Request");
        break;
      case 129:
        Serial.print("4.01 Unauthorized");
        break;
      case 130:
        Serial.print("4.02 Bad Option");
        break;
      case 131:
        Serial.print("4.03 Forbidden");
        break;
      case 132:
        Serial.print("4.04 Not Found");
        break;
      case 133:
        Serial.print("4.05 Method Not Allowed");
        break;
      case 134:
        Serial.print("4.06 Not Acceptable");
        break;
      case 140:
        Serial.print("4.12 Precondition Failed");
        break;
      case 141:
        Serial.print("4.13 Request Entity Too Large");
        break;
      case 143:
        Serial.print("4.15 Unsupported Content-Format");
        break;
      case 160:
        Serial.print("5.00 Internal Server Error");
        break;
      case 161:
        Serial.print("5.01 Not Implemented");
        break;
      case 162:
        Serial.print("5.02 Bad Gateway");
        break;
      case 163:
        Serial.print("5.03 Service Unavailable");
        break;
      case 164:
        Serial.print("5.04 Gateway Timeout");
        break;
      case 165:
        Serial.print("5.05 Proxying Not Supported");
        break;
      default:
        Serial.print("Unknown ");
        Serial.print(packetReceivedBuffer[1], BIN);
    }
    Serial.println(")");
    Serial.print("Message ID : ");
    Serial.print("(");
    Serial.print(word(packetReceivedBuffer[2], packetReceivedBuffer[3]));
    Serial.println(")");
    Serial.println();
  }
  delay(5000);
}

// send an CoAP request to the time server at the given address 
void sendCoAPpacket(IPAddress& address, String uri, String query,String secondQuery, String payload) {
  // Based on draft-ietf-core-coap-13 :  http://tools.ietf.org/html/draft-ietf-core-coap-13
  memset(packetBuffer, 0, COAP_PACKET_SIZE * sizeof(byte));
  
  // Where we are in the packet
  byte index = 0;
  
  // Message Format → http://tools.ietf.org/html/draft-ietf-core-coap-13#section-3
  
  // Initialize values needed to form CoAP header
  packetBuffer[index++] = 0b01010000;  // Version : 1 (01), Type  : Non-Confirmable (01), Option Count : 0 (0000)
  packetBuffer[index++] = 0b00000011;  // Code : PUT (00000011)

  
  // Increment messageID
  messageID++;
  
  packetBuffer[index++] = highByte(messageID);  // Message ID high byte
  packetBuffer[index++] = lowByte(messageID);   // Message ID low byte
  
  // options
  packetBuffer[index++] = 0xB0 | uri.length(); // Uri-Path : 11 → http://tools.ietf.org/html/draft-ietf-core-coap-13#section-5.10
  
  
  byte i;
  for (i = 0; i < uri.length(); i++) {
    packetBuffer[i + index] = uri.charAt(i);
  }
  index += i;
  
  if(query.length() <= 12) 
    packetBuffer[index++] = 0x40 | query.length();  // Uri-Query : 15
  else {
    // lenght is greater than 12
    packetBuffer[index++] = 0x4D;                   // Uri-Query : 15, Lenght : 13( An 8-bit unsigned integer precedes 
    packetBuffer[index++] = query.length() - 13;    // the Option Value and indicates the Option Length minus 13.
  }
  
  // uri-query
  for (i = 0; i < query.length(); i++) {
    packetBuffer[i + index] = query.charAt(i);
  }
  index += i;
  
  if(secondQuery.length() <= 12) 
    packetBuffer[index++] = 0x00 | secondQuery.length();  // Uri-Query : 15
  else {
    // lenght is greater than 12
    packetBuffer[index++] = 0x0D;                         // Uri-Query : 15, Lenght : 13 A 8-bit unsigned integer precedes 
    packetBuffer[index++] = secondQuery.length() - 13;    // the Option Value and indicates the Option Length minus 13.
  }
  
  // uri-query
  for (i = 0; i < secondQuery.length(); i++) {
    packetBuffer[i + index] = secondQuery.charAt(i);
  }
  index += i;
  
  packetBuffer[index++] = 0xFF;  // Payload Marker (0xFF)
  
  //payload
  for (i = 0; i < payload.length(); i++) {
    packetBuffer[i + index] = payload.charAt(i);
  }
  index += i;

  // all CoAP fields have been given values, now
  // you can send a packet requesting a PUT request
  Udp.beginPacket(address, coapPort); //CoAP requests are to port 5683 by default
  Serial.print("Sending CoAP request packet of size ");
  Serial.print(Udp.write(packetBuffer, index));
  Serial.print(" to ");
  for (int i =0; i < 4; i++) {
    Serial.print(address[i], DEC);
    if (i < 3)
      Serial.print(".");
  }
  Serial.println();  
  Udp.endPacket();
  
  // note the time that the connection was made:
  lastConnectionTime = millis();
}
