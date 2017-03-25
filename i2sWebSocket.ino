#include <EEPROM.h>
#include <Ticker.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <i2s.h>

#include <WebSockets.h>
#include <WebSocketsClient.h>


#define BUFBIT (8)
#define BUFSIZE (1<<BUFBIT)
#define BUFMASK (BUFSIZE-1)
#define I2S_READ_QUEUE_SIZE 64
#define WORD_BYTE_SIZE 4 


int LRSelect=0;
static uint32_t buffer[I2S_READ_QUEUE_SIZE * WORD_BYTE_SIZE]={0};
static short ring_buffer[BUFSIZE]={100};
long write_counter=0;
long read_counter=0;
Ticker ticker;
boolean f_flash=false;
boolean send_flag=false;
boolean shouldSaveConfig = false;
int send_count=0;

struct API_CONFIG {
  char username[40];
  char password[40];
  char server[40];
  char endpoint[40];
};

API_CONFIG apiConfig;

WebSocketsClient wclient;

//When you use RX I2S module, You should be connect IO02 and IO14, I2S module(MIC or someone) WS pin.
//Because, RX WS pin read only in ESP8266.
//TX
//IO15  CLK
//IO02  WS
//IO03  DATA 
//RX
//IO13  CLK
//IO14  WS
//IO12  DATA
void flash_buf();
void read_buffer();

void saveConfigCallback () {
  Serial.println("Should save config");
  shouldSaveConfig = true;
}

void connectWiFi(){
  EEPROM.begin(200);
  EEPROM.get<API_CONFIG>(0, apiConfig);
  WiFiManagerParameter api_server("server", "server", apiConfig.server, 40);
  WiFiManagerParameter api_endpoint("endpoint", "endpoint uri", apiConfig.endpoint, 40);
  WiFiManagerParameter api_user("username", "WebAPI username", apiConfig.username, 40);
  WiFiManagerParameter api_password("password", "WebAPI password", apiConfig.password, 40, "type=password");
  
  
  WiFiManager wifiManager;
  wifiManager.setDebugOutput(false);
  wifiManager.addParameter(&api_user);
  wifiManager.addParameter(&api_password);
  wifiManager.addParameter(&api_server);
  wifiManager.addParameter(&api_endpoint);
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  
  wifiManager.setBreakAfterConfig(true);
  delay(3000);
  if (digitalRead(0) == LOW) {
    wifiManager.resetSettings();
  }
  wifiManager.autoConnect("EspAudio");
  if (shouldSaveConfig) {
    strcpy(apiConfig.username, api_user.getValue());
    strcpy(apiConfig.password, api_password.getValue());
    strcpy(apiConfig.server, api_server.getValue());
    strcpy(apiConfig.endpoint, api_endpoint.getValue());
    EEPROM.put<API_CONFIG>(0, apiConfig);
    EEPROM.commit();
    Serial.println("write to EEPROM");
  }  
}

void webSocketEvent(WStype_t type, uint8_t* payload, size_t lenght){
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[WSc] Disconnected!\n");
      f_flash=false;
      break;
    case WStype_CONNECTED:
      Serial.printf("[WSc] Connected!\n");
      f_flash=true;
      break;
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("setup");
  connectWiFi();
  delay(500);
  wclient.begin(apiConfig.server,8080, apiConfig.endpoint);
  wclient.onEvent(webSocketEvent);
  i2s_begin();
  i2s_set_rate(16000);// I2S sampling rate is 16kHZ and I2S RX CLK output 1Mhz.
  delay(10);
  ticker.attach_ms(1,read_buffer);  
}

void read_buffer(){
  if(send_flag==false){
    boolean result = i2s_read_async(buffer);
    if(result){
      int i;
      for(i=1; i < 64; i+=2){
        ring_buffer[write_counter & BUFMASK] = (short)(0xffff & ((0x7FFFFF00 & buffer[i])>>15));
        write_counter++;
      }
      result =false;
    }
    if((write_counter & BUFMASK)==0){
      send_flag=true;
    }
    
  }
}


void loop() {
  wclient.loop(); 
  if(send_flag){
    wclient.sendBIN((uint8_t *)ring_buffer, BUFSIZE*2);  
    send_flag=false;
  }else{
  }  
}

