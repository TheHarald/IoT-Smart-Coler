#include <ESP8266WiFi.h>
#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT_Client.h"
#include "Mail.h"
#include "Settings.h"

/***********подключение датчика воды*************/
float full_volume = 20;
float current_volume = 0;
float minimal_volume = 2;
char* text_from = "В кулере закначивается вода";
const byte Interrupt_Pin PROGMEM = D7;
volatile uint16_t count_imp; //количество сигналов
float count_imp_all; 
uint16_t liter_hour; // лтров в час
uint16_t liter_min; // литров в минуту
uint32_t currentTime, loopTime;
float liter; 
/*********************************/


bool RESTART_COLER = false;
bool allow_to_send = true;
char* location_level =  "5";
char* location_room = "501A";

WiFiClient client;
Adafruit_MQTT_Client mqtt(&client, AIO_SERVER, AIO_SERVERPORT, AIO_USERNAME, AIO_KEY);
Adafruit_MQTT_Publish volume = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/volume");
Adafruit_MQTT_Publish RESTART_P = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/restart");
Adafruit_MQTT_Subscribe text = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/text");
Adafruit_MQTT_Subscribe level = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/level");
Adafruit_MQTT_Subscribe room = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/room");
Adafruit_MQTT_Subscribe min_volume = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/min_volume");
Adafruit_MQTT_Subscribe RESTART_S = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/restart");
Adafruit_MQTT_Subscribe *subscription;

void MQTT_connect() {
  int8_t ret;

  // Stop if already connected.
  if (mqtt.connected()) {
    return;
  }

  Serial.print("Connecting to MQTT... ");

  uint8_t retries = 3;
  while ((ret = mqtt.connect()) != 0) { // connect will return 0 for connected
       Serial.println(mqtt.connectErrorString(ret));
       Serial.println("Retrying MQTT connection in 5 seconds...");
       mqtt.disconnect();
       delay(5000);  // wait 5 seconds
       retries--;
       if (retries == 0) {
         // basically die and wait for WDT to reset me
         while (1);
       }
  }
  Serial.println("MQTT Connected!");
}


void sendMessage(String msg){
  
   msg +=  "\n\rОт кулера на: " + String(location_level) + " этаже в комнате: " + String(location_room)+"\n\r";
   msg += "Оставшийся уровень воды в кулере: " + String(full_volume-current_volume) +"\n\r" + "Примероне время до опустощения: скоро будет!";
   
    
  if (sendMail(smtpHost, smtpPort, smtpUser, smtpPass, mailTo, mailSubject, msg+"\n\r")) {
    Serial.print(F("Mail sended through "));
    Serial.println(smtpHost);
  } else {
    Serial.print(F("Error sending mail through "));
    Serial.print(smtpHost);
    Serial.println('!');
  }
}

ICACHE_RAM_ATTR void getFlow ()
{
  count_imp++;
}

void calc_volume(){
  currentTime = millis();
  if (currentTime >= (loopTime + 1000))
  {
    loopTime = currentTime;
    count_imp_all = count_imp_all + count_imp;

    liter_hour = (count_imp * 60 * 60 / 450);
    liter_min = (count_imp * 60 / 450);

    count_imp = 0;
    Serial.println("---------");
    Serial.print(liter_hour); Serial.println(" л/ч");
    Serial.print(liter_min); Serial.println(" л/м");
    Serial.print(String(count_imp_all / 450, 2)); Serial.println(" л(всего)");
    current_volume = count_imp_all / 450;
    Serial.println("Объём вытекшей воды = " + String(current_volume));
  }
}





void setup() {
  Serial.begin(115200);

  //Установка натсроек датчика воды
  pinMode(Interrupt_Pin, INPUT);
  attachInterrupt(Interrupt_Pin, getFlow, FALLING);
  pinMode(2, OUTPUT);
  
  currentTime = millis();
  loopTime = currentTime;
  ////////////////////////////////////
   WiFi.begin(staSSID, staPass);
   while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print('.');
   }
   Serial.print(' ');
   Serial.println(WiFi.localIP());

   mqtt.subscribe(&text);
   mqtt.subscribe(&level);
   mqtt.subscribe(&room);
   mqtt.subscribe(&min_volume);
   mqtt.subscribe(&RESTART_S);
   
}



void loop() {

  MQTT_connect();

  calc_volume(); // подсчёт воды
  
  while ((subscription = mqtt.readSubscription(5000))) {
    if (subscription == &text) {
      Serial.print(F("Got text -> : "));
      text_from = (char *)text.lastread;
      Serial.println(text_from);
    }
    if (subscription == &min_volume) {
      Serial.print(F("Got min volume -> : "));
      minimal_volume = atof((char *)min_volume.lastread);
     Serial.println(minimal_volume);
    }

    if (subscription == &level) {
      Serial.print(F("Got level -> : "));
      location_level = (char *)level.lastread;
      Serial.println(location_level);
    }

    if (subscription == &room) {
      Serial.print(F("Got room -> : "));
      location_room = (char *)room.lastread;
      Serial.println(location_room);
    }
    
    if (subscription == &RESTART_S) {
      Serial.print(F("Got RESTART -> : "));
      RESTART_COLER = atof((char *)RESTART_S.lastread);
      Serial.println(RESTART_COLER);

      current_volume = 0;
      minimal_volume = 2;
      count_imp = 0;
      count_imp_all = 0;
      allow_to_send = true;
      Serial.println("current_volume restarted");
      Serial.println("minimal_volume restarted");

      RESTART_COLER = 0;

      if (! RESTART_P.publish(RESTART_COLER)) {
        Serial.println(F("Failed"));
      } else {
        Serial.println(F("OK restart publised!"));
       }
      
    }
  }// while


  if (! volume.publish(full_volume-current_volume)) {
    Serial.println(F("Failed"));
  } else {
    Serial.println(F("OK volume!"));
  }


  if((full_volume-current_volume < minimal_volume) && allow_to_send){
    Serial.print(full_volume);Serial.println("  < " + (String)minimal_volume);
    sendMessage(text_from);
    allow_to_send = false;
  }

}
