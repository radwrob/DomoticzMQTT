/*
 Domoticz MQTT example

 It will reconnect to the server if the connection is lost using a blocking
 reconnect function. See the 'mqtt_reconnect_nonblocking' example for how to
 achieve the same result without blocking the main loop.
 
*/
#include "config.h"
//watchdog
#include <avr/wdt.h>

uint8_t attackState = 0;
uint8_t alarmIndicatorState = 0;
uint8_t alarmState = 0;
uint8_t sabotAlarmState = 0; 
uint8_t sabotSirenState = 0;
uint8_t sirenState = 0;
uint8_t lightState = 0;
uint8_t redState = 0;
uint8_t yellowState = 0;
uint8_t greenState = 0;
uint8_t relayStateTmp = 0;
uint8_t acSignal = 0;
uint8_t btnState = 0;
bool alarmTrigerred = false;

char payload_buf[64] = {0};

void callback(char* topic, byte* payload, unsigned int length) {
  char buffer[384];
  Sprint("Message arrived [");
  Sprint(topic);
  Sprint("] ");
  if( sizeof(buffer) < length ){
    Sprintln("Callback from Domoticz is too big...");
    return;    
  }else {
    for (int i=0;i<length;i++) {
      buffer[i] = (char)payload[i];
      Sprint((char)payload[i]);
    }    
  }
  Sprintln();
  char * pch;
  int idx;
  uint8_t value;   
  pch = strstr( buffer, "idx" );
  if( pch ) {    
    idx = atoi(pch + 7);
    Sprintln(idx);
  }else {
    Sprintln("idx not found in payload");
  }
  pch = strstr( buffer, "nvalue" );
  if( pch ) {  
    value = atoi(pch + 10);
    Sprintln(value);
  }else {
    Sprintln("nvalue not found in payload");
  }
  switch ( idx )
  {
     case LIGHT_IDX:
        lightState = value;
        break;
     case LED_GREEN_IDX:
        greenState = value;
        break;
     case LED_RED_IDX:
        redState = value;
        break;
     case LED_YELLOW_IDX:
        yellowState = value;
        break;
     case SIREN_IDX:
        sirenState = value;
        break;        
     case SABOT_SIREN_IDX:
        sabotSirenState = value;
        break;
     case ALARM_IDX:
        alarmState = value;
        break;
     case SABOT_ALARM_IDX:
        sabotAlarmState = value;
        break;
     case ALARM_INDICATOR_IDX:
        alarmIndicatorState = value;
        break;   
     case ATTACK_INDICATOR_IDX:
        attackState = value;
        attack(attackState);
        break;                                                               
     default:
        Sprintln("Undefined IDX");
  }  
}

EthernetClient ethClient;
PubSubClient client(ethClient);

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Sprint("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("arduinoClient")) {
      Sprintln("connected");
      client.subscribe("domoticz/out");
    } else {
      Sprint("failed, rc=");
      Sprint(client.state());
      Sprintln(" try again in 5 seconds");
      wdt_reset();
      // Wait 5 seconds before retrying
      delay(5000);
    }
    wdt_reset();
  }
}

void setup()
{
  Sbegin(9600);
  
  wdt_enable(WDTO_8S);
  
  sensors.begin();
  dht.setup(GPIO_DHT11);
  
  pinMode(GPIO_ALARM, INPUT_PULLUP);
  pinMode(GPIO_SABOT_ALARM, INPUT_PULLUP);
  pinMode(GPIO_SABOT_SIREN, INPUT_PULLUP);
  pinMode(BUTTON, INPUT_PULLUP);
  pinMode(GPIO_SIREN, OUTPUT);
  pinMode(GPIO_LIGHT, OUTPUT);
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
      
  client.setServer(server, 1883);
  client.setCallback(callback);

  Ethernet.begin(mac, ip);

  // Allow the hardware to sort itself out
  delay(1500);
}

void loop()
{
  wdt_reset();
  
  if (!client.connected()) {
    reconnect();
    getDeviceInfo(ALARM_IDX);
    getDeviceInfo(SABOT_ALARM_IDX);
    getDeviceInfo(SABOT_SIREN_IDX);
    getDeviceInfo(SIREN_IDX);
    getDeviceInfo(LIGHT_IDX);
    getDeviceInfo(LED_GREEN_IDX);
    getDeviceInfo(LED_YELLOW_IDX);
    getDeviceInfo(LED_RED_IDX);
    getDeviceInfo(ALARM_INDICATOR_IDX);
    getDeviceInfo(ATTACK_INDICATOR_IDX);
    sendTemperature();    
    sendDHT11Value();
  }
  client.loop();

  // Read digital motion value
  unsigned long currentMillis = millis();
  relayStateTmp = digitalRead(GPIO_ALARM);
  if( relayStateTmp != alarmState ){
    alarmState = relayStateTmp;
    Sprintln("Alarm: " + String(alarmState)); 
    changeSwitchState( ALARM_IDX, alarmState );
    if ((alarmIndicatorState == 1) && (alarmState == 1) && (attackState == 0)) {
      alarmTrigerred = true;    
      armedAlarm(alarmIndicatorState, alarmTrigerred);  
      previousMillisAlarm = currentMillis;
    }
  }
  relayStateTmp = digitalRead(GPIO_SABOT_ALARM);
  if( relayStateTmp != sabotAlarmState ){
    sabotAlarmState = relayStateTmp;
    Sprintln("Sabotage Alarm: " + String(sabotAlarmState)); 
    changeSwitchState( SABOT_ALARM_IDX, sabotAlarmState );      
  }
  relayStateTmp = digitalRead(GPIO_SABOT_SIREN);
  if( relayStateTmp != sabotSirenState ){
    sabotSirenState = relayStateTmp;
    Sprintln("Sabotage Siren: " + String(sabotSirenState)); 
    changeSwitchState( SABOT_SIREN_IDX, sabotSirenState );      
  }    

  //TEMPERATURE  
  currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;
    sensors.requestTemperatures();
    sendTemperature();    
    sendDHT11Value();
    reportAcSignal();    
  }

  //MOTION DETECT
  currentMillis = millis();
  if(alarmTrigerred){
    if (currentMillis - previousMillisAlarm >= intervalAlarm) {
      alarmTrigerred = false;
      if( alarmIndicatorState ){
        attackState = 1;
        attack(attackState);
      }
    }  
  }

  //ALARM BUTTON
  relayStateTmp = digitalRead(BUTTON);
  if( relayStateTmp != btnState ){  
    btnState = relayStateTmp;
    if( alarmIndicatorState || attackState ){
      alarmIndicatorState = 0;
      armedAlarm(alarmIndicatorState,0);    
      changeGpioState(GPIO_SIREN, HIGH);
      delay(100);       
      changeGpioState(GPIO_SIREN, LOW);
      delay(900);   
      wdt_reset();                     
    }else {
      alarmIndicatorState = 1;
      armedAlarm(alarmIndicatorState,0);
      for(int i=0;i<10;i++){
        changeGpioState(GPIO_SIREN, HIGH);
        delay(100);       
        changeGpioState(GPIO_SIREN, LOW);
        delay(900);   
        wdt_reset();       
      }
    }
    attackState = 0;
    attack(attackState);
  }
  
  updateAcSignal();
  changeAllGpio();
}

void updateAcSignal(){
  //check L signal on power supply
  uint8_t tmp = ( analogRead(A_BATTERY) < 800 ) ? 12 : 230;
  if(tmp != acSignal){
    acSignal = tmp;
    reportAcSignal();
  }
}

void reportAcSignal(){
  Sprintln("AC: " + String(acSignal));
  changeDomoticzSingleValue(VOLTAGE_IDX, acSignal);
}

void attack(uint8_t enable) {
  changeSwitchState(ATTACK_INDICATOR_IDX, enable);  
  if( enable ){
    greenState = 1;
    yellowState = 1;
    redState = 1;  
    sirenState = 1;
    lightState = 1;
    changeAllGpio(); 
    changeSwitchState(LED_GREEN_IDX,greenState);
    changeSwitchState(LED_YELLOW_IDX,yellowState);
    changeSwitchState(LED_RED_IDX,redState);     
    changeSwitchState(SIREN_IDX,sirenState);     
    changeSwitchState(LIGHT_IDX,lightState);     
  }else{
    sirenState = 0;
    changeGpioState(GPIO_SIREN, LOW);
    changeSwitchState(SIREN_IDX,sirenState); 
  }
}

void armedAlarm(uint8_t enableAlarm, uint8_t trigerred){
  if(trigerred){
    greenState = 0;
    yellowState = 1;
    redState = 0;    
  }else if(enableAlarm){
    greenState = 0;
    yellowState = 0;
    redState = 1;
  }else{
    greenState = 1;
    yellowState = 0;
    redState = 0;    
  }
  changeAllGpio();
  //update domoticz state
  changeSwitchState(ALARM_INDICATOR_IDX, enableAlarm);  
  changeSwitchState(LED_GREEN_IDX,greenState);
  changeSwitchState(LED_YELLOW_IDX,yellowState);
  changeSwitchState(LED_RED_IDX,redState);
}

void changeSwitchState(int idx, uint8_t value){
  char tmp[5];
  payload_buf[0] = 0;
  strcat(payload_buf,"{\"idx\":");
  sprintf(tmp, "%d", idx);
  strcat(payload_buf,tmp);
  strcat(payload_buf,",\"nvalue\":");
  sprintf(tmp, "%d", value);
  strcat(payload_buf,tmp);
  strcat(payload_buf,",\"svalue\":\"0\"}");   
  Sprintln(payload_buf);
  client.publish("domoticz/in", payload_buf);
}

void changeDomoticzSingleValue(int idx, float value){
  char tmp[10]; 
  payload_buf[0] = 0;
  strcat(payload_buf,"{\"idx\":");
  sprintf(tmp, "%d", idx);
  strcat(payload_buf,tmp);
  strcat(payload_buf,",\"nvalue\":0,\"svalue\":\"");
  dtostrf(value, 9, 2, tmp);
  strcat(payload_buf,tmp);
  strcat(payload_buf,"\"}");   
  Sprintln(payload_buf);
  client.publish("domoticz/in", payload_buf);
}

void changeDomoticzDoubleValue(int idx, int temp, int humid){
  char tmp[10];
  payload_buf[0] = 0;
  strcat(payload_buf,"{\"idx\":");
  sprintf(tmp, "%d", idx);
  strcat(payload_buf,tmp);
  strcat(payload_buf,",\"nvalue\":0,\"svalue\":\"");
  sprintf(tmp, "%d", temp);
  strcat(payload_buf,tmp);
  strcat(payload_buf,";");
  sprintf(tmp, "%d", humid);
  strcat(payload_buf,tmp);  
  strcat(payload_buf,";2\"}");  
  Sprintln(payload_buf);
  client.publish("domoticz/in", payload_buf);
}

void sendTemperature()
{
  float tempC;
  sensors.requestTemperatures();
  tempC = sensors.getTempC(sensor_black);  
  changeDomoticzSingleValue(TEMP_BLACK_IDX, tempC);
  tempC = sensors.getTempC(sensor_yellow);
  changeDomoticzSingleValue(TEMP_YELLOW_IDX, tempC);  
  tempC = sensors.getTempC(sensor_package);
  changeDomoticzSingleValue(TEMP_PACKAGE_IDX, tempC);    
}

void sendDHT11Value(){
  int temp = dht.getTemperature();
  int humid = dht.getHumidity();
  if( dht.getStatusString() == "OK" ){
    changeDomoticzDoubleValue(DHT11_IDX,temp,humid);
    Sprint("[DHT11] Temp: ");
    Sprintln(temp);
    Sprint("[DHT11] Humid: ");
    Sprintln(humid);
  }else{
    Sprintln("[DHT11] status failed");
  }
}

void getDeviceInfo(int idx){
  char tmp[5];
  payload_buf[0] = 0;
  itoa(idx,tmp,10);
  strcat(payload_buf,"{\"command\":\"getdeviceinfo\",\"idx\":");
  strcat(payload_buf,tmp); 
  strcat(payload_buf,"}");  
  Sprintln(payload_buf);
  client.publish("domoticz/in", payload_buf);
}

void changeGpioState(uint8_t gpio, uint8_t value){
    digitalWrite(gpio, value?HIGH:LOW);
}

void changeAllGpio(){
  changeGpioState(GPIO_SIREN, sirenState);
  changeGpioState(GPIO_LIGHT, lightState);
  changeGpioState(LED_GREEN, !greenState);  
  changeGpioState(LED_YELLOW, !yellowState);  
  changeGpioState(LED_RED, !redState);  
}

