/* 
 *  © 2022 Amazon Web Services, Inc. or its affiliates. All Rights Reserved.
 *  
 *  SimpleIOT Arduino client library.
 *  
 *  Dependency on MQTTClient library from: https://github.com/256dpi/arduino-mqtt
 *  and on ArduinoJson from https://arduinojson.org/
 *
 *  Also on AWSGreenGrassIOT, but that library doesn't seem to be available publicly
 *  so it has to be installed separately from:
 *    https://github.com/aws-samples/arduino-aws-greengrass-iot.git
 *  
 *  Search in the libraries for MQTT and install install the latest by Joel Gaehwiler.
 *  Also, search for ArduinoJSON.
 */

#ifndef __SIMPLEIOT_H__
#define __SIMPLEIOT_H__

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ssl_client.h>
#include <ArduinoMqttClient.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <AWSGreenGrassIoT.h>


#define INTERNAL_STATIC_BUFFER_SIZE 100
#define INTERNAL_TOPIC_BUFFER_SIZE  200

#define _DEBUG 1

class SimpleIOT; // forward decl

const size_t SimpleIOTInternalBufferSize = 1024; // How many bytes to allocate for internal MQTT and JSON buffers

// There are three classes of messages: 
//
//  - APP: Application, for values having to do with the application itself. This way, APP messages
//         can be redirected to other data stores if there is a regulatory need for it.
//  - ADM: Admin, for items like provisioning, diagnostics, and device management.
//  - SYS: System, for device-related data like heartbeat, battery level, etc.

typedef enum {
  MESSAGE_APP,
  MESSAGE_ADM,
  MESSAGE_SYS
} SimpleIOTMessageType;

typedef enum {
  UPDATE_FIRMWARE,
  UPDATE_CONFIG,
  UPDATE_FILE,
  UPDATE_TEST
} SimpleIOTUpdateType;


typedef enum {
  IOT_HEARTBEAT,
  IOT_RUNDIAG,
  IOT_BATTERYLEVEL,
  IOT_RESTART,
  IOT_SHUTDOWN,
  IOT_RESET,
  IOT_FACTORYRESET,
  IOT_RESETKEY,
  IOT_SAVESTATE,
  IOT_CLEARSTATE,
  IOT_RETURNSTATE,
  IOT_CUSTOM,
} SimpleIOTDiagType;


typedef enum {
  IOT_INT,
  IOT_FLOAT,
  IOT_DOUBLE,
  IOT_STRING,
  IOT_BOOLEAN
} SimpleIOTType;

// Callback handler signatures
//

// Called when IOT connection has been established and everything is ready to go
//
typedef void (*SimpleIOTReadyCallback)(SimpleIOT *iot,
                    int status,
                    String message);

// When a data value is modified from the cloud, it is sent here
//
typedef void (*SimpleIOTDataCallback)(SimpleIOT *iot,
                    String name,
                    String value,
                    SimpleIOTType type);

// When an update request is received, this is called with the version, URL of payload
// and an optional update type.
//
typedef void (*SimpleIOTTriggerUpdateCallback)(SimpleIOT *iot,
                    String version,
                    String downloadUrl,
                    SimpleIOTUpdateType updateType);

// If provided, this function will be called with the progress of the update
// download.
//
typedef void (*SimpleIOTOTACallback)(int currentDownload,
                    int totalDownload,
                    int percent);

// Called when a diagnostic request is received from the cloud.
//
typedef const char* (*SimpleIOTDiagCallback)(SimpleIOT *iot,
                    String diagId,
                    String data,
                    SimpleIOTDiagType diagType);

// Internal structs to use for calling back handlers. We keep a pointer to the SimpleIOT instance
// in place so we can pass it back to C-only handler.

typedef struct {
  SimpleIOT* iot;
  SimpleIOTReadyCallback callback;
} SimpleIOTReadyCallbackStruct;

typedef struct {
  SimpleIOT* iot;
  SimpleIOTTriggerUpdateCallback callback;
} SimpleIOTTriggerUpdateCallbackStruct;

typedef struct {
  SimpleIOT* iot;
  SimpleIOTDataCallback callback;
} SimpleIOTDataCallbackStruct;

typedef struct {
  SimpleIOT* iot;
  SimpleIOTDiagCallback callback;
} SimpleIOTDiagCallbackStruct;

//////////////////////////////////////////////////////////////////////////////////////////////////////
class SimpleIOT {

private:
    SimpleIOT() {} // empty plain ctor for Singleton

public:
    static SimpleIOT* getImpl() { return _iot_singleton; }
    static MqttClient* getClient() { return _iot_singleton->_mqttClient; }

    // Use this to create an instance.
    static SimpleIOT* create(const char* wifiSSID,
                             const char* wifiPassword,
                             const char* iotEndpoint,
                             const char* caPem,
                             const char* certPem,
                             const char* keyPem,
                             bool withGateway=false); // set to true if device goes through a gateway

    // And this one to initialize and connect.
    //
    void config(const char* project,
                            const char* model,
                            const char* serialNumber,
                            const char* fwVersion = "1.0.0",
                            SimpleIOTReadyCallback onReady = NULL,
                            SimpleIOTDataCallback onData = NULL,
                            SimpleIOTTriggerUpdateCallback onTriggerUpdate = NULL,
                            SimpleIOTDiagCallback onDiag = NULL);
    ~SimpleIOT();

    // Methods to set the values by name. All values are internally coerced to string
    //
    int set(const char* name, const char* value);
    int set(const char* name, int value);
    int set(const char* name, float value);
    int set(const char* name, double value);
    int set(const char* name, bool value);

    // Same as above, but with location data
    //
    int set(const char* name, const char* value, float latitude, float longitude);
    int set(const char* name, int value, float latitude, float longitude);
    int set(const char* name, float value, float latitude, float longitude);
    int set(const char* name, double value, float latitude, float longitude);
    int set(const char* name, bool value, float latitude, float longitude);

    // Called by loop to give time for networking layer
    //
    void loop(float delayMs=200);

    // Allow getter to secure wifi in case main app needs to use it
    //
    WiFiClientSecure* wifi() { return _wifiClient; }

    // OTA update -- Note: only works with update on S3 since it needs the RootCA.
    // It also only works for specific processors (needs abstraction).
    //
    void performOTA(const char* url, SimpleIOTOTACallback otaCallback = NULL);

    // You can call this explicitly at boot time to issue an async 'check' request.
    // If there is a matching update, it will be returned via the SimpleIOTTriggerUpdateCallback
    // callback handler in the config call. At that point, you can prompt the user aand call the performOTA
    // method to go get the actual upload. If there isn't anything do update, nothing will happen.
    //
    void checkForUpdate(bool force = false);

    // Call this to confirm update has been installed. This will mark the upadte
    // as complete for this device.
    //
    void updateInstalled();

    // For internal use, but it can't be declared private
    //
    void _invokeCallback(const char* topic, const char* buffer, const unsigned int length);
    //int diag(const char* diagID, const char* result);

  protected:
    //
    // constructor is protected -- use 'create' to create a singleton
    //
    SimpleIOT(const char* wifiSSID,
                const char* wifiPassword,
                const char* iotEndpoint,
                const char* caPem,
                const char* certPem,
                const char* keyPem,
                bool withGateway=false);

  private:
    bool _withGateway;
    bool _ready;
    char* _wifiSsid;
    char* _wifiPassword;
    char* _iotEndpoint;
    char* _caPem;
    char* _certPem;
    char* _keyPem;
    char* _project;
    char* _model;
    char* _serialNumber;
    char* _fwVersion;
    char* _monitorTopic;
    char* _diagTopic;
    char* _triggerUpdateTopic;
    char* _clientId;
    
    SimpleIOTReadyCallbackStruct _readyCallback;
    SimpleIOTDataCallbackStruct _dataCallback;
    SimpleIOTDiagCallbackStruct _diagCallback;
    SimpleIOTOTACallback  _otaCallback;
    SimpleIOTTriggerUpdateCallbackStruct _triggerUpdateCallback;

    char _monitorTopicBuffer[INTERNAL_TOPIC_BUFFER_SIZE + 1];
    char _triggerUpdateTopicBuffer[INTERNAL_TOPIC_BUFFER_SIZE + 1];
    int _fwUpdateTotalLength;       //total size of firmware to download
    int _fwUpdateCurrentLength;     //current size of written firmware
    int _fwUpdatePercent;           //Percent downloaded

    WiFiClientSecure* _wifiClient;
    MqttClient* _mqttClient;
    AWSGreenGrassIoT* _greengrass;
    static SimpleIOT* _iot_singleton;  // have to do this to avoid forking and modifying AWSGreenGrassIoT


    // Private methods
    //
    int _sendMessage(const char* op,
                        const char* name,
                        const char* value,
                        SimpleIOTMessageType msgtype=MESSAGE_APP);
    int _sendMessage(const char* op,
                        const char* name,
                        const char* value,
                        float lat,
                        float lng,
                        SimpleIOTMessageType msgtype=MESSAGE_APP);
    int _sendRawMessage(const char* op,
                        DynamicJsonDocument payload,
                        SimpleIOTMessageType msgtype=MESSAGE_APP);
    int _publish(char* buffer, char* payload);
    void _updateFirmware(uint8_t *data, size_t len);
    void _doUpdate(char* op, bool force = false);
    void _updateReceived();      // this marks the update as having been received.

    // Admin commands are handled internally by the SDK. 
    //
    void _handleAdminRequest(const char* topic, DynamicJsonDocument jdoc);

    // Some Diag commands are handled internally by the SDK, others passed on to provided callback
    // handler by the app. 
    //
    void _handleDiagRequest(const char* topic, DynamicJsonDocument jdoc);
};

#endif
