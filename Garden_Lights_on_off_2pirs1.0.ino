//************ LIBRARIES ***********
#include <millisDelay.h>  //Library for delays. You might need to install it manually with a zip file
//https://www.instructables.com/id/Coding-Timers-and-Delays-in-Arduino/
//Step 4
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WiFiUdp.h>
#include <functional>
#include "switch.h"
#include "UpnpBroadcastResponder.h"
#include "CallbackFunction.h"
//----------------------------

//*****************GLOBAL SCOPE DECLARATION *************

// Prototypes
boolean connectWifi();

//Callback Functions from CallbackFunction.h
//CallbackFunction onCallback;
//CallbackFunction offCallback;

// ON/OFF callbacks
void waterPlantsOn(); 
void waterPlantsOff(); 
void gardenLightsOn();
void gardenLightsOff();
void patioLightsOn();
void patioLightsOff();
void crystalLightsOn();
void crystalLightsOff();
//Subroutines/callbacks to read the Photoresistor and PIRs values
void Photoread();
void Piread();

//Photoresistor and PIRs vars
int presistor = A0;   //Photoresistor at analog pin read A0
int valueph = 0;      //Store value from photoresistor (0 - 1023)
int valuepir = 0;     //Sotre value of PIR (0 - 1)
int valuepir2 = 0;    //Store value of PIR2 (0 -1)

//Vars For the Delays
//You can change DELAY_TIME and DELAY_VALVE values as you please
unsigned long DELAY_TIME = 250000;  //4.2  min delay for relay2 and (relay3 + relay4) to turn OFF
                                    //Garden Lights and (conservatory lights + patio lights) turn OFF after delay when PIR triggered 
unsigned long DELAY_VALVE = 800000; //13.3 min delay for irrigation valve to turn OFF
//Initializing delays vars
unsigned long delayStart = 0;       //The time that (relay3 + relay4) delay started after turning ON
unsigned long delayValve = 0;       //The time the irrigation valve delay started after turning ON
unsigned long delayPir2 = 0;        //The time that relay2 delay started after ON (triggered by PIR2)
boolean delayRunning = false;       //True if still waiting for (relay3 + relay4) delay to finish
boolean delayValveRunning = false;  //True if still waiting for irrigation valve delay to finish
boolean delayPir2Running = false;   //True if still waiting for relay2 delay to finish when triggered by PIR2 

//Status or control trackers vars
//Initialize ON/OFF status trackers for Alexa
boolean isWaterPlantsOn = false;  
boolean isGardenLightsOn = false;
boolean isPatioLightsOn = false;
boolean isCrystalLightsOn = false;
boolean switchStatus = false;
//Init ON/OFF status trackers for PIR and PIR2 triggers
boolean relay34stat = false;    //Init (conservatory + patio lights) state tracker if PIR status High and photoresistor value is <200 False/OFF
boolean relay1stat = false;     //Init irrigation valve state tracker set boolean to False/OFF
boolean relay2stat = false;     //Init garden lights state tracker set boolean to False/OFF
boolean relay3stat = false;     //Init patio lights state tracker set boolean to False/OFF
boolean relay4stat = false;     //Init conservatory lights state tracker set boolean to False/OFF
boolean relay2pirstat = false;  //Init garden lights state tracker if PIR2 status High and photoresistor value is < 200 False/OFF


// Change this before you flash
const char* ssid = "NETGEAR20_EXT";     //SSID or name of your wifi network 
const char* password = "fuzzymango260"; //Your Wifi network password

boolean wifiConnected = false;  //Init Wifi connection status tracker set boolean to False because Wifi isn't connected


//Initializing the names for the switches to NULL
//Switches are for Alexa to recognize 
//You can change the part after the * (i.e *water can change to *valve)
//If you change the switch name make sure you change it on the setup definition as well
UpnpBroadcastResponder upnpBroadcastResponder;

Switch *water = NULL;
Switch *garden = NULL;
Switch *patio = NULL;
Switch *crystal = NULL;
//---------------------------------

//************* SETUP I/O PINS & OTHER VARIABLES*****************
void setup()
{
 Serial.begin(9600);     //Setup serial
 pinMode (A0, INPUT);   //Set A0 as an INPUT for the PHOTORESISTOR VALUE
 pinMode (D1, OUTPUT);  //Set D1 as an OUTPUT for the IRRIGATION VALVE to water the plants
 pinMode (D2, OUTPUT);  //Set D2 as an OUTPUT for the GARDEN LIGHTS
 pinMode (D3, OUTPUT);  //Set D3 as an OUTPUT for the GARDEN LIGHTS2
 pinMode (D4, OUTPUT);  //Set D4 as an OUTPUT for the CONSERVATORY LIGHTS
 pinMode (D5, INPUT);   //Set D5 as an INPUT for the SECOND PIR or PIR2
 pinMode (D6, OUTPUT);  //Set D6 as an OUTPUT to RESET ESP8266 when there is NO WIFI CONNECTION
 pinMode (D7, OUTPUT);  //Set D7 as an OUTPUT for NO WIFI NETWORK CONNECTION LED
 pinMode (D8, INPUT);   //Set D8 as an INPUT for the PIR
 //Initializing output values to HIGH
 digitalWrite (D1,HIGH);  //Init D1 HIGH relay1 is OFF
 digitalWrite (D2,HIGH);  //Init D2 HIGH relay2 is OFF
 digitalWrite (D3,HIGH);  //Init D3 HIGH relay3 is OFF
 digitalWrite (D4,HIGH);  //Init D4 HIGH relay4 is OFF
 digitalWrite (D6,HIGH);  //Init D6 HIGH no reset pulse
 digitalWrite (D7,HIGH);  //Init D7 HIGH Pilot LED is OFF
 //Initializing status trackers to False or OFF
 //For Alexa
 isWaterPlantsOn = false;
 isGardenLightsOn = false;
 isPatioLightsOn = false;
 isCrystalLightsOn = false;
 switchStatus = false;
 //For PIR and PIR2 triggers
 relay34stat = false;     //Relay34 status init for garden lgihts2 + conservatory lights
 relay1stat = false;      //Relay1 status init for irrigation valve
 relay2stat = false;      //Relay2 status init for garden lights
 relay3stat = false;      //Relay3 status init for garden lights2
 relay4stat = false;      //Relay4 status init for conservatory lights
 relay2pirstat = false;   //Relay 2 status init for garden lights when PIR2 triggers and photo value < 200
 //Initializing delays
 delayStart = millis();   //Timer for relay34 init conservatory lights
 delayRunning = true;     //Timer for relay34 running conservatory lights
 delayValve = millis();   //Timer for relay1 init irrigation valve
 delayValveRunning = true;//Timer for relay1 running irrigation valve
 delayPir2 = millis();    //Timer for relay2 when triggered by PIR2 init Garden Lights
 delayPir2Running = true; //Timer for relay2 when triggered by PIR2 running Garden Lights

 //Callbacks Functions
 //onCallback = oncb;
 //offCallback = offcb;
 
 
   
  // Initialise wifi connection
  wifiConnected = connectWifi(); //No Wifi connection because WifiConnected init value = false
  
  if(wifiConnected){
    upnpBroadcastResponder.beginUdpMulticast();
    
    // Define your switches here. Max 14 (you can add or remove switches)
    // Format: Alexa invocation name, local port no, on callback, off callback
    //you can change water, water plants, waterPlants, waterPlants (i.e valve = new Switch ("valve", 80, valveOn, valveOff);)
    water = new Switch("water plants", 80, waterPlantsOn, waterPlantsOff); 
    garden = new Switch("garden lights", 81, gardenLightsOn, gardenLightsOff); 
    patio = new Switch("patio lights", 82, patioLightsOn, patioLightsOff); 
    crystal = new Switch("crystal lights", 83, crystalLightsOn, crystalLightsOff); 
    
    Serial.println("Adding switches upnp broadcast responder");
    //Make sure these match to the Switches you first declared on the global scope
    //If you change the names you have to change the part after the * (i.e *water change to *valve)
    upnpBroadcastResponder.addDevice(*water);
    upnpBroadcastResponder.addDevice(*garden);
    upnpBroadcastResponder.addDevice(*patio);
    upnpBroadcastResponder.addDevice(*crystal);
  }
}
//------ END OF SETUP ------------------

//********* START OF LOOP ***************
 
void loop()
{
	 if(wifiConnected){
     upnpBroadcastResponder.serverLoop();
      
      water->serverLoop();
      garden->serverLoop();
      patio->serverLoop();
      crystal->serverLoop();
      	 
	 }
    if(relay1stat == true){   //Security loop for valve to turn off automatically after timer
      if (delayValveRunning && ((millis() - delayValve) >= DELAY_VALVE)){ //If Valve timer ends 
        delayValve += DELAY_VALVE; //Prevents drift in the delays
        waterPlantsOff();          //Turn relay1 (valve) off
        water->relay1Check();      //Sends an update of the switch status OFF to the server 
      }
    }
    Piread();   //PIR reading subroutine callback
}
//----------- END OF LOOP ----------


//************ START OF CALLBACKS OR SUBROUTINES ********
//Relay1 Irrigation valve or water plants ON/OFF callbacks
void waterPlantsOn() {
    Serial.println("VALVE ON (RELAY1)...");
    digitalWrite (D1,LOW);
    relay1stat = true;   // Valve state tracker set boolean to True valve on
    delayValve = millis(); //Restart timer
    delayValveRunning = true; //Timer state tracker set boolean to True
    Serial.println(relay1stat);
    bool isWaterPlantsOn = true;
    Serial.println(isWaterPlantsOn);
}

void waterPlantsOff() {
    Serial.println("VALVE OFF (RELAY1) ...");
    digitalWrite (D1,HIGH);
    relay1stat = false;   // Valve state tracker set boolean to False valve off
    Serial.println(relay1stat);
    isWaterPlantsOn = false;
    Serial.println(isWaterPlantsOn);
}
//************************

//Relay2 Garden Lights ON/OFF callbacks
void gardenLightsOn() {
    Serial.println("GARDEN LIGHTS ON (RELAY2) ...");
    digitalWrite (D2,LOW);
    relay2stat = true;  //Garden lights state tracker set boolean to True lights on
    Serial.println(relay2stat);
    isGardenLightsOn = true;
    Serial.println(isGardenLightsOn); 
}

void gardenLightsOff() {
    Serial.println("GARDEN LIGHTS OFF (RELAY2)...");
    digitalWrite(D2,HIGH);
    relay2stat = false;  //Garden lights state tracker set boolean to False lights off
    Serial.println(relay2stat);
    isGardenLightsOn = false;
    Serial.println(isGardenLightsOn);  
}
//*************************

//Relay3 Patio Lights ON/OFF callbacks
void patioLightsOn() {
    Serial.println("PATIO LIGHTS ON (RELAY3) ...");
    digitalWrite (D3,LOW);
    relay3stat = true;  //Patio Lights state tracker set boolean to True lights on
    Serial.println(relay3stat);
    isPatioLightsOn = true;
    Serial.println(isPatioLightsOn); 
}

void patioLightsOff() {
    Serial.println("PATIO LIGTHS OFF (RELAY3) ...");
    digitalWrite (D3,HIGH);
    relay3stat = false;  //Patio lights state tracker set boolean to False lights off
    Serial.println(relay3stat);
    isPatioLightsOn = false;
    Serial.println(isPatioLightsOn);   
}
//**********************

//Relay4 Conservatory lights ON/OFF callbacks
void crystalLightsOn() {
    Serial.println("CONSERVATORY LIGHTS ON (RELAY4) ...");
    digitalWrite (D4,LOW);
    relay4stat = true;  //Conservatory lights state tracker set boolean to True
    Serial.println(relay4stat);
    isCrystalLightsOn = true;
    Serial.println(isCrystalLightsOn); 
}

void crystalLightsOff() {
  Serial.println("CONSERVATORY LIGHTS OFF (RELAY4) ...");
  digitalWrite(D4,HIGH);
  relay4stat = false;  //Conservatory lights state tracker set boolean to False lights off
  Serial.println(relay4stat);
  isCrystalLightsOn = false;
  Serial.println(isCrystalLightsOn);   
}
//**********************

// connect to wifi – returns true if successful or false if not
boolean connectWifi(){
  boolean state = true;
  int i = 0;
  
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.println("");
  Serial.println("Connecting to WiFi");

  // Wait for connection
  Serial.print("Connecting ...");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (i > 10){
      state = false;
      break;
    }
    i++;
  }
  
  if (state){
    Serial.println("");
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
    digitalWrite(D7,LOW); //Turn off Pilot LED on D7 if connection successful
  }
  else {
    Serial.println("");
    Serial.println("Connection failed.");
    delay (400);
    digitalWrite(D6,LOW); //RESET if connection fails
  }
  
  return state;
}
//************************


//PIR read subroutine
void Piread(){
  valuepir = digitalRead(D8);   //Read PIR status and store it in valuepir
  valuepir2 = digitalRead(D5);  //Read PIR2 status and store it in valuepir2
  if ((valuepir == HIGH) || (valuepir2 == HIGH)){
     Serial.print("PIR VALUE = ");
     Serial.println(valuepir);    //Print PIR status on the serial monitor
     Serial.print("PIR2 VALUE = ");
     Serial.println(valuepir2);   //Print PIR2 status on the serial monitor
     Photoread ();    //Call Photoresistor read subroutine to check if there's enough light in the room
  }
  if ((valuepir == LOW) && (relay34stat == true)){  //TURN OFF if lights turned on with PIR & Photoresistor
    if (delayRunning && ((millis() - delayStart) >= DELAY_TIME)){
      delayStart += DELAY_TIME; //Prevents drift in the delays
      patioLightsOff();
      crystalLightsOff();
      relay34stat = false; // Ligths state tracker set boolean to False
      Serial.println("GROUP RELAY34 TRACKER OFF");
      Serial.println(relay34stat); //Print tracker status on the serial monitor
      patio->relay3Check();   //Sends an update of the switch status OFF to the server
      crystal->relay4Check(); //Sends an update of the switch status OFF to the server
      }
  }
  if ((valuepir2 == LOW) && (relay2pirstat == true)){ //TURN OFF if lights turned on with PIR2 & Photoresistor
    if (delayPir2Running && ((millis() - delayPir2) >= DELAY_TIME)){
    gardenLightsOff();   //Turn Garden Lights off
    relay2pirstat = false;  //Garden Lights state tracker when PIR2 triggered and photo < 200 set boolean to False
    Serial.println("GARDEN LIGHTS PIR2 TRIGGERED TRACKER OFF");
    Serial.println(relay2pirstat); //Print tracker status on the serial monitor
    garden->relay2Check();         //Sends an update of the switch status ON/OFF to the server
    }          
  }
}
//*************************

//Photoresistor Read subroutine
void Photoread(){  
  valueph = analogRead(presistor);  //Read the Photoresistor value
  Serial.println(valueph);          //Print the Photoresistor value
  if ((valueph < 200) && (valuepir == HIGH) && (relay34stat == false)){ 
    //You can change the 200 for another value suitable for the light in your room (0 - 1024)
    patioLightsOn();             //Turn Garden Lights2 on
    crystalLightsOn();             //Turn Conservatory Lights on
    relay34stat = true;     // Ligths state tracker set boolean to true
    delayStart = millis();  //Restarting timer or delay counter
    delayRunning = true;    //Timer or delay running tracker set to true
    Serial.println("GROUP RELAY34 TRACKER ON");
    Serial.println(relay34stat); //Print tracker status on the serial monitor
    patio->relay3Check1();    //Sends an update of the switch status ON to the server
    crystal->relay4Check1();  //Sends an update of the switch status ON to the server
  }
  if ((valueph <200) && (valuepir2 == HIGH) && (relay2pirstat == false)){
     //You can change the 200 for another value suitable for the light in your room (0 - 1024)
    gardenLightsOn();               //Turn Garden Lights on
    relay2pirstat = true;     //Garden Lights state tracker set boolean to true
    delayPir2 = millis();     //Restarting timer or delay counter
    delayPir2Running = true;  //Timer running tracker set to true 
    Serial.println("PIR2 TRIGGERED GARDEN LIGHTS TRACKER ON");
    Serial.println(relay2pirstat);  //Print tracker status on the serial monitor
    garden->relay2Check1();         //Sends an update of the switch status ON to the server
  }
}
//*****************************
//-------------END OF PROGRAM -----------*/
