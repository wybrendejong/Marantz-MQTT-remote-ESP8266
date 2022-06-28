#include <ESP8266WiFi.h>
#include <PubSubClient.h>

// WiFi
const char* ssid = ""; // Enter your WiFi name
const char* password = "";  // Enter WiFi password

// MQTT Broker
const char* mqttServer = "";
const char* topic = "esp8266/marantz";
const char* mqttUser = "";
const char* mqttPassword = "";
const int mqttPort = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

/* some definitions from the IRremote Arduino Library */
#define RC5_ADDRESS_BITS 5
#define RC5_COMMAND_BITS 6
#define RC5_EXT_BITS 6
#define RC5_COMMAND_FIELD_BIT 1
#define RC5_TOGGLE_BIT 1
#define RC5_BITS (RC5_COMMAND_FIELD_BIT + RC5_TOGGLE_BIT + RC5_ADDRESS_BITS + RC5_COMMAND_BITS)  // 13
#define RC5X_BITS (RC5_BITS + RC5_EXT_BITS) // 19
#define RC5_UNIT 889  // (32 cycles of 36 kHz)
#define RC5_DURATION (15L * RC5_UNIT)  // 13335
#define RC5_REPEAT_PERIOD (128L *RC5_UNIT)  // 113792
#define RC5_REPEAT_SPACE (RC5_REPEAT_PERIOD - RC5_DURATION)  // 100 ms

#define IR_TX_PIN 15 // pin to use for tx

//#define TRACE // print binary commands to serial

uint8_t sLastSendToggleValue = 0;

/* 
 *  normal Philips RC-5 as in the https://en.wikipedia.org/wiki/RC-5
 *  code taken from IRremote with some changes
 */

int sendRC5(uint8_t aAddress, uint8_t aCommand, uint_fast8_t aNumberOfRepeats){
  digitalWrite(IR_TX_PIN, LOW);
  uint16_t tIRData = ((aAddress & 0x1F) << RC5_COMMAND_BITS);
  if (aCommand < 0x40) {
    // set field bit to lower field / set inverted upper command bit
    tIRData |= 1 << (RC5_TOGGLE_BIT + RC5_ADDRESS_BITS + RC5_COMMAND_BITS);
  }
  else {
    // let field bit zero
    aCommand &= 0x3F;
  }
  tIRData |= aCommand;
  tIRData |= 1 << RC5_BITS;
  if (sLastSendToggleValue == 0) {
    sLastSendToggleValue = 1;
    // set toggled bit
    tIRData |= 1 << (RC5_ADDRESS_BITS + RC5_COMMAND_BITS);
  }
  else
  {
    sLastSendToggleValue = 0;
  }
  uint_fast8_t tNumberOfCommands = aNumberOfRepeats + 1;
  while (tNumberOfCommands > 0)
  {
    for (int i = 13; 0 <= i; i--)
    {
#ifdef TRACE
      Serial.print((tIRData &(1 << i)) ? '1' : '0');
#endif
        (tIRData &(1 << i)) ? send_1() : send_0();
    }
    tNumberOfCommands--;
    if (tNumberOfCommands > 0)
    {
      // send repeated command in a fixed raster
      delay(RC5_REPEAT_SPACE / 1000);
    }
#ifdef TRACE
    Serial.print("\n");
#endif
  }
  digitalWrite(IR_TX_PIN, LOW);
  return 0;
}
 
int sendRC5_X(uint8_t aAddress, uint8_t aCommand, uint8_t aExt, uint_fast8_t aNumberOfRepeats)
{
  uint32_t tIRData = (uint32_t)(aAddress & 0x1F) << (RC5_COMMAND_BITS + RC5_EXT_BITS);
  digitalWrite(IR_TX_PIN, LOW);
  if (aCommand < 0x40)
  {
    // set field bit to lower field / set inverted upper command bit
    tIRData |= (uint32_t) 1 << (RC5_TOGGLE_BIT + RC5_ADDRESS_BITS + RC5_COMMAND_BITS + RC5_EXT_BITS);
  }
  else
  {
    // let field bit zero
    aCommand &= 0x3F;
  }

  tIRData |= (uint32_t)(aExt & 0x3F);
  tIRData |= (uint32_t) aCommand << RC5_EXT_BITS;
  tIRData |= (uint32_t) 1 << RC5X_BITS;

  if (sLastSendToggleValue == 0)
  {
    sLastSendToggleValue = 1;
    // set toggled bit
    tIRData |= (uint32_t) 1 << (RC5_ADDRESS_BITS + RC5_COMMAND_BITS + RC5_EXT_BITS);
  }
  else
  {
    sLastSendToggleValue = 0;
  }

  uint_fast8_t tNumberOfCommands = aNumberOfRepeats + 1;

  while (tNumberOfCommands > 0)
  {

    for (int i = 19; 0 <= i; i--)
    {
#ifdef TRACE
      Serial.print((tIRData &((uint32_t) 1 << i)) ? '1' : '0');
#endif
        (tIRData &((uint32_t) 1 << i)) ? send_1() : send_0();
      if (i == 12)
      {
#ifdef TRACE
        Serial.print("<p>");
#endif
        // space marker for marantz rc5 extension
        delayMicroseconds(RC5_UNIT *2 *2);
      }
    }
#ifdef TRACE
    Serial.print("\n");
#endif
    tNumberOfCommands--;
    if (tNumberOfCommands > 0)
    {
      // send repeated command in a fixed raster
      delay(RC5_REPEAT_SPACE / 1000);
    }
  }
  digitalWrite(IR_TX_PIN, LOW);
  return 0;
}

void setup(){
  //start serial connection
  Serial.begin(115200);
  Serial.print("test");
  delay(1000);
  // configure output pin
  pinMode(IR_TX_PIN, OUTPUT);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi..");
  }
  Serial.println("Connected to the WiFi network");
 
  client.setServer(mqttServer, mqttPort);
  client.setCallback(callback);
 
  while (!client.connected()) {
    Serial.println("Connecting to MQTT...");
 
    if (client.connect("ESP8266Marantz", mqttUser, mqttPassword )) {
 
      Serial.println("connected");  
 
    } else {
 
      Serial.print("failed with state ");
      Serial.print(client.state());
      delay(2000);
 
    }
  }
 
  client.publish("esp/test", "hello"); //Topic name
  client.subscribe("esp/test");
  client.subscribe(topic);
}

void send_0()
{
  digitalWrite(IR_TX_PIN, HIGH);
  delayMicroseconds(RC5_UNIT);
  digitalWrite(IR_TX_PIN, LOW);
  delayMicroseconds(RC5_UNIT);
}

void send_1()
{
  digitalWrite(IR_TX_PIN, LOW);
  delayMicroseconds(RC5_UNIT);
  digitalWrite(IR_TX_PIN, HIGH);
  delayMicroseconds(RC5_UNIT);
}

//
// MQTT PART 
// 

void callback(char *topic, byte *payload, unsigned int length) {
    Serial.print("Message arrived in topic: ");
    Serial.println(topic);
    Serial.print("Message:");
    String message;
    for (int i = 0; i < length; i++) {
        message = message + (char) payload[i];  // convert *byte to string
    }
    Serial.print(message);
// Payload messages can be found below, 'fm' 'cd' etc. Not every code has been found yet.
    if (message == "fm")          {sendRC5(17, 63, 1);}
    if (message == "cd")          {sendRC5(20, 63, 1);} 
    if (message == "ld")          {sendRC5(12, 63, 1);} 
    if (message == "vcr1")        {sendRC5(5, 63, 1);}  
    if (message == "vcr2")        {sendRC5(6, 63, 1);}
    if (message == "aux")         {sendRC5_X(16, 0, 6, 1);}
    if (message == "mute")        {sendRC5(16, 13, 1);} 
    if (message == "vol_up")      {sendRC5(16, 16, 1);} 
    if (message == "vol_down")    {sendRC5(16, 17, 1);} 
    if (message == "tuner_up")    {sendRC5(17, 30, 1);} 
    if (message == "tuner_down")  {sendRC5(17, 31, 1);}
    if (message == "standby")     {sendRC5(16, 12, 1);}
    if (message == "adv_det_off") {sendRC5(16, 82, 1); sendRC5(16, 82, 1);}  
    if (message == "adv_det_on")  {sendRC5(16, 83, 1);} 
    if (message == "disp")        {sendRC5_X(16, 15, 0, 1);} 
    Serial.println();
    Serial.println("-----------------------");
}

void loop()
{
  client.loop();
}
