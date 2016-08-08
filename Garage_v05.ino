//
// Garagensteuerung + Temp per MQTT
//
// verwendete Topics:
//	garage/open					garage/connected (yes)			garage/humid
//	garage/close				garage/temperatur
//	garage/state (open,oeffnen,schliessen,geschlossen)		
//
// TODO 
//	mqtt mit username u. passwort	
//	WLAN passwort 
//	wenn Temperatur diff kleiner 1 grad keine mqtt		


#include <ESP8266WiFi.h>  //fuer ESP wifi
#include <ESP8266mDNS.h>  //fuer OTA
#include <WiFiUdp.h>      //fuer OTA
#include <ArduinoOTA.h>   //fuer OTA
// #include <Ticker.h>       //Zeitintervall für Watchdog
#include <PubSubClient.h> //fuer MQTT
#include <DHT.h>		  // fue DHT 22

#define DHTTYPE DHT22

long now =0; //in ms für torstatus
long lastMsg = 0; //in ms für torstatus
long nowDHT =0; //in ms für Dht messung
long lastMsgDHT = 0; //in ms für Dht messung
float temp = 0;
float hum = 0;
float diff = 1.0;
int min_timeout=7000; //in ms 
int min_timeoutDHT=30000;
int lastStatus=9999;	// 0=geschlossen, 1=offen, 2=öffnen, 3= schliessen
bool retain = false;
int reedStatus = 0; // Variable für die TorPosition
String torStatus = "closed"; //  kann auf open, closed, opening, closing gehen

//Ticker secondTick;
//volatile int watchdogCount = 0;  //volatile damit aus Sketch und aus Procedure die Variable verändert werden kann

// WiFi config
const char* ssid = "your SSID";
const char* password = "your PW";

//MQTT configuration
String mqtt_client_id="ESP8266";  
#define mqtt_server "192.10.10.10"   // MQTT config --> change IP 

//MQTT client
WiFiClient espClient;
PubSubClient mqtt_client(espClient);

// Pins
const int reedPin = 2; // Status fuers Garagentor
const int torPin = 5; // triggert öffnen und schliessen bei HIGH
#define DHTPIN 4	// TODO schauen ob der Pin geht

DHT dht(DHTPIN, DHTTYPE, 20); // Initialisiere DHT 3. Parameter für esp (increase threshold)
WiFiServer TelnetServer(8266);

void setup_wifi() {
  delay(10);
  Serial.print("Connecting to ");
//  Serial.print(wifi_ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  yield();
  Serial.println("OK");
  Serial.print("   IP address: ");
  Serial.println(WiFi.localIP());
}


// void ISRwatchdog(){
  // watchdogCount ++;
  // if (watchdogCount == 5){
    // Serial.print("goodby");
    // ESP.reset();
  // }
// }


void callback(String topic, byte* payload, unsigned int length) {
//
// callback zum Messagehandling auf subscribed Topics
// TODO Message im Aufruf mitgeben (String definieren)
//
 Serial.print("Message arrived [");
 Serial.print(topic);
 Serial.print("] ");
for (int i=0;i<length;i++) {
	char receivedChar = (char)payload[i];
	Serial.print(receivedChar);
}
Serial.println();
if (topic == "garage/open"){
    handleOpenRequest();
}
else if (topic == "garage/close"){
    handleCloseRequest();
}
}


void pubMQTT(String topic,String topic_val, bool retain){
//
// Funktion zum publishen (topic, inhalt, retain)
//
    Serial.print("Newest topic " + topic + " value:");
    Serial.println(String(topic_val).c_str());
    mqtt_client.publish(topic.c_str(), String(topic_val).c_str(), retain);
}


void torstatus(bool checkAgainstLast) {
  now = millis();
  if (now - lastMsg > min_timeout) {
    lastMsg = now;
    now = millis();
    reedStatus = digitalRead(reedPin);
    // this checks to see if the switch has changed state
    if (lastStatus != reedStatus || checkAgainstLast) {
        lastStatus = reedStatus;
        if(reedStatus == 0) {
          torStatus = "geschlossen";
        }
        else if(reedStatus == 1) {
         torStatus = "offen";
        }
        Serial.print("Sent ");
        Serial.print(String(torStatus).c_str());
		Serial.println(" to garage/state");
        pubMQTT("garage/state",torStatus,true);
    }
  }
}


void switchRelais () {
//
// TODO im Testmodus, später den PIN torPIN (=5) auf Low? setzten
// Delaytime stoppen und anpassen
//
  Serial.println();
  Serial.println("Relais an");
  digitalWrite(torPin, HIGH);
  delay (2000);
  Serial.println("Relais aus");
  digitalWrite(torPin, LOW);
  delay(8000);
  }


void handleOpenRequest (){
  if (torStatus != "offen" && torStatus != "oeffnen") {
    Serial.print("oeffne die Garage");
    torStatus= "oeffnen";
	lastStatus = 2;
    pubMQTT("garage/state",torStatus,false);
    switchRelais();
	torstatus(true);
   
  }
}


void handleCloseRequest (){
  if (torStatus != "geschlossen" && torStatus != "schliessen") {
    Serial.print("Schliesse die Garage");
    torStatus= "schliessen";
	lastStatus = 3;
    pubMQTT("garage/state",torStatus,false);
    switchRelais();
    torstatus(true);
 
  }
}


void mqtt_reconnect() {
  // Loop until we're reconnected
  while (!mqtt_client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqtt_client.connect(mqtt_client_id.c_str())) {
		Serial.println("connected");
		// subscribe to channel garage/open garage/close
		//publish connected auf true
		mqtt_client.subscribe("garage/open");
		mqtt_client.subscribe("garage/close");
		pubMQTT("garage/connected", "yes",true);
		torstatus (true);
    } 
	else {
		Serial.print("failed, rc=");
		Serial.print(mqtt_client.state());
		Serial.println(" try again in 5 seconds");
		// Wait 5 seconds before retrying
		delay(5000);
    }
  }
}


void readTemp () {

	nowDHT = millis();
	if (nowDHT - lastMsgDHT > min_timeoutDHT) {
		lastMsgDHT = nowDHT;
		nowDHT = millis();
		temp = dht.readHumidity();
		hum = dht.readTemperature();
		// Check if any reads failed and exit early (to try again).
		if (!isnan(h) || !isnan(t)) {
			pubMQTT("/garage/temperatur", String(t),true);
			pubMQTT("/garage/humid", String(h),true);
		}
	}
}


// ############################# START SETUP #########################################
void setup() {
  Serial.begin(115200);
  Serial.println("\r\nBooting...");
  // Relay schutz
digitalWrite(torPin, HIGH);
pinMode(reedPin, INPUT);

//secondTick.attach(1, ISRwatchdog);

 setup_wifi();
 dht.begin();
 
 // OTA config start
  Serial.print("Configuring OTA device...");
  TelnetServer.begin();   //Necesary to make Arduino Software autodetect OTA device  
  ArduinoOTA.onStart([]() {Serial.println("OTA starting...");});
  ArduinoOTA.onEnd([]() {Serial.println("OTA update finished!");Serial.println("Rebooting...");});
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {Serial.printf("OTA in progress: %u%%\r\n", (progress / (total / 100)));});  
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
  Serial.println("OK");
// OTA config ende

// MQTT config start
  Serial.println("Configuring MQTT server...");
  mqtt_client.setServer(mqtt_server, 1883);
  mqtt_client.setCallback(callback);
  Serial.printf("   Server IP: %s\r\n",mqtt_server);  
  //Serial.printf("   Username:  %s\r\n",mqtt_user);
  Serial.println("   Cliend Id: "+mqtt_client_id);  
  Serial.println("   MQTT configured!");
// MQTT config ende
}


// ######################### ENDE SETUP #################################

void loop() {
  ArduinoOTA.handle();
  if (!mqtt_client.connected()) {
	mqtt_reconnect();
	}
mqtt_client.loop();
torstatus (false);
readtemp ();  
//  watchdogCount = 0;
yield();
delay(100);
}
