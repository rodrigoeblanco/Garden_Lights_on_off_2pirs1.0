#include "Switch.h"
#include "CallbackFunction.h"


        
//<<constructor>>
Switch::Switch(){
    Serial.println("default constructor called");
}
//Switch::Switch(String alexaInvokeName,unsigned int port){
Switch::Switch(String alexaInvokeName, unsigned int port, CallbackFunction oncb, CallbackFunction offcb){
    uint32_t chipId = ESP.getChipId();
    char uuid[64];
    sprintf_P(uuid, PSTR("38323636-4558-4dda-9188-cda0e6%02x%02x%02x"),
          (uint16_t) ((chipId >> 16) & 0xff),
          (uint16_t) ((chipId >>  8) & 0xff),
          (uint16_t)   chipId        & 0xff);
    
    serial = String(uuid);
    persistent_uuid = "Socket-1_0-" + serial+"-"+ String(port);
        
    device_name = alexaInvokeName;
    localPort = port;
    onCallback = oncb;
    offCallback = offcb;
    
    startWebServer();
}


 
//<<destructor>>
Switch::~Switch(){/*nothing to destruct*/}

//STARTING THE SERVER LOOP
void Switch::serverLoop(){
    if (server != NULL) {
        server->handleClient();
        delay(1);
    }
}
//************************

//STARTING A NEW WEB SERVER
void Switch::startWebServer(){
  server = new ESP8266WebServer(localPort);

  server->on("/", [&]() {
    handleRoot();
  });
 

  server->on("/setup.xml", [&]() {
    handleSetupXml();
  });

  server->on("/upnp/control/basicevent1", [&]() {
    handleUpnpControl();
  });

  server->on("/eventservice.xml", [&]() {
    handleEventservice();
  });

  //server->onNotFound(handleNotFound);
  server->begin();

  Serial.println("WebServer started on port: ");
  Serial.println(localPort);
}
//**********************************

 //DONT KNOW WHAT THIS DOES
void Switch::handleEventservice(){
  Serial.println(" ########## Responding to eventservice.xml ... ########\n");

  String eventservice_xml = "<scpd xmlns=\"urn:Belkin:service-1-0\">"
        "<actionList>"
          "<action>"
            "<name>SetBinaryState</name>"
            "<argumentList>"
              "<argument>"
                "<retval/>"
                "<name>BinaryState</name>"
                "<relatedStateVariable>BinaryState</relatedStateVariable>"
                "<direction>in</direction>"
                "</argument>"
            "</argumentList>"
          "</action>"
          "<action>"
            "<name>GetBinaryState</name>"
            "<argumentList>"
              "<argument>"
                "<retval/>"
                "<name>BinaryState</name>"
                "<relatedStateVariable>BinaryState</relatedStateVariable>"
                "<direction>out</direction>"
                "</argument>"
            "</argumentList>"
          "</action>"
      "</actionList>"
        "<serviceStateTable>"
          "<stateVariable sendEvents=\"yes\">"
            "<name>BinaryState</name>"
            "<dataType>Boolean</dataType>"
            "<defaultValue>0</defaultValue>"
           "</stateVariable>"
           "<stateVariable sendEvents=\"yes\">"
              "<name>level</name>"
              "<dataType>string</dataType>"
              "<defaultValue>0</defaultValue>"
           "</stateVariable>"
        "</serviceStateTable>"
        "</scpd>\r\n"
        "\r\n";
           
    server->send(200, "text/plain", eventservice_xml.c_str());
}
//*****************************************

//SEND TO THE SERVER A CONFIRMATION AFTER AN ON/OFF REQUEST HAPPENS
void Switch::handleUpnpControl(){
  Serial.println("########## Responding to  /upnp/control/basicevent1 ... ##########");      
  
  //for (int x=0; x <= HTTP.args(); x++) {
  //  Serial.println(HTTP.arg(x));
  //}

  String request = server->arg(0);      
  Serial.print("request:");
  Serial.println(request);

   if(request.indexOf("SetBinaryState") >= 0) {
    if(request.indexOf("<BinaryState>1</BinaryState>") >= 0) {
        Serial.println("Got Turn on request");
         switchStatus = true;
         onCallback();

        sendRelayState();
    }

    if(request.indexOf("<BinaryState>0</BinaryState>") >= 0) {
        Serial.println("Got Turn off request");
        switchStatus = false;
        offCallback();
        
        sendRelayState();
    }
  }

  if(request.indexOf("GetBinaryState") >= 0) {
    Serial.println("Got binary state request");
    sendRelayState(); //callback to send the state of the relays ON/OFF
    }
  
  server->send(200, "text/plain", ""); //Sends ON/OFF request to the server 
  delay(50); //A small delay to improve the status update
  server->send(200, "text/plain", ""); //Resend the same information. This made the status update faster
}
//*******************

//I GUESS THIS MAKES ALEXA TELL YOU TO DISCOVER DEVICES
void Switch::handleRoot(){
  server->send(200, "text/plain", "You should tell Alexa to discover devices");
}
//***************************************

//SETUP DEVICES WHEN ADDING DEVICES IN ALEXA APP OR VOICE DEVICE
void Switch::handleSetupXml(){
  Serial.println(" ########## Responding to setup.xml ... ########\n");
  
  IPAddress localIP = WiFi.localIP();
  char s[16];
  sprintf(s, "%d.%d.%d.%d", localIP[0], localIP[1], localIP[2], localIP[3]);
  
 
  String setup_xml = "<?xml version=\"1.0\"?>"
            "<root>"
             "<device>"
                "<deviceType>urn:Belkin:device:controllee:1</deviceType>"
                "<friendlyName>"+ device_name +"</friendlyName>"
                "<manufacturer>Belkin International Inc.</manufacturer>"
                "<modelName>Socket</modelName>"
                "<modelNumber>3.1415</modelNumber>"
                "<modelDescription>Belkin Plugin Socket 1.0</modelDescription>\r\n"
                "<UDN>uuid:"+ persistent_uuid +"</UDN>"
                "<serialNumber>221517K0101769</serialNumber>"
                "<binaryState>0</binaryState>" 
                "<serviceList>"
                  "<service>"
                      "<serviceType>urn:Belkin:service:basicevent:1</serviceType>"
                      "<serviceId>urn:Belkin:serviceId:basicevent1</serviceId>"
                      "<controlURL>/upnp/control/basicevent1</controlURL>"
                      "<eventSubURL>/upnp/event/basicevent1</eventSubURL>"
                      "<SCPDURL>/eventservice.xml</SCPDURL>"
                  "</service>"
              "</serviceList>" 
              "</device>"
            "</root>\r\n"
            "\r\n";
        
    server->send(200, "text/xml", setup_xml.c_str());
    
    Serial.print("Sending :");
    Serial.println(setup_xml);
}

String Switch::getAlexaInvokeName() {
    return device_name;
}
//****************************************

//SENDS THE STATE OF THE RELAYS ON/OFF TO THE SERVER FOR ALEXA TO KNOW
void Switch::sendRelayState() {
  String body = 
      "<s:Envelope xmlns:s=\"http://schemas.xmlsoap.org/soap/envelope/\" s:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\"><s:Body>\r\n"
      "<u:GetBinaryStateResponse xmlns:u=\"urn:Belkin:service:basicevent:1\">\r\n"
      "<BinaryState>";
      
  body += (switchStatus ? "1" : "0");
  
  body += "</BinaryState>\r\n"
      "</u:GetBinaryStateResponse>\r\n"
      "</s:Body> </s:Envelope>\r\n";
  
  server->send(200, "text/xml", body.c_str()); //Updates the status of the relays ON/OFF to the server
  delay(50); //A small delay to improve the status update  
  server->send(200, "text/xml", body.c_str()); //Resend the same information. This made the status update faster

  Serial.print("Sending :");
  Serial.println(body);
}
//************************************

//SEARCH RESPONSE WHEN SEARCHING FOR DEVICES IN ALEXA
void Switch::respondToSearch(IPAddress& senderIP, unsigned int senderPort) {
  Serial.println("");
  Serial.print("Sending response to ");
  Serial.println(senderIP);
  Serial.print("Port : ");
  Serial.println(senderPort);

  IPAddress localIP = WiFi.localIP();
  char s[16];
  sprintf(s, "%d.%d.%d.%d", localIP[0], localIP[1], localIP[2], localIP[3]);

  
 String response = 
       "HTTP/1.1 200 OK\r\n"
       "CACHE-CONTROL: max-age=86400\r\n"
       "DATE: Sat, 26 Nov 2016 04:56:29 GMT\r\n"
       "EXT:\r\n"
       "LOCATION: http://" + String(s) + ":" + String(localPort) + "/setup.xml\r\n"
       "OPT: \"http://schemas.upnp.org/upnp/1/0/\"; ns=01\r\n"
       "01-NLS: b9200ebb-736d-4b93-bf03-835149d13983\r\n"
       "SERVER: Unspecified, UPnP/1.0, Unspecified\r\n"
       "ST: urn:Belkin:device:**\r\n"
       "USN: uuid:" + persistent_uuid + "::urn:Belkin:device:**\r\n"
       "X-User-Agent: redsonic\r\n\r\n";

  UDP.beginPacket(senderIP, senderPort);
  UDP.write(response.c_str());
  UDP.endPacket();
  /* add yield to fix UDP sending response. For more informations : https://www.tabsoverspaces.com/233359-udp-packets-not-sent-from-esp-8266-solved */
  yield();      

   Serial.println("Response sent !");
}
//*******************************

//PIR TRIGGERED TIMER/DELAY OFF CALLBACKS
//Send ON request update for relay1 
void Switch::relay1Check1(){
  Serial.println("Got Turn on request");
         switchStatus = true;
         onCallback();

        sendRelayState();
}

//Send ON request update for relay2 
void Switch::relay2Check1(){
  Serial.println("Got Turn on request");
         switchStatus = true;
         onCallback();

        sendRelayState();
}

//Send ON request update for relay3 
void Switch::relay3Check1(){
  Serial.println("Got Turn on request");
         switchStatus = true;
         onCallback();

        sendRelayState();
}

//Send ON request update for relay4
void Switch::relay4Check1(){
  Serial.println("Got Turn on request");
         switchStatus = true;
         onCallback();

        sendRelayState();
}

//Send OFF request update for relay1
void Switch::relay1Check(){
  Serial.println("Got Turn off request");
        switchStatus = false;
        offCallback();
        
        sendRelayState();
}

//Send OFF request update for relay2
void Switch::relay2Check(){
  Serial.println("Got Turn off request");
        switchStatus = false;
        offCallback();
        
        sendRelayState();
}

//Send OFF request update for relay3
void Switch::relay3Check(){
  Serial.println("Got Turn off request");
        switchStatus = false;
        offCallback();
        
        sendRelayState();
}

//Send OFF request update for relay4
void Switch::relay4Check(){
  Serial.println("Got Turn off request");
        switchStatus = false;
        offCallback();
        
        sendRelayState();
}
//-------------------
//END OF PROGRAM
