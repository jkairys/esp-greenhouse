#include <Wire.h> 
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27,16,2); 

#include <ESP8266WiFi.h>
#include <PubSubClient.h>

#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>


#include <TimeLib.h>
#include <NtpClientLib.h>

#define LCD_SDA 0
#define LCD_SCL 2
#define PIN_RELAY D7
#define PIN_FLOW D2 

#define STATE_READY 0
#define STATE_WATERING 1

byte state = STATE_READY;

unsigned long pulse_count = 0;

const char* ssid = "mushroom";
const char* password = "gumboots";
const char* mqtt_server = "192.168.0.3";

IPAddress ip(192, 168, 0, 21);
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0); 

WiFiClient espClient;
PubSubClient client(espClient);

int run_duration = 0;

long lastMsg = 0;
char msg[50];
int value = 0;

bool ntp_ready = false;

// seconds to water for.
unsigned int watering_duration = 3;
// seconds between waterings
unsigned int watering_interval = 5*60;

void setup_ntp(){
  NTP.onNTPSyncEvent([](NTPSyncEvent_t error) {
    if (error) {
      Serial.print("Time Sync error: ");
      if (error == noResponse)
        Serial.println("NTP server not reachable");
      else if (error == invalidAddress)
        Serial.println("Invalid NTP server address");
    }
    else {
      Serial.print("Got NTP time: ");
      Serial.println(NTP.getTimeDateString(NTP.getLastNTPSync()));
      ntp_ready = true;
    }
    
  });
  NTP.begin("192.168.0.3", 1, false);
  NTP.setInterval(60);
  NTP.setTimeZone(10);
  
}

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  
  WiFi.begin(ssid, password);
  

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  WiFi.config(ip, gateway, subnet);
  
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  //lcd.clear();
  ///lcd.print("Connected");
  //lcd.setCursor(0,1);
  //lcd.print(WiFi.localIP());
}




void callback(char* topic, byte* payload, unsigned int length) {
  String tmp;
  tmp = String(topic);
  
  tmp.replace("garden/greenhouse/settings/","");
  
  char buf[16] = "";
  int i = 0;
  for (i = 0; i < length; i++) {
    buf[i] = (char)payload[i];
  }
  buf[i] = '\0';
  String pl = String(buf);
  
  if(tmp == "run"){
    //Serial.println("Run: " + pl);
    //run_duration = pl.toInt();
    start_watering();
  }
  
  if(tmp == "interval"){
    //Serial.println("Run: " + pl);
    watering_interval = pl.toInt();
    //start_watering();
  }

  if(tmp == "duration"){
    //Serial.println("Run: " + pl);
    watering_duration = pl.toInt();
    //start_watering();
  }

}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP8266Client")) {
      Serial.println("connected");
      // Once connected, publish an announcement...
      client.publish("garden/greenhouse/status", "online");
      // ... and resubscribe
      client.subscribe("garden/greenhouse/settings/#");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void relay(byte state){
  if(state == 0){
    pinMode(PIN_RELAY, INPUT);
  }else{
    pinMode(PIN_RELAY, OUTPUT);
    digitalWrite(PIN_RELAY,0);  
  }
  
}

void flowPulse(){
  pulse_count++;
  //Serial.println(String(millis(),DEC) + "p");
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Wire.begin(LCD_SDA, LCD_SCL);
  lcd.init();                     
  lcd.backlight();
  lcd.print("WiFi Connecting");


  setup_wifi();
  setup_ntp();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  ArduinoOTA.setHostname("greenhouse");
  
  ArduinoOTA.onStart([]() {
    lcd.clear();
    lcd.print("Updating F/W");
  });
  ArduinoOTA.onEnd([]() {
    lcd.clear();
    lcd.print("Update Complete");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    lcd.setCursor(0,1);
    String tmp;
    float pct = progress;
    pct = 100 * pct / total;
    tmp = String(pct, 0 ) + "%   ";
    char p[16];
    tmp.toCharArray(p, 16);
    lcd.print(p);
  });
  
  ArduinoOTA.begin();

  attachInterrupt(PIN_FLOW, flowPulse, RISING); // use for Ground connection not Vcc connection
  //pinMode(PIN_RELAY, OUTPUT);
  //relay(1);
}


unsigned long next_disp = 0;
unsigned long last_published_pulse_count = 0;
time_t next_water = tmConvert_t(2016,10,01, 10, 30, 00);




void start_watering(){
  state = STATE_WATERING;
  run_duration = watering_duration;
  next_water = now() + watering_interval;
  //lcd.clear();
  //lcd.print("Watering");
  relay(1);
}

void stop_watering(){
  relay(0);
  lcd.clear();
  lcd.print("Watering finish.");
  state = STATE_READY;
}

time_t tmConvert_t(int YYYY, byte MM, byte DD, byte hh, byte mm, byte ss)
{
  tmElements_t tmSet;
  tmSet.Year = YYYY - 1970;
  tmSet.Month = MM;
  tmSet.Day = DD;
  tmSet.Hour = hh;
  tmSet.Minute = mm;
  tmSet.Second = ss;
  return makeTime(tmSet);         //convert to time_t
}


void loop() {
  // put your main code here, to run repeatedly:

  if (!client.connected()) {
    reconnect();
  }
  client.loop();
  ArduinoOTA.handle();

  long t = millis();
  //int t_hr = hour(now());
  //int t_min = minute(now());
  //int t_sec = second(now());
  

  //lcd.print(t_hr);
  //lcd.print(":");
  //lcd.print(t_min);
  //lcd.print(":");
  //lcd.print(t_sec);
  
  if(t > next_disp){
    time_t n = now();
    next_disp = t + 1000;
    lcd.setCursor(0,0);
    
    if(!ntp_ready){
      if(now() > tmConvert_t(2016, 1, 1, 0, 0, 0)){
        ntp_ready = true;
        // start watering in 5 seconds.
        next_water = n + 5;
      }else{
        lcd.print("Waiting for NTP");
        return;  
      }
    }

    if(state == STATE_READY){
      //lcd.print("Now:  " + NTP.getTimeStr()+"  ");  
      //lcd.setCursor(0,1);
      lcd.setCursor(0,0);
      if(next_water > n){
        time_t tdiff = next_water - now();
        lcd.print("Next: " + NTP.getTimeStr(tdiff)+"        ");      
      }else{
        // we need to water!
        start_watering();
      }  
    }
    
    
    //lcd.print(NTP.getTimeStr());
    //lcd.print("      ");
    lcd.setCursor(0,1);
    lcd.print(pulse_count,DEC);
    

    if (last_published_pulse_count != pulse_count) {
      String tmp;
      char buf[16];
      tmp = String(pulse_count);
      tmp.toCharArray(buf, 40);
      client.publish("garden/greenhouse/water_meter", buf);
      last_published_pulse_count = pulse_count;
    }

    if(state == STATE_WATERING){
      lcd.setCursor(0,0);
      lcd.print(pulse_count,DEC);
      lcd.print("      ");
      //lcd.print("Watering        ");
      lcd.setCursor(0,1);
      lcd.print(NTP.getTimeStr(run_duration) + "        ");

      run_duration--;
      if(run_duration <= 0){
        //start_watering();
        stop_watering();        
      }
    }

   
    
  }
}
