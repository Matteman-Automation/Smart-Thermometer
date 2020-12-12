/*
 *     M      M     AA    TTTTTTT  TTTTTTT  EEEEEEE M      M     AA     NN    N
 *     MM    MM    A  A      T        T     E       MM    MM    A  A    NN    N
 *     M M  M M   A    A     T        T     E       M M  M M   A    A   N  N  N
 *     M  MM  M   AAAAAA     T        T     EEEE    M  MM  M   AAAAAA   N  N  N - AUTOMATION
 *     M      M  A      A    T        T     E       M      M  A      A  N   N N 
 *     M      M  A      A    T        T     E       M      M  A      A  N    NN  
 *     M      M  A      A    T        T     EEEEEEE M      M  A      A  N    NN  
 *     
 *     
 *     Project    : temperature and humidity
 *     Versie     : 2.4
 *     Datum      : 12-2020
 *     Schrijver  : Ap Matteman
 */    


#include <PubSubClient.h>
#include <ESP8266WiFi.h> 
#include <ESP8266WebServer.h>
#include <DHT.h>
#include <LedControl.h>

/* WiFi en MQTT gegevens */
const char* ssid = "My-HA-network";
const char* password = "MyHome@48";
const char* mqtt_server = "192.168.10.10";
const char* mqtt_user = "MQTTHome";
const char* mqtt_password = "Home@48";

ESP8266WebServer server(80);
WiFiClient espClient;
PubSubClient client(espClient); // MQTT Client


// Sensors
#define DHTTYPE DHT22
uint8_t DHTPin = D2; // DTH22 sensor - Inside temp & humidity
DHT dht(DHTPin, DHTTYPE);

// Digital display
// Pin D7 => DIN (Data In), pin D6 => Clock, Pin D5 => Load, Ontal display's = 1
LedControl lc = LedControl(D7, D6, D5, 1);               

// Motionssensor
const int PIRPIN=D1;   

float Temperature;
float Humidity;
long lastRead = 0;
long MQTTSend = 0;
long Motion = 0;
// long Duration = 10000;  // 10000 = 10 seconds, I use 10 seconds for demo, change to a longer time.
long Duration = 250000;  // Keep the display 25 minutus on after the last motion detection.
int Retry = 0;          // times the MQTT server is not available. 
byte Night = 0; 
byte PriMotion = 0;

 
void setup() {
  
  Serial.begin(115200);
  delay(100);
  
  pinMode(DHTPin, INPUT);
  dht.begin();              

  Serial.println("Connecting to ");
  Serial.println(ssid);

  //Connect with WiFi network
  WiFi.begin(ssid, password);

  //Check WiFi connection
  while (WiFi.status() != WL_CONNECTED) {
  delay(1000);
  Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected..!");
  Serial.print("Got IP: ");  Serial.println(WiFi.localIP());

  // Connect with MQTT server
  client.setServer(mqtt_server, 1883);
  reconnect();

  // WebServer
  server.on("/", handle_OnConnect);
  server.onNotFound(handle_NotFound);
  server.begin();
  Serial.println("HTTP server started");

  // Digital display
  // 0 - this is the first module. If we have more modules they will be numbered 0, 1, 2, 3, etc.
  lc.shutdown(0, false);        // wake up from Powersave mode
  lc.setIntensity(0,5);         // Brightness
  lc.clearDisplay(0);           // Clear all digits

  //Connect to the PIR (Passive InfraRed detector)
  pinMode(PIRPIN, INPUT);

  // receive information from Home Assistant
  client.setCallback(callback);
}

void reconnect() {
  // Recconnect to the MQTT server

  Serial.println("Attempting Reconnect MQTT connection...");
  // Attempt to connect
  if (client.connect(mqtt_server, mqtt_user, mqtt_password)) 
  {
    Serial.println("MQTT connected");
    Retry = 0;
    client.subscribe("master/night");
  } 
  else 
  {
    // MQTT connection does not work
    ++Retry;
    Serial.println();
    Serial.print("MQTT failed, rc=");
    Serial.println(client.state());
    if (Retry > 15)
    {
      Serial.println("REBOOTING");
      Retry = 0;
      Serial.println(" Reboot in 2 seconds");
      // Wait 2 seconds before retrying
      delay(2000);
      ESP.restart();
    }
  }

  
}

void callback(char* topic, byte* message, unsigned int length) {

  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  // If a message is received on the topic room/lamp, you check if the message is either on or off. Turns the lamp GPIO according to the message
  Serial.print("Night is ");
  if(messageTemp == "ON"){
  
    Night = 1;
    Serial.print("On");
  }
  else if(messageTemp == "OFF"){
    Night = 0;
    Serial.print("Off");
  }

 if(Night==0){
    lc.shutdown(0, false);
    lc.setIntensity(0,5);
  }
  else if(Night==1) {
    lc.shutdown(0, false);
    lc.setIntensity(0,1);
  }
}


void loop() {

  
  long now = millis();
  server.handleClient();
  // Check for motion
  int pirValue = digitalRead(PIRPIN); // Read PIR
  if (pirValue == LOW) {
    // No Motion
    // Do nothing
  }
  else {
    // Motion
    PriMotion = 1;
    Motion = now; 
  }

  //Wait 5 seconds before reading the DHT22 sensor and write to serial port
  if (now - lastRead > 5000) {
    lastRead = now;
    Temperature = dht.readTemperature(); // Gets the values of the temperature
    Humidity = dht.readHumidity(); // Gets the values of the humidity
    // Writing to serial port 
    Serial.println();
    Serial.print("Got IP: ");  Serial.print(WiFi.localIP()); Serial.print("\t");
    Serial.print("Temperature: "); Serial.print(Temperature); Serial.print(" *C /t");
    Serial.print("Humidity: "); Serial.print(Humidity); Serial.print(" %\t");
    Serial.print("PriMotion: "); Serial.print(PriMotion); Serial.print(" \t");
    Serial.print("Retry MQTT="); Serial.print(Retry); Serial.print(" \t");
    Serial.print("Time PIR Counter="); Serial.print(now - Motion);
  }
  
  // Temperature and Humidity to the display
  int temp = Temperature * 10;
  int humi = Humidity * 10;

  int dec;      // Decimalen
  int ones;     // eenheden
  int tens;     // tientallen

  dec = temp % 10;
  temp = temp / 10;
  ones = temp % 10;
  temp = temp / 10;
  tens = temp % 10;
  lc.setDigit(0,7,tens,false);
  lc.setDigit(0,6,ones,true);
  lc.setDigit(0,5,dec,false);
  lc.setChar(0,4,'c',false);

  dec = humi % 10;
  humi = humi / 10;
  ones = humi % 10;
  humi = humi / 10;
  tens = humi % 10;
  lc.setDigit(0,3,tens,false);
  lc.setDigit(0,2,ones,true);
  lc.setDigit(0,1,dec,false);

  if (PriMotion == 1){
    if (now - Motion < Duration){
      // Display On
      if(Night==0){
        lc.setIntensity(0,5);
        lc.shutdown(0, false);
      }
      else if(Night==1) {
        lc.setIntensity(0,1);
        lc.shutdown(0, false);
      }
    }
    else {
      PriMotion = 0;
    }      
  }
  else {
    lc.shutdown(0, true);  
  }

  if (now - MQTTSend > 500) {
    String MyIP;
    MyIP = WiFi.localIP().toString().c_str();
    MQTTSend = now;
    // Check MQTT connection
    if (!client.connected()) {
     Serial.println();
     Serial.println("No MQTT connection !!!");
     reconnect();
    }  
    else
    {
      // Send info to MQTT server
      client.publish("Office/Sensor/Inside/Temperature", String(Temperature).c_str());
      client.publish("Office/Sensor/Inside/Humidity", String(Humidity).c_str());
      if (PriMotion == 0)
      {
        client.publish("Office/Sensor/Inside/MotionDesk","OFF", 2);
      } else
      {
        client.publish("Office/Sensor/Inside/MotionDesk","ON", 2);
      }
      client.publish("Office/Sensor/Inside/IP", String(MyIP).c_str());
    }
  }
  
}

void handle_OnConnect() {

  // Temperature = dht.readTemperature(); // Gets the values of the temperature
  // Humidity = dht.readHumidity(); // Gets the values of the humidity 
  server.send(200, "text/html", SendHTML(Temperature,Humidity, PriMotion)); 
  Serial.println();
  Serial.println("WebPage");
  
}

void handle_NotFound(){
  server.send(404, "text/plain", "Not found");
}

String SendHTML(float Temperaturestat,float Humiditystat, byte Motionstat){
  String ptr = "<!DOCTYPE html> <html>\n";
  ptr +="<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  ptr +="<meta http-equiv=\"refresh\" content=\"15\">\n";    // Auto Refresh the site               
  ptr +="<title>Office Matteman Automation</title>\n";
  ptr +="<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  ptr +="body{margin-top: 50px;} h1 {color: #444444;margin: 50px auto 30px;}\n";
  ptr +="p {font-size: 24px;color: #444444;margin-bottom: 10px;}\n";
  ptr +="</style>\n";
  ptr +="</head>\n";
  ptr +="<body>\n";
  ptr +="<div id=\"webpage\">\n";
  ptr +="<h1>Office Matteman Automation</h1>\n";
  
  ptr +="<p>Temperature: ";
  ptr +=Temperaturestat;
  ptr +=" C</p>";
  ptr +="<p>Humidity: ";
  ptr +=Humiditystat;
  ptr +=" %</p>";

  if (Motionstat == 1){
    ptr +="<p>Motion Detect</p> ";
  } else {
    ptr +="<p>NO Motion Detect</p> ";
  }
  
  ptr +="</div>\n";
  ptr +="</body>\n";
  ptr +="</html>\n";
  return ptr;
}
