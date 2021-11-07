#include <NTPClient.h>
#include <WiFiUdp.h>
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "ntp1.inrim.it", 3600, 60000);

#include <EmonLib.h>                   // Include Emon Library
EnergyMonitor emon1;                   // Create an instance

#include <EspMQTTClient.h>
EspMQTTClient client(
  "FEDDYNVENTOR88",
  "12345678",
  "10.0.1.2",  // MQTT Broker server ip
  "",   // Can be omitted if not needed
  "",   // Can be omitted if not needed
  "homesp8266",     // Client name that uniquely identify your device
  1883              // The MQTT port, default to 1883. this line can be omitted
);


unsigned int lastSent = 0;
int sendInterval = 2000;

unsigned int clockMec = 0; //conserva millis dell'ultimo tick del secondo, vedi riga
int turnOnHour = 6;
int turnOnMin = 30;
int turnOffHour = 23;
int turnOffMin = 30;

bool goPowerOff = true;
int currentBottomThreshold = 1;  // soglia sotto la quale è sicuro procedere con lo shutdown in sicurezza

char daynot[7] = {'1','1','1','1','1','1','1'};
bool outputState = false;

int h = 0;
int m = 0;
int s = 0;
int dow = 0;

// Tare value for current measurements
// NOTE: Varies among chips and supply circuits
int currentCalibZero = 0;
double Irms = 0;

void setup()
{
  Serial.begin(115200);

  emon1.current(A0, 30);             // Current: input pin, calibration.

  client.enableDebuggingMessages(); // Enable debugging messages sent to serial output
  client.enableHTTPWebUpdater(); // Enable the web updater. User and password default to values of MQTTUsername and MQTTPassword. These can be overrited with enableHTTPWebUpdater("user", "password").
  //client.enableLastWillMessage("TestClient/lastwill", "I am going offline");  // You can activate the retain flag by setting the third parameter to true

  pinMode(4,OUTPUT);  //onboard
  pinMode(13,OUTPUT); //external (up over VCC pin)
  digitalWrite(4, LOW);
  digitalWrite(13, LOW);
}

void onConnectionEstablished()
{
  
  timeClient.begin();
  timeClient.update();
  h = timeClient.getHours();
  m = timeClient.getMinutes();
  s = timeClient.getSeconds();
  dow = timeClient.getDay();

  Serial.println("NTP Gathered = "+String(h)+":"+String(m)+":"+String(s)+" #"+String(dow));
  
  // Subscribe to "mytopic/test" and display received message to Serial
  client.subscribe("homesp/current/interval", [](const String & payload) {
    sendInterval = (payload.toInt());
  });
  client.subscribe("homesp/current/zero", [](const String & payload) {
    currentCalibZero = (payload.toInt());
  });
  client.subscribe("homesp/current/threshold", [](const String & payload) {
    currentBottomThreshold = (payload.toInt());
  });

  client.subscribe("homesp/timer/on/hour", [](const String & payload) {
    turnOnHour = (payload.toInt());
  });
  client.subscribe("homesp/timer/on/min", [](const String & payload) {
    turnOnMin = (payload.toInt());
  });
  client.subscribe("homesp/timer/off/hour", [](const String & payload) {
    turnOffHour = (payload.toInt());
  });
  client.subscribe("homesp/timer/off/min", [](const String & payload) {
    turnOffMin = (payload.toInt());
  });
  client.subscribe("homesp/timer/off/go", [](const String & payload) {
    if (payload!="1") Serial.println("HOLDING POWER OFF");
    goPowerOff = payload=="1";
  });
  client.subscribe("homesp/timer/days", [](const String & payload) {
    strcpy(daynot, payload.c_str());
    for (int i=0; i<7; i++){
      Serial.print(daynot[i]);
    }
    Serial.println(" GIORNI NO");
  });
  
  client.subscribe("homesp/out/set", [](const String & payload) {
    outputState= (payload=="1");
    digitalWrite(4, payload=="1");
    client.publish("homesp/out", digitalRead(4)?"1":"0");
  });
  client.subscribe("homesp/out/get", [](const String & payload) {
    client.publish("homesp/out", digitalRead(4)?"1":"0");
  });

  client.subscribe("homesp/out2/set", [](const String & payload) {
    outputState= (payload=="1");
    digitalWrite(13, payload=="1");
    client.publish("homesp/out2", digitalRead(13)?"1":"0");
  });
  client.subscribe("homesp/out2/get", [](const String & payload) {
    client.publish("homesp/out2", digitalRead(13)?"1":"0");
  });
  client.subscribe("homesp/out2/blink", [](const String & payload) {
    digitalWrite(13, HIGH);
    delay(200);
    digitalWrite(13, LOW);
  });

  // Subscribe to "mytopic/wildcardtest/#" and display received message to Serial
  //client.subscribe("currentesp/wildcardtest/#", [](const String & topic, const String & payload) {
  //  Serial.println("(From wildcard) topic: " + topic + ", payload: " + payload);
  //});

}

void loop()
{
  client.loop();
  
  if (millis() - clockMec > 999) {
  
    s++;
    if (s>=60){
      m++;
      s=0;
    }
    if (m>=60){
      h++;
      m=0;
    }
    if (h>=24){
      s=0;m=0;h=0;
      dow++;
    }
    if (dow==8){
      dow=1;
    }

    if (m==0 && h==3 && s==0){
      timeClient.update();
    }

    Serial.print(m>=turnOffMin);
    Serial.print(h>=turnOffHour);
    Serial.print(goPowerOff);
    Serial.print(Irms<currentBottomThreshold);
    Serial.print(((char)daynot[dow-1])=='1');
    Serial.println(" Timer ongoing = "+String(h)+":"+String(m)+":"+String(s));
    
    if (m>=turnOffMin && h==turnOffHour && goPowerOff && Irms<currentBottomThreshold && ((char)daynot[dow-1])=='1'){
      if (!outputState){  // è la prima volta che entri qui se prima era LOW
        digitalWrite(4,HIGH);
        client.publish("homesp/out", "1"); 
        goPowerOff = false;
        client.publish("homesp/timer/off/go", "0");
        outputState = true;
      }
    }
    if (m==turnOnMin && h==turnOnHour){
      digitalWrite(4,LOW);
      outputState = false;
      client.publish("homesp/out", "0");
      goPowerOff = true;
      client.publish("homesp/timer/off/go", "1");
    }

    clockMec = millis();
  }
  if (millis() - lastSent > sendInterval) {

    Irms = emon1.calcIrms(1480);  // Calculate Irms only
    Irms = (Irms<=(currentCalibZero/220.0))?0:Irms*220.0-currentCalibZero;
    
    //Serial.println( Irms );         // Apparent power
    
    if (!outputState)   // pin spento, vai
      client.publish("homesp/wattage", double2string(Irms,1));
/*
    bool prevState = goPowerOff;
    if (Irms>currentBottomThreshold)
      goPowerOff=false;
    else
      goPowerOff=true;
    if (goPowerOff!=prevState)
      client.publish("homesp/timer/off/go", (goPowerOff)?"1":"0");
*/
    lastSent = millis();
  }
}



String double2string(double n, int ndec) {
    String r = "";

    int v = n;
    r += v;     // whole number part
    r += '.';   // decimal point
    int i;
    for (i=0;i<ndec;i++) {
        // iterate through each decimal digit for 0..ndec 
        n -= v;
        n *= 10; 
        v = n;
        r += v;
    }

    return r;
}
