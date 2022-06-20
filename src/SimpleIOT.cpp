/*
 * © 2022 Amazon Web Services, Inc. or its affiliates. All Rights Reserved.
 * 
 * SimpleIOT Arduino Client Library
 */
 
 #include "SimpleIOT.h"

SimpleIOT* SimpleIOT::_iot_singleton = NULL;  // Singleton needed for greengrass


#define DELAY_MS_BEFORE_RESTART    2000
#define MAXIMUM_JSON_PAYLOAD_SIZE  1024
#define OP_SET_DATA   "data/set"

#define SIMPLEIOT_APP_TOPIC_PREFIX    "simpleiot_v1/app"
#define SIMPLEIOT_APP_MONITOR_PREFIX  SIMPLEIOT_APP_TOPIC_PREFIX "/monitor"
#define SIMPLEIOT_ADM_TOPIC_PREFIX    "simpleiot_v1/adm"
#define SIMPLEIOT_SYS_TOPIC_PREFIX    "simpleiot_v1/sys"
#define SIMPLEIOT_DIAG_TOPIC_PREFIX   SIMPLEIOT_SYS_TOPIC_PREFIX "/diag"
#define UPDATE_TOPIC_PREFIX  "simpleiot_v1/adm/update"

///////////////////////////////////////////////////////////////

int SimpleIOT::_publish(char* topic, char* payload)
{
  if (this->_withGateway) {
      Serial.println("Publishing via GG");
      this->_greengrass->publish(topic, payload);
  } else {
      Serial.println("Publishing via direct MQTT");
      this->_mqttClient->beginMessage(topic);
      this->_mqttClient->print(payload);
      this->_mqttClient->endMessage();
  }
  return 0;
}

/*
 * payload: {
 *          "action": "set",
 *          "project": "Sunshine,
 *          "serial": "TIE-DEMO01",
 *          "name": "oil_pressure",
 *          "value": "20"
 *          }
 */
int SimpleIOT::_sendRawMessage(const char* op, DynamicJsonDocument payload, SimpleIOTMessageType msgtype)
{
char jsonBuffer[SimpleIOTInternalBufferSize];
char topicBuffer[INTERNAL_TOPIC_BUFFER_SIZE + 1];
  
  serializeJson(payload, jsonBuffer);

  switch(msgtype) {
    case MESSAGE_APP:
        snprintf(topicBuffer, INTERNAL_TOPIC_BUFFER_SIZE, "%s/%s/%s/%s/%s", SIMPLEIOT_APP_TOPIC_PREFIX,
                    op,
                    this->_project,
                    this->_model,
                    this->_serialNumber);
        break;
    case MESSAGE_ADM:
        snprintf(topicBuffer, INTERNAL_TOPIC_BUFFER_SIZE, "%s/%s/%s/%s/%s", SIMPLEIOT_ADM_TOPIC_PREFIX,
                    op,
                    this->_project,
                    this->_model,
                    this->_serialNumber);
        break;
    case MESSAGE_SYS:
        snprintf(topicBuffer, INTERNAL_TOPIC_BUFFER_SIZE, "%s/%s/%s/%s/%s", SIMPLEIOT_SYS_TOPIC_PREFIX,
                    op,
                    this->_project,
                    this->_model,
                    this->_serialNumber);
        break;
  }
  
  #ifdef _DEBUG
    Serial.print("SimpleIOT: Send Topic  : ");
    Serial.println(topicBuffer);
    Serial.print("SimpleIOT: Send Payload: ");
    Serial.println(jsonBuffer);
  #endif

    return this->_publish(topicBuffer, jsonBuffer);
}

/*
 * payload: {
 *          "action": "set",
 *          "project": "Sunshine,
 *          "serial": "TIE-DEMO01",
 *          "name": "oil_pressure",
 *          "value": "20",
 *          "geo_lat": "12.2", // if withGps specified
 *          "geo_lng": "-123.4", // if withGps specified
 *          }
 */
int SimpleIOT::_sendMessage(const char* op, const char* name, const char* value, SimpleIOTMessageType msgtype)
{
DynamicJsonDocument root(SimpleIOTInternalBufferSize);

  root["action"] = "set";
  root["project"] = this->_project;
  root["serial"] = this->_serialNumber;
  root["name"] = name;
  root["value"] = value;

  return _sendRawMessage(op, root, msgtype);
}

// Send a message with lat/lng values
//
int SimpleIOT::_sendMessage(const char* op, const char* name, const char* value, float lat, float lng, SimpleIOTMessageType msgtype)
{
DynamicJsonDocument root(SimpleIOTInternalBufferSize);

  char lat_str[10];
  char lng_str[10];

  root["action"] = "set";
  root["project"] = this->_project;
  root["serial"] = this->_serialNumber;
  root["name"] = name;
  root["value"] = value;

  sprintf(lat_str, "%3.4f", lat);
  root["geo_lat"] = String(lat_str);
  sprintf(lng_str, "%3.4f", lng);
  root["geo_lng"] = String(lng_str);

  return _sendRawMessage(op, root, msgtype);
}


/* This is the static callback passed on to the MQTT Client. We've already assigned us to the
 *  client instance reference. We use that to pass back the data returned back to us by the
 *  MQTT client and unmarshall the data.
 */
void _mqttSubCallback(int messageSize)
{
  char buffer[SimpleIOTInternalBufferSize+1];

  MqttClient* client = SimpleIOT::getClient();
  String topic = client->messageTopic();

  if (client->available()) {
      client->read((uint8_t *) &buffer, (size_t) sizeof(buffer));
      SimpleIOT::getImpl()->_invokeCallback((const char *) topic.c_str(),
                                            (const char *) buffer,
                                            (const unsigned int) strlen(buffer));
  }
}


//////////////////////////////////////////////////////////////////////////

SimpleIOT* SimpleIOT::create(const char* wifiSSID,
                        const char* wifiPassword,
                        const char* iotEndpoint,
                        const char* caPem,
                        const char* certPem,
                        const char* keyPem,
                        bool withGateway)
{
    if (!_iot_singleton) {
    _iot_singleton = new SimpleIOT(wifiSSID, wifiPassword, iotEndpoint, 
                  caPem, certPem, keyPem, withGateway);
  }

  return _iot_singleton;
}


SimpleIOT::SimpleIOT(const char* wifiSSID, const char* wifiPassword, const char* iotEndpoint, 
      const char* caPem, const char* certPem, const char* keyPem, bool withGateway)

{
//  Serial.println("SimpleIOT Constructor");
  this->_ready = false;
  this->_withGateway = withGateway;
  this->_wifiSsid = (char *) wifiSSID;
  this->_wifiPassword = (char *) wifiPassword;
  this->_caPem = (char *) caPem;
  this->_certPem = (char *) certPem;
  this->_keyPem = (char *) keyPem;
  this->_iotEndpoint = (char *) iotEndpoint;
}


SimpleIOT::~SimpleIOT()
{
  if (this->_withGateway) {
      delete this->_greengrass;
  } else {
      delete this->_iot_singleton;
  }
}


void SimpleIOT::config(const char* project, const char* model, const char* serialNumber, const char* fwVersion,
                SimpleIOTReadyCallback onReady,
                SimpleIOTDataCallback onData, 
                SimpleIOTTriggerUpdateCallback onTriggerUpdate,
                SimpleIOTDiagCallback onDiag)
{
  Serial.println("SimpleIOT config");
  this->_project = (char *) project;
  this->_model = (char *) model;
  this->_serialNumber = (char *) serialNumber;
  this->_fwVersion = (char *) fwVersion;
  this->_readyCallback.iot = this;
  this->_readyCallback.callback = onReady;
  this->_dataCallback.iot = this;
  this->_dataCallback.callback = onData;
  this->_triggerUpdateCallback.iot = this;
  this->_triggerUpdateCallback.callback = onTriggerUpdate;
  this->_diagCallback.iot = this;
  this->_diagCallback.callback = onDiag;

  // We subscribe to a monitor topic with our project/model/device settings.
  //

  snprintf(this->_monitorTopicBuffer, INTERNAL_TOPIC_BUFFER_SIZE, 
                "%.25s/%.25s/%.25s/%.25s/#", SIMPLEIOT_APP_MONITOR_PREFIX, project, model, serialNumber);
  snprintf(this->_triggerUpdateTopicBuffer, INTERNAL_TOPIC_BUFFER_SIZE, 
                "%.25s/%.25s/%.25s/%.25s", UPDATE_TOPIC_PREFIX, project, model, serialNumber);

  this->_triggerUpdateTopic = this->_triggerUpdateTopicBuffer;
  this->_monitorTopic = this->_monitorTopicBuffer;

  char thingName[INTERNAL_STATIC_BUFFER_SIZE + 1];
  snprintf(thingName, INTERNAL_STATIC_BUFFER_SIZE, "%.25s-%.25s", model, serialNumber);
  this->_clientId = thingName;

  Serial.println("SimpleIOT: Starting WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(this->_wifiSsid, this->_wifiPassword);

  Serial.print("SimpleIOT: Connecting to Wi-Fi: ");
  Serial.println(this->_wifiSsid);

  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }

  // Configure WiFiClientSecure for IoT
  //
  Serial.println("SimpleIOT: Configuring WiFi for secure access");
  this->_wifiClient = new WiFiClientSecure();
  this->_wifiClient->setCACert(this->_caPem);
  this->_wifiClient->setCertificate(this->_certPem);
  this->_wifiClient->setPrivateKey(this->_keyPem);

  if (this->_withGateway) {
        Serial.println("SimpleIOTGW: Creating Greengrass client");
  
    // NOTE: assume registered Thing Name is the same as the serial number for the device.
    // Otherwise GG discovery will not work.
  
    this->_greengrass = new AWSGreenGrassIoT(this->_iotEndpoint, this->_serialNumber, 
                                             this->_caPem, this->_certPem, this->_keyPem);
    
    Serial.println("SimpleIOTGW: Connecting to GG Core");
  
    while (!this->_greengrass->connectToGG()) {
      Serial.print(".");
      delay(200);
    }
  
    if(!this->_greengrass->isConnected()){
      Serial.println("SimpleIOTGW: TIMEOUT ERROR");
      return;
    }
  } else {
    //
    // Regular MQTT client
    //
    Serial.println("SimpleIOT: Creating MQTT client");

    this->_mqttClient = new MqttClient(*(this->_wifiClient));
    
    // Connect to MQTT endpoint on AWS - NOTE: we should make the port configurable.
    //
    Serial.print("SimpleIOT: Connecting to AWS IOT at endpoint: ");
    Serial.println(this->_iotEndpoint);

    if (!(this->_mqttClient->connect(this->_iotEndpoint, 8883))) {
        Serial.print("ERROR Connecting to MQTT endpoint. Halting: ");
        Serial.println(this->_mqttClient->connectError());
        while (1);
    }

  Serial.println("SimpleIOT: Connected to AWS IOT.");

    this->_mqttClient->onMessage(_mqttSubCallback);


    if(!this->_mqttClient->connected()){
      Serial.println("SimpleIOT: TIMEOUT ERROR");
      return;
    }
  }

  // If a return handler is specified, we subscribe to the monitor topic
  //
  if (onData) {
    Serial.println("SimpleIOT: Subscribing to Monitor Topic: " + String(this->_monitorTopic));
    this->_mqttClient->subscribe(this->_monitorTopic);
  }

  // If there's an onTriggerUpdate handler, we subscribe to it. It gets invoked when there's an 'update'
  // push message coming from the cloud. This can either be done Live when a device is connected to IOT or as
  // a response to a 'update' message with a 'check' op, sent to the server with the current device Serial and Firmware 
  // version. If there is an update, the response will be a doupdate message with information on the payload.
  //
  if (onTriggerUpdate) {
    Serial.println("SimpleIOT: Subscribing to MQTT Trigger Update Topic: " + String(this->_triggerUpdateTopic));
    this->_mqttClient->subscribe(this->_triggerUpdateTopic);
    
  }

  Serial.print("SimpleIOT: AWS IOT connected. IP Address: ");
  Serial.println(WiFi.localIP());

  this->_ready = true;
  if (this->_readyCallback.callback) {
    this->_readyCallback.callback(this, 0, "Ready");
  }
}

void SimpleIOT::_invokeCallback(const char* topic, const char* buffer, const unsigned int buflen)
{
  SimpleIOTType typeValue = IOT_STRING;

  // We parse the buffer as json, then we extract the data coming in, convert it to the data type
  // indicated, and call the right data handler.
  //
  // 
    //  typedef enum {
    //  IOT_INT,
    //  IOT_FLOAT,
    //  IOT_DOUBLE,
    //  IOT_STRING,
    //  IOT_BOOLEAN
    //} SimpleIOTType;

  Serial.print("SimpleIOT: Got callback from MQTT: ");
  Serial.println(topic);
  Serial.println(buffer);

  DynamicJsonDocument jdoc(MAXIMUM_JSON_PAYLOAD_SIZE);
  deserializeJson(jdoc, buffer);

  // Let's check to see if it's an update
  //
  if (strncmp(UPDATE_TOPIC_PREFIX, topic, strlen(UPDATE_TOPIC_PREFIX)) == 0) {
    const char* serial = jdoc["device"];
    const char* version = jdoc["version"];
    const char* payload_url = jdoc["url"];
    const char* md5 = jdoc["md5"];

    bool force = true;

    JsonVariant forceValue = jdoc.getMember("force");
    if (!forceValue.isNull()) {
        force = forceValue.as<bool>();
    }

// TBD: determine update type passed via payload. For now, we default to FIRMWARE
//
//    bool update_type_str = jdoc["type"];
    SimpleIOTUpdateType update_type = UPDATE_FIRMWARE;

    // Now, we check to see if the packet is with us, if the version matches (or exceeds) and whether
    // it has been forced. 

    this->_triggerUpdateCallback.callback(this, version, payload_url, update_type);
  }
  else if (strncmp(SIMPLEIOT_ADM_TOPIC_PREFIX, topic, strlen(SIMPLEIOT_ADM_TOPIC_PREFIX)) == 0) {
    this->_handleAdminRequest(topic, jdoc);
  } else if (strncmp(SIMPLEIOT_DIAG_TOPIC_PREFIX, topic, strlen(SIMPLEIOT_DIAG_TOPIC_PREFIX)) == 0) {
    this->_handleDiagRequest(topic, jdoc);
  } else {
    if (this->_dataCallback.callback) {
      const char* name = jdoc["name"];
      const char* value = jdoc["value"];
      JsonVariant type = jdoc.getMember("type");

      if (!(type.isNull())) {
        const char* typeStr = type.as<const char *>();
        if (strcmp(typeStr, "string") || strcmp(typeStr, "str"))
          typeValue = IOT_STRING;
        else if (strcmp(typeStr, "integer") || strcmp(typeStr, "int"))
          typeValue = IOT_INT;
        else if (strcmp(typeStr, "float"))
          typeValue = IOT_FLOAT;
        else if (strcmp(typeStr, "double"))
          typeValue = IOT_DOUBLE;
        else if (strcmp(typeStr, "boolean") || strcmp(typeStr, "bool"))
          typeValue = IOT_BOOLEAN;
      }
      // Callback for onData. Value is passed as string, but with a typeValue
      // so it can be coerced if needed.
      //
      this->_dataCallback.callback(this, String(name), String(value), typeValue);
    }
  }
  // In either case we should verify that it's addressed for this device and model.
}

// These are internal functions to handle admin and diagnostic requests.
// Items intended to be handled by the SDK are dealt with without going back to the main app.

// Those meant to be handled by the app are passed on to the provided handlers (if there)
// and the results returned.
//
// These are currently placeholders. They will be populated as the SDK functionality is
// extended.
//
void SimpleIOT::_handleAdminRequest(const char* topic, DynamicJsonDocument jdoc)
{
   // *TBD*
}

// NOTE: for diagnostics, certain of the SimpleIOTDiagType values may be performed here in the SDK
// since they wouldn't be required to be handled by the application.
//
// We'll add  
void SimpleIOT::_handleDiagRequest(const char* topic, DynamicJsonDocument jdoc)
{
  if (this->_diagCallback.callback) {
    const char* diagId = jdoc["id"];
    const char* diagData = jdoc["data"];
    int diagType = jdoc["type"];

    const char* result = this->_diagCallback.callback(this, diagId, diagData, (SimpleIOTDiagType) diagType);

    // We assume the data we get back is a JSON string. We would return it verbatim back via 
    // MQTT, with the transaction ID in the topic.

    // *TBD*
  }
}


int SimpleIOT::set(const char* name, const char* value)
{
  return this->_sendMessage(OP_SET_DATA, name, value, MESSAGE_APP);
}

int SimpleIOT::set(const char* name, int value)
{
  char buffer[INTERNAL_STATIC_BUFFER_SIZE + 1];
  snprintf(buffer, INTERNAL_STATIC_BUFFER_SIZE, "%d", value);

  return this->_sendMessage(OP_SET_DATA, name, buffer, MESSAGE_APP);
}

int SimpleIOT::set(const char* name, float value)
{
  char buffer[INTERNAL_STATIC_BUFFER_SIZE + 1];
  snprintf(buffer, INTERNAL_STATIC_BUFFER_SIZE, "%.6f", value);

    return this->_sendMessage(OP_SET_DATA, name, buffer, MESSAGE_APP);
}

int SimpleIOT::set(const char* name, double value)
{
  char buffer[INTERNAL_STATIC_BUFFER_SIZE + 1];
  snprintf(buffer, INTERNAL_STATIC_BUFFER_SIZE, "%.6g", value);

  return this->_sendMessage(OP_SET_DATA, name, buffer, MESSAGE_APP);
}

int SimpleIOT::set(const char* name, boolean value)
{
  return this->_sendMessage(OP_SET_DATA, name, value ? "true" : "false", MESSAGE_APP);
}

int SimpleIOT::set(const char* name, const char* value, float latitude, float longitude)
{
  return this->_sendMessage(OP_SET_DATA, name, value, latitude, longitude, MESSAGE_APP);
}

int SimpleIOT::set(const char* name, int value, float latitude, float longitude)
{
  char buffer[INTERNAL_STATIC_BUFFER_SIZE + 1];
  snprintf(buffer, INTERNAL_STATIC_BUFFER_SIZE, "%d", value);

  return this->_sendMessage(OP_SET_DATA, name, buffer, latitude, longitude, MESSAGE_APP);
}

int SimpleIOT::set(const char* name, float value, float latitude, float longitude)
{
  char buffer[INTERNAL_STATIC_BUFFER_SIZE + 1];
  snprintf(buffer, INTERNAL_STATIC_BUFFER_SIZE, "%.6f", value);

  return this->_sendMessage(OP_SET_DATA, name, buffer, latitude, longitude, MESSAGE_APP);
}

int SimpleIOT::set(const char* name, double value, float latitude, float longitude)
{
  char buffer[INTERNAL_STATIC_BUFFER_SIZE + 1];
  snprintf(buffer, INTERNAL_STATIC_BUFFER_SIZE, "%.6g", value);

  return this->_sendMessage(OP_SET_DATA, name, buffer, latitude, longitude, MESSAGE_APP);
}

int SimpleIOT::set(const char* name, boolean value, float latitude, float longitude)
{
  return this->_sendMessage(OP_SET_DATA, name, value ? "true" : "false", latitude, longitude, MESSAGE_APP);
}


void SimpleIOT::loop(float delayMs)
{
    if (this->_mqttClient) {
        this->_mqttClient->poll();
    }
    if (delayMs > 0) {
        delay(delayMs);
    }
}


// Utility to extract header value from headers
String getHeaderValue(String header, String headerName) {
  return header.substring(strlen(headerName.c_str()));
}

// Based on ESP32 OTA system. This should be abstracted out so it can run on any processor
// 
// NOTE that the URL only works with S3/CloudFront since it downloads via SSL and the HTTP client needs the
// RootCA of the server. We happen to have it as part of OTA connection so it only works with 
// OTA updates hosted on AWS services.
//
// References:
//
// - https://github.com/kurimawxx00/webota-esp32/blob/main/WebOTA.ino
// - https://techtutorialsx.com/2017/11/18/esp32-arduino-https-get-request/
//

void SimpleIOT::performOTA(const char* url, SimpleIOTOTACallback otaCallback)
{
HTTPClient client;

  this->_otaCallback = otaCallback;
  this->_fwUpdateTotalLength = 0;
  this->_fwUpdateCurrentLength = 0;
  this->_fwUpdatePercent = 0;
  
  client.begin(url, this->_caPem); // NOTE: we need to append the ROOT_CA so HTTPS calls can be made
  // Get file, just to check if each reachable
  int resp = client.GET();
  Serial.print("Response: ");
  Serial.println(resp);

  if(resp > 0) {
      // get length of document (is -1 when Server sends no Content-Length header)
      
      this->_fwUpdateTotalLength = client.getSize();
      
      // transfer to local variable
      int len = this->_fwUpdateTotalLength;
      
      // this is required to start firmware update process
      Update.begin(UPDATE_SIZE_UNKNOWN);
      Serial.printf("FW Size: %u\n",this->_fwUpdateTotalLength);
      
      // create buffer for read
      uint8_t buff[128] = { 0 };
      // get tcp stream
      WiFiClient * stream = client.getStreamPtr();
      
      // read all data from server
      Serial.println("Updating firmware...");
      while(client.connected() && (len > 0 || len == -1)) {
           // get available data size
           size_t size = stream->available();
           
           if (size) {
              // read up to 128 byte
              int c = stream->readBytes(buff, ((size > sizeof(buff)) ? sizeof(buff) : size));
              // pass to function
              this->_updateFirmware(buff, c);
              if(len > 0) {
                 len -= c;
              }
           }
           delay(1);
      }
  }else{
    Serial.println("ERROR: Cannot download firmware file");
  }
  client.end();
}


// Update firmware incrementally.
// Buffer is declared to be 128 so chunks of 128 bytes from firmware is written to device until server closes.
//
// We calculate percentage done in integer format and invoke the callback handler only when the value has changed.
// Since the download is only 128 bytes at a time, this cuts down on a lot of callbacks.

void SimpleIOT::_updateFirmware(uint8_t *data, size_t len)
{
  int percent;
  Update.write(data, len);
  this->_fwUpdateCurrentLength += len;

  if (this->_otaCallback != NULL) {
    percent = int((float(this->_fwUpdateCurrentLength) / float(this->_fwUpdateTotalLength)) * 100.0);
    if (percent != this->_fwUpdatePercent) {
       this->_fwUpdatePercent = percent;
       this->_otaCallback(this->_fwUpdateCurrentLength, this->_fwUpdateTotalLength, percent);
    }
  }

  // if current length of written firmware is not equal to total firmware size, repeat
  
  if (this->_fwUpdateCurrentLength != this->_fwUpdateTotalLength) 
     return;

  Update.end(true);
  
  this->_updateReceived(); // We send the update received message to the server so it marks the record properly

  Serial.printf("\nUpdate Success, Total Size: %u\nRebooting...\n", this->_fwUpdateCurrentLength);
  // Restart ESP32 to see changes 
  
  delay(DELAY_MS_BEFORE_RESTART);            // then we wait a little before restarting to let the updateReceived call get through
  ESP.restart();
}

void SimpleIOT::_doUpdate(char* op, bool force)
{
DynamicJsonDocument root(SimpleIOTInternalBufferSize);

  root["project"] = this->_project;
  root["serial"] = this->_serialNumber;
  root["version"] = this->_fwVersion;
  root["op"] = op;
  if (force) {
    root["force"] = true;
  }

  _sendRawMessage(op, root, MESSAGE_ADM);
}

void SimpleIOT::checkForUpdate(bool force)
{
  this->_doUpdate((char *) "check", force);
}

// This will be called internally once 100% of the update has been received.
//
void SimpleIOT::_updateReceived()
{
  this->_doUpdate((char *) "received");
}

// This is an alternative one. It can be used by the app to indicate an update has been installed
// if the download and installation is done manually and not through the performOTA with a callback
// handler. 

void SimpleIOT::updateInstalled()
{
  this->_doUpdate((char *) "installed");
}
