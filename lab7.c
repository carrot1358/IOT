#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <NoDelay.h>
#include <Bounce2.h>
#include "DHT.h"
#include <BH1750.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <BH1750.h>

#define DHTPIN 2    
#define DHTTYPE DHT22

#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 64 
#define OLED_RESET     -1 

#define ADC A0


const char* ssid = "_____________";
const char* password = "____________";

const char* mqtt_server = "broker.netpie.io";
const int mqtt_port = 1883;
const char* mqtt_Client = "_________________________________";
const char* mqtt_username = "__________________________________";
const char* mqtt_password = "________________________________";

const byte LED = D5;
const byte Buzzer = D8;
const int swPin = D7; 
int mode = 1,last_mode;
unsigned long buttonPressStartTime = 0;  
bool ismodeselct = false;

bool isADC_Alarm=true, isTemp_Alarm=false, isLight_Alarm=false, isAlarm=false;
bool LightHL=true,TempHL=true, adcHL=true;
float lightLimit = 100,tempLimit=25,adcLimit=2.5;
String AlarmMode;

WiFiClient espClient;
PubSubClient client(espClient);
noDelay buzzer(250);
noDelay Post_info(500);
DHT dht(DHTPIN, DHTTYPE); 
Adafruit_SSD1306 OLED(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
BH1750 lightMeter(0x23);

Bounce debouncer1 = Bounce();

char msg[200];

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection…");
    if (client.connect(mqtt_Client, mqtt_username, mqtt_password)) {
      Serial.println("connected");
      client.subscribe("@msg/#");  
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println("try again in 5 seconds");
      delay(5000);
    }
  }
}
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  String message;
  String tpc;
  for (int i = 0; i < length; i++) {
    message = message + (char)payload[i];
  }
  Serial.println(message);
  getMsg(topic, message);
  AlarmMode = getAlarmMode(topic,message);
  getLimitAlarm(topic,message);
  getHighLowAlarm(topic,message);
}

void setup() {
  Serial.begin(115200);
  dht.begin();
  Wire.begin();
  OLED.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  if (lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE)) {
    Serial.println(F("BH1750 Advanced begin"));
  } else {
    Serial.println(F("Error initialising BH1750"));
  }


  pinMode(LED,OUTPUT);
  pinMode(swPin, INPUT_PULLUP);
  pinMode(Buzzer,OUTPUT);

  debouncer1.attach(swPin);
  debouncer1.interval(50);
  
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
}

void loop() {
  if(isAlarm){
    bool Buzzer_status = false;
    if(buzzer.update()){
      Buzzer_status = !Buzzer_status;
      digitalWrite(Buzzer,HIGH);
      digitalWrite(LED,Buzzer_status);
    }
  }
  if(digitalRead(swPin) == 0){
    while(digitalRead(swPin)==0);
    delay(200);
    isAlarm = false;
  }
  
  if (!client.connected()) {
     reconnect();
  }

  float h = dht.readHumidity();
  float t = dht.readTemperature();
  

  int LED1_Status = digitalRead(LED);

  int readADC = analogRead(A0);
  int mapADC = map(readADC,0,1023,0,330);
  float calADC = mapADC / 100.0;
  
  float lux = lightMeter.readLightLevel();
  
  
  if(isADC_Alarm){
    ADC_Alarm(calADC,adcLimit,adcHL);
  }else if(isTemp_Alarm){
    Temp_Alarm(t,tempLimit,TempHL);
  }else if (isLight_Alarm){
    Light_Alarm(lux,lightLimit,LightHL);
  }
    //////////////////////////////
    // Message Shadow to NetPie //
    //////////////////////////////
    String data = "{\"data\":{\"Humidity\": " +String(h)+
                   ",\"Temperature\": " +String(t)+
                   ",\"LED\": " +String(LED1_Status)+
                   ",\"ADC\": " +String(calADC) +
                   ",\"Light\": " +String(lux) +
                   ",\"isAlarm\": " + isAlarm+
                  //which one to cheack
                   ",\"isADC_Alarm\": " +isADC_Alarm+
                   ",\"isTemp_Alarm\": " +isTemp_Alarm+
                   ",\"isLight_Alarm\": " +isLight_Alarm+
                  // Limit
                   ",\"adcLimit\": " +adcLimit+
                   ",\"tempLimit\": " +tempLimit+
                   ",\"lightLimit\": " +lightLimit+
                  //Highter or Lower
                   ",\"adcHL\": " + adcHL+
                   ",\"TempHL\": " + TempHL+
                   ",\"LightHL\": " + LightHL
                   +  "}}";
    Serial.println(data);
    data.toCharArray(msg , (data.length() + 1));
    client.publish("@shadow/data/update", msg);
    client.loop();
  
  delay(500);
}


String getMsg(String topic_, String message_) {     
  if (topic_ == "@msg/LED") {
    if (message_ == "on") {                  //#["esp8266"].publishMsg("LED","on")
      digitalWrite(LED, HIGH); //LED เปิด
    } else if (message_ == "off") {          //#["esp8266"].publishMsg("LED1","off")
      digitalWrite(LED, LOW); //LED1 ปิด
    }
  }
  return "";
}

String getAlarmMode(String topic_, String message_){
  if(topic_ == "@msg/Mode"){
    if(message_ == "ADC"){
      Off_Alarm();
      isADC_Alarm = true;
      return "ADC";
    }
    else if(message_ == "Temp"){
      Off_Alarm();
      isTemp_Alarm = true;
      return "Temp";
    }
    else if(message_ == "Light"){
      Off_Alarm();
      isLight_Alarm = true;
      return "Light";
    }
  }
  return "Null";
}
void getLimitAlarm(String topic_, String message_){
  if(topic_ == "@msg/adcLimit"){
    adcLimit = message_.toFloat();
  }
  if(topic_ == "@msg/tempLimit"){
    tempLimit = message_.toFloat();
  }
  if(topic_ == "@msg/lightLimit"){
    lightLimit = message_.toFloat();
  }
}

void getHighLowAlarm(String topic_,String message_){
  if(topic_ == "@msg/adcHL"){
    adcHL = message_.toInt();
  }
  if(topic_ == "@msg/TempHL"){
    TempHL = message_.toInt();
  }
  if(topic_ == "@msg/LightHL"){
    LightHL = message_.toInt();
  }
}
void Off_Alarm(){
  isADC_Alarm = false;
  isTemp_Alarm = false;
  isLight_Alarm = false;
}
void ADC_Alarm(float calADC, float onAlarm,bool isHigher){
  if(isHigher){
    if(calADC >= onAlarm){
    isAlarm = true;
    }
  }else{
    if(calADC <= onAlarm){
    isAlarm = true;
    }
  }
  
}

void Temp_Alarm(float t, float onAlarm, bool isHigher){
  if(isHigher){
    if(t >= onAlarm){
      isAlarm = true;
    }
  }else{
    if(t <= onAlarm){
      isAlarm = true;
    }
  }
}

void Light_Alarm(float lux, float onAlarm,bool isHigher){
  if(isHigher){
    if(lux >= onAlarm){
      isAlarm = true;
    }
  }else{
    if(lux <= onAlarm){
      isAlarm = true;
    }
  }
}
