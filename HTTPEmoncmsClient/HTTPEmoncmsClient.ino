/*
  HTTP emoncms client
 
 This sketch connects to an emoncms server and sends sensor readings.
 
 Circuit:
 * Ethernet shield attached to pins 10, 11, 12, 13
 * DHT22 temperature and humidity sensor attached to port A0
 * Light sensor attached to port A1
 
 created 19 Apr 2012
 by Tom Igoe
 modified 3 Apr 2013
 by Baptiste Gaultier
 
 based on 
 http://arduino.cc/en/Tutorial/WebClientRepeating
 This code is in the public domain.
 
 */

#include <SPI.h>
#include <Ethernet.h>
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

// assign a MAC address for the ethernet controller.
// fill in your address here:
byte mac[] = { 
  0x90, 0xA2, 0xDA, 0x00, 0x69, 0xD5};
  
// fill in an available IP address on your network here,
// for auto configuration:
IPAddress ip(169, 254, 0, 64);
IPAddress subnet(255, 255, 0, 0);

// initialize the library instance:
EthernetClient client;

char server[] = "www.baptistegaultier.fr";     //emoncms URL
boolean lastConnected = false;                 // state of the connection last time through the main loop

void setup() {
  // start serial port:
  Serial.begin(9600);
  dht.begin();
  
  // give the ethernet module and DHT22 sensor time to boot up:
  delay(1000);
  // Display a welcome message
  Serial.println("HTTP emoncms client v0.1 starting...");
  
  // attempt a DHCP connection:
  Serial.println("Attempting to get an IP address using DHCP:");
  if (!Ethernet.begin(mac)) {
    // if DHCP fails, start with a hard-coded address:
    Serial.println("failed to get an IP address using DHCP, trying manually");
    Ethernet.begin(mac, ip);
  }
  // print the Ethernet board/shield's IP address:
  Serial.print("My IP address: ");
  Serial.println(Ethernet.localIP());
  
  wdt_enable(WDTO_8S); 
}

void loop() {
  wdt_reset();
  
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
  
  // if there's incoming data from the net connection.
  // send it out the serial port.  This is for debugging
  // purposes only:
  if (client.available()) {
    client.flush();
    client.stop();
  }
  
  // if there's no net connection, but there was one last time
  // through the loop, then stop the client:
  if (!client.connected() && lastConnected) {
    Serial.println();
    Serial.println("Disconnecting...");
    client.stop();
  }

  // if you're not connected, and at least four seconds have
  // passed since your last connection, then connect again and
  // send data:
  if(!client.connected() && power >= 0 && readings >= 2) {
    temperature = dht.readTemperature();
    humidity = dht.readHumidity();
    Serial.print("Sending data to emoncms");
    Serial.print(" power : ");
    Serial.print(power);
    Serial.println("W");
    Serial.print(" temperature : ");
    Serial.print(temperature,1);
    Serial.println("C");
    Serial.print(" humidity = ");
    Serial.print(humidity,1);
    Serial.print("%");
    
    sendData(power, temperature, humidity);
    readings = 0;
  }
  // store the state of the connection for next time through
  // the loop:
  lastConnected = client.connected();
}

// this method makes a HTTP connection to the server:
void sendData(float power, float temperature, float humidity) {
  // if there's a successful connection:
  if (client.connect(server, 80)) {
    Serial.println("Connecting...");
    // send the HTTP PUT request:
    client.print("GET /emoncms/input/post.json?apikey=27cba500bc32a91487c1bb6bacdcdfee&json={power");
    client.print(":");
    client.print(power);
    client.print(",temperature:");
    client.print(temperature);
    client.print(",humidity:");
    client.print(humidity);    
    client.println("} HTTP/1.1");
    client.println("Host: www.baptistegaultier.fr");
    client.println("User-Agent: Arduino-ethernet");
    client.println("Connection: close");
    client.println();
  } 
  else {
    // if you couldn't make a connection:
    Serial.println("Connection failed");
    Serial.println("Disconnecting...");
    client.stop();
  }
}
