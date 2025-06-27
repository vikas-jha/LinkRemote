#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <EEPROM.h>
#include <Wire.h>
#include "Adafruit_MPR121.h"

#ifndef _BV
#define _BV(bit) (1 << (bit)) 
#endif

#define D0 16
#define D1 05
#define D2 04
#define D3 00
#define D4 02
#define D5 14
#define D6 12
#define D7 13
#define D8 15
#define S3 10

const char* board_id = "000A.07E2ALAqt.0001";
String wifi_config = "";
String mqtt_config = "";
boolean connect_mqtt = false;

String mqtt_broker  = "";
int    mqtt_port    = 0;
String mqtt_username = "";
String mqtt_password = "";
String mqtt_topic = "";
String mqtt_controller_topic = "";

String mqtt_schema = "{0:RWb, [1:7]:RWB, 8:RW[0:5]}";

//D4 is connected to built in led
int controls[] = {D0, D4, D5, D6, D7, D8, S3};
int controls_length = sizeof(controls)/sizeof(controls[0]);

const byte interruptPin = D3;
volatile byte interruptCounter = 0;
int numberOfInterrupts = 0;

String cmd = "";
String eeprom_data = "";
int eeprom_length = 0;

WiFiClient espClient;
PubSubClient mqtt(espClient);
uint16_t rc_attempt = 32;
const int led = 13;

Adafruit_MPR121 cap = Adafruit_MPR121();
uint16_t eventTime[12] = {0};
uint16_t lasttouched = 0;
uint16_t currtouched = 0;
boolean  output      = true;
boolean changed      = false;

void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }
  message.trim();
  handleMessage(message);
}

void handleMessage(String message){
  message.trim();
  if(message.startsWith("SDV")){
   int idx_control_start = message.indexOf(' ');
   int idx_control_end = message.indexOf('=');
   int idx_value_start = idx_control_end + 1;
   int idx_value_end = message.length();
   int idx_id_start = message.indexOf('#');
   String id = "";
   if(idx_id_start > -1){
    idx_value_end = idx_id_start;
    id = message.substring(idx_id_start);
   }
   String control = message.substring(idx_control_start, idx_control_end);
   String value = message.substring(idx_value_start, idx_value_end);

   int i = control.toInt();
   if(i < controls_length){
    int pin = controls[i];
    digitalWrite(pin, (value.toInt() == 0 ? LOW : HIGH));
    String message = "ACT " + String(board_id) + ":SDV " + String(i) + "=" + digitalRead(pin) + "#" + id;
    Serial.println(message);
    publishMQTT(mqtt_controller_topic, message, true);
    }else{
      Serial.println("Bad request: " + message + + "#" + id);
    }
  }if(message.startsWith("SAV")){
   int idx_control_start = message.indexOf(' ');
   int idx_control_end = message.indexOf('=');
   int idx_value_start = idx_control_end + 1;
   int idx_value_end = message.length();
   int idx_id_start = message.indexOf('#');
   String id = "";
   if(idx_id_start > -1){
    idx_value_end = idx_id_start;
    id = message.substring(idx_id_start);
   }
   String control = message.substring(idx_control_start, idx_control_end);
   String value = message.substring(idx_value_start);

   int i = control.toInt();
   if(i < controls_length){
    int pin = controls[i];
    analogWrite(pin, value.toInt());
    String message = "ACT " + String(board_id) + ":SAV " + String(i) + "=" + analogRead(pin) + "#" + id;
    Serial.println(message);
    publishMQTT(mqtt_controller_topic, message, false);
    }else{
      Serial.println("Bad request: " + message + "#" + id);
    }
  }else if(message.startsWith("INF")){
    
  }else if(message.startsWith("GAV")){
    int idx_control_start = message.indexOf(' ');
    String control = message.substring(idx_control_start);
    int i = control.toInt();
    String message = "GAV " + String(board_id) + ":SAV " + String(i) + "=" + analogRead(controls[i]);
    publishMQTT(mqtt_controller_topic, message, false);
    
  }else if(message.startsWith("GDV")){
    int idx_control_start = message.indexOf(' ');
    String control = message.substring(idx_control_start);
    int i = control.toInt();
    String message = "GDV " + String(board_id) + ":SAV " + String(i) + "=" + digitalRead(controls[i]);
    publishMQTT(mqtt_controller_topic, message, true);
    
  }else if(message.startsWith("PNG")){
    publishMQTT(mqtt_controller_topic, ("PNG " + String(board_id)) + ":", false);
  }else if(message.startsWith("SCM")){
    publishMQTT(mqtt_controller_topic, "SCM " + String(board_id) + ":" + mqtt_schema, false);
  }

}

void handleInterrupt() {
    interruptCounter++;
}

void setup(void) {
  String ssid = "";
  String password = "";
  
  for (uint8_t i = 0; i < controls_length; i++) {
    pinMode(controls[i], OUTPUT);
    digitalWrite(controls[i], HIGH);
  }

  //pinMode(interruptPin, INPUT_PULLUP);
  //attachInterrupt(digitalPinToInterrupt(interruptPin), handleInterrupt, FALLING);
  
  Serial.begin(115200);
  Serial.setTimeout(10);

  if (!cap.begin(0x5A)) {
    Serial.println("MPR121 not found, check wiring?");
    while (1);
  }
  Serial.println("MPR121 found!");

  cap.setThreshholds(6,6);
  
  Serial.println(" ");
  Serial.println("Number of controls: " + String(controls_length));
  
  EEPROM.begin(512);
  readEEPROM();
  
  if(eeprom_length > 0 && eeprom_length < 512){
    String data = eeprom_data;
    int idx = data.indexOf(';');
    while(idx > 0){
      String substr = data.substring(0, idx + 1);
      if(substr.startsWith("WIFI=")){
        wifi_config = substr.substring(5);
        Serial.println("WIFI " + wifi_config);
      }else if(substr.startsWith("MQTT=")){
        mqtt_config = substr.substring(5);
        Serial.println("MQTT " + mqtt_config);
      }
      data = data.substring(idx + 1);
      idx = data.indexOf(';');
    }
  }
  
  if(wifi_config.length() > 0){
    int i = wifi_config.indexOf(':');
    String ssid = wifi_config.substring(0, i);
    String password = wifi_config.substring(i + 1, wifi_config.length() - 1);
    Serial.println(ssid + ":" + password);

    Serial.print("Connecting to ");
    Serial.print(ssid);
    WiFi.begin(ssid.c_str(), password.c_str());

    Serial.println("");
  
  
    digitalWrite(LED_BUILTIN, LOW);
    // Wait for connection
    int attempt = 0;
    while (WiFi.status() != WL_CONNECTED && attempt < 10) {
      delay(500);
      Serial.print(".");
      attempt++;
    }
    digitalWrite(LED_BUILTIN, HIGH);
    Serial.println();
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
  
  //Setup mqtt parameters
  if(mqtt_config.length() > 0){
    int idx_auth_end = mqtt_config.indexOf(' ');
    
    String auth = mqtt_config.substring(0, idx_auth_end);
    int idx_username_end = auth.indexOf(':');
    mqtt_username = auth.substring(0, idx_username_end);
    mqtt_password = auth.substring(idx_username_end + 1);

    String url = mqtt_config.substring(idx_auth_end + 1);

    int idx_host_end = url.indexOf(':');
    int idx_port_end = url.indexOf('/');
    
    mqtt_broker = url.substring(0, idx_host_end);
    mqtt_port = url.substring(idx_host_end + 1, idx_port_end).toInt();
    String cluster_id = url.substring(idx_port_end + 1, url.length() - 1);
    mqtt_topic = cluster_id + "/" + String(board_id);
    mqtt_controller_topic = cluster_id + "/controller" ;
    Serial.println(mqtt_username + "/" + "<password>" + "@" + mqtt_broker + ":" + mqtt_port + "=>" + mqtt_topic);
    connect_mqtt = true;
    mqtt.setServer(mqtt_broker.c_str(), mqtt_port);
    mqtt.setCallback(callback);
  }
  
}

void reconnect() {
  // Loop until we're reconnected
  if (connect_mqtt == true && !mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (mqtt.connect(board_id,mqtt_username.c_str(),mqtt_password.c_str())) {
      Serial.println("connected");
      // ... and resubscribe
      mqtt.subscribe(mqtt_topic.c_str(), 1);
    } else {
      Serial.print("failed, rc=");
      Serial.print(mqtt.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(500);
    }
  }
}

void loop(void) {
  delay(100);
    
  if (WiFi.status() == WL_CONNECTED && !mqtt.connected()) {
    reconnect();
  }
  mqtt.loop();
  serialEvent();

  currtouched = cap.touched();
  uint16_t now = millis();
  for (uint8_t i=0; i<12; i++) {
    // it if *is* touched and *wasnt* touched before, alert!
    if ((currtouched & _BV(i)) && !(lasttouched & _BV(i)) ) {
      
      if(eventTime[i] == 0 || now - eventTime[i] > 200){
        Serial.print(i); Serial.println(" touched");
      }

      eventTime[i] = now;
      
    }
    // if it *was* touched and now *isnt*, alert!
    if (!(currtouched & _BV(i)) && (lasttouched & _BV(i)) ) {
      if(eventTime[i] == 0 || now - eventTime[i] > 200){
        Serial.print(i); Serial.println(" released");
         if(i == 0){
          changed = !changed;
          if(changed){
            digitalWrite(D8, HIGH);
            digitalWrite(D7, HIGH);
          }else{
            digitalWrite(D8, LOW);
            digitalWrite(D7, LOW);
          }
        }
      }
      eventTime[i] = now;
    }
  }

  // reset our state
  lasttouched = currtouched;
}

void serialEvent() {
  while (Serial.available()) {
    char c = Serial.read();// read the incoming data as string
    if (c == ';') {
      processCommand(cmd);
      cmd = "";
    } else {
      cmd += c;
    }
  }
}

void processCommand(String command){
  Serial.println(command);
  command.trim();
  if(command.startsWith("SWF")){
    String wifi = command.substring(4, command.length()) + ";" ;
    String data = "WIFI="+wifi + "MQTT=" + mqtt_config;
    Serial.println(data);
    if(writeEEPROM(data)){
      wifi_config = wifi;
    }
  }else if(command.startsWith("SMQ")){
    String mqtt = command.substring(4, command.length()) + ";" ;
    String data = "WIFI="+wifi_config + "MQTT=" + mqtt;
    Serial.println(data);
    if(writeEEPROM(data)){
      mqtt_config = mqtt;
    }
  }else {
    Serial.println("No handler found.");
  }
}

void readEEPROM(){
  int addr = 0;
  Serial.println();
  String data = "";
  int length = 0 | EEPROM.read(addr);
  addr++;
  length = (length << 8) | (EEPROM.read(addr));
  addr++;
  Serial.println(length);
  eeprom_length = length;
  if(length > 0 && length < 500){
    for(int i=0;i<length;i++) 
    {
     data += (char)EEPROM.read(addr + i);
   }
 }
 eeprom_data = data;
 Serial.println(data);
}

boolean writeEEPROM(String data){
 int addr = 0;
 int length = data.length();
 if(length < 500){
    EEPROM.write(addr, (length & 0xff00) >> 8); 
    addr++; 
    EEPROM.write(addr, (length & 0xff)); 
    addr++;
    for(int i=0;i<data.length();i++) 
    {
      EEPROM.write(addr + i ,data[i]);
    }
    if(EEPROM.commit()){
      return true;
    }else{
      Serial.println("EEPROM write failed.");
      return false;
    }
  }
}


int getLowHigh(int v) {
  if ( v == 0) {
    return LOW;
  } else {
    return HIGH; 
  }
}

void set_register(int address, unsigned char r, unsigned char v){
    Wire.beginTransmission(address);
    Wire.write(r);
    Wire.write(v);
    Wire.endTransmission();
}


void publishMQTT(String topic, String message, boolean retained){
  int length = message.length();
  byte buffer[length + 1];
  message.getBytes(buffer, length + 1);
  mqtt.publish(topic.c_str(), buffer,length, retained);
}
