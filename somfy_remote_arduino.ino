#include <LiquidCrystal.h>

#include <Arduino.h>
#include <vector>

/* Adapted to run on ESP32 from original code at https://github.com/Nickduino/Somfy_Remote

This program allows you to emulate a Somfy RTS or Simu HZ remote.
If you want to learn more about the Somfy RTS protocol, check out https://pushstack.wordpress.com/somfy-rts-protocol/

The rolling code will be stored in non-volatile storage (Preferences), so that you can power the Arduino off.

Serial communication of the original code is replaced by MQTT over WiFi.

Modifications should only be needed in config.h.

This forked version 0.1 contains changes made by "Roy Oltmans" published on 29-apr-2019
Published on https://github.com/RoyOltmans/somfy_esp8266_remote_arduino

Changes are:
- Refarctored code from PlatformIO back to Arduino
- Created some state calls and a statefull setup
- Seperated some stuff for robustness
- Made compatible with HA via MQTT

*/
#define ESP8266 true

// GPIO macros
#ifdef ESP32
    #define SIG_HIGH GPIO.out_w1ts = 1 << PORT_TX
    #define SIG_LOW  GPIO.out_w1tc = 1 << PORT_TX
#elif ESP8266
    #define SIG_HIGH GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, 1 << PORT_TX)
    #define SIG_LOW  GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, 1 << PORT_TX)
#endif

// Store the rolling codes in NVS
#ifdef ESP32
    #include <Preferences.h>
    Preferences preferences;
#elif ESP8266
    #include <EEPROM.h>
#endif


// Configuration of the remotes that will be emulated
struct REMOTE {
    unsigned int id;
    char const* mqtt_topic;
    unsigned int default_rolling_code;
    uint32_t eeprom_address;
};

char const* state_shutter;

#include "config.h"

// Wifi and MQTT
#ifdef ESP32
    #include <WiFi.h>
#elif ESP8266
    #include <ESP8266WiFi.h>
#endif

#include "ToolLib.h"
#include <PubSubClient.h>

WiFiClient wifiClient;
PubSubClient client(wifiClient);

// Buttons
#define SYMBOL 640
#define HAUT 0x2
#define STOP 0x1
#define BAS 0x4
#define PROG 0x8

byte frame[7];

void BuildFrame(byte *frame, byte button, REMOTE remote);
void SendCommand(byte *frame, byte sync);
void receivedCallback(char* topic, byte* payload, unsigned int length);
void mqttconnect();

void setup() {
    // USB serial port
    Serial.begin(115200);

    // Output to 433.42MHz transmitter
    pinMode(PORT_TX, OUTPUT);
    SIG_LOW;

    // Connect to WiFi
    setup_wifi();

    // Configure MQTT
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(receivedCallback);

    // Open storage for storing the rolling codes
    #ifdef ESP32
        preferences.begin("somfy-remote", false);
    #elif ESP8266
        EEPROM.begin(1024);
    #endif

    // Clear all the stored rolling codes (not used during normal operation). Only ESP32 here (ESP8266 further below).
    #ifdef ESP32
        if ( reset_rolling_codes ) {
                preferences.clear();
        }
    #endif

    // Print out all the configured remotes.
    // Also reset the rolling codes for ESP8266 if needed.
    for ( REMOTE remote : remotes ) {
        Serial.print("Simulated remote number : ");
        Serial.println(remote.id, HEX);
        Serial.print("Current rolling code    : ");
        unsigned int current_code;

        #ifdef ESP32
            current_code = preferences.getUInt( (String(remote.id) + "rolling").c_str(), remote.default_rolling_code);
        #elif ESP8266
            if ( reset_rolling_codes ) {
                EEPROM.put( remote.eeprom_address, remote.default_rolling_code );
                EEPROM.commit();
            }
            EEPROM.get( remote.eeprom_address, current_code );
        #endif

        Serial.println( current_code );
    }
    Serial.println();
}

void loop() {
    // Reconnect MQTT if needed
    if ( !client.connected() ) {
        mqttconnect();
    }

    client.loop();

    delay(100);
}

void mqttconnect() {
    // Loop until reconnected
    while ( !client.connected() ) {
        Serial.print("MQTT connecting ...");

        // Connect to MQTT, with retained last will message "offline"
        if (client.connect(mqtt_id, mqtt_user, mqtt_password, status_topic, 1, 1, "offline")) {
            Serial.println("connected");

            // Subscribe to the topic of each remote with QoS 1
            for ( REMOTE remote : remotes ) {
                client.subscribe(remote.mqtt_topic, 1);
                Serial.print("Subscribed to topic: ");
                Serial.println(remote.mqtt_topic);
            }

            // Update status, message is retained
            client.publish(status_topic, "online", true);
        }
        else {
            Serial.print("failed, status code =");
            Serial.print(client.state());
            Serial.println("try again in 5 seconds");
            // Wait 5 seconds before retrying
            delay(5000);
        }
    }
}

void receivedCallback(char* topic, byte* payload, unsigned int length) {
    char command = *payload; // 1st byte of payload
    bool commandIsValid = false;
    REMOTE currentRemote;

    Serial.print("MQTT message received: ");
    Serial.println(topic);

    Serial.print("Payload: ");
    for (int i = 0; i < length; i++) {
        Serial.print((char)payload[i]);
    }
    Serial.println();

    // Command is valid if the payload contains one of the chars below AND the topic corresponds to one of the remotes
    if ( length == 1 && ( command == 'u' || command == 's' || command == 'd' || command == 'p' ) ) {
        for ( REMOTE remote : remotes ) {
            if ( strcmp(remote.mqtt_topic, topic) == 0 ){
                currentRemote = remote;
                commandIsValid = true;
            }
        }
    }

    if ( commandIsValid ) {
        if ( command == 'u' ) {
            Serial.println("Monte"); // Somfy is a French company, after all.
            BuildFrame(frame, HAUT, currentRemote);
            state_shutter = "open";
        }
        else if ( command == 's' ) {
            Serial.println("Stop");
            BuildFrame(frame, STOP, currentRemote);
            state_shutter = "unknown";
        }
        else if ( command == 'd' ) {
            Serial.println("Descend");
            BuildFrame(frame, BAS, currentRemote);
            state_shutter = "closed";
        }
        else if ( command == 'p' ) {
            Serial.println("Prog");
            BuildFrame(frame, PROG, currentRemote);
        }

        Serial.println("");

        SendCommand(frame, 2);
        for ( int i = 0; i<2; i++ ) {
            SendCommand(frame, 7);
        }

        // Send the MQTT ack message
        String ackString = "id: 0x";
        ackString.concat( String(currentRemote.id, HEX) );
        ackString.concat(", cmd: ");
        ackString.concat(command);
        client.publish(ack_topic, ackString.c_str());
        client.publish(state_topic, state_shutter, true);
    }
}

void BuildFrame(byte *frame, byte button, REMOTE remote) {
    unsigned int code;

    #ifdef ESP32
        code = preferences.getUInt( (String(remote.id) + "rolling").c_str(), remote.default_rolling_code);
    #elif ESP8266
        EEPROM.get( remote.eeprom_address, code );
    #endif

    frame[0] = 0xA7;            // Encryption key. Doesn't matter much
    frame[1] = button << 4;     // Which button did  you press? The 4 LSB will be the checksum
    frame[2] = code >> 8;       // Rolling code (big endian)
    frame[3] = code;            // Rolling code
    frame[4] = remote.id >> 16; // Remote address
    frame[5] = remote.id >>  8; // Remote address
    frame[6] = remote.id;       // Remote address

    Serial.print("Frame         : ");
    for(byte i = 0; i < 7; i++) {
        if(frame[i] >> 4 == 0) { //  Displays leading zero in case the most significant nibble is a 0.
            Serial.print("0");
        }
        Serial.print(frame[i],HEX); Serial.print(" ");
    }

    // Checksum calculation: a XOR of all the nibbles
    byte checksum = 0;
    for(byte i = 0; i < 7; i++) {
        checksum = checksum ^ frame[i] ^ (frame[i] >> 4);
    }
    checksum &= 0b1111; // We keep the last 4 bits only


    // Checksum integration
    frame[1] |= checksum; //  If a XOR of all the nibbles is equal to 0, the blinds will consider the checksum ok.

    Serial.println(""); Serial.print("With checksum : ");
    for(byte i = 0; i < 7; i++) {
        if(frame[i] >> 4 == 0) {
            Serial.print("0");
        }
        Serial.print(frame[i],HEX); Serial.print(" ");
    }


    // Obfuscation: a XOR of all the bytes
    for(byte i = 1; i < 7; i++) {
        frame[i] ^= frame[i-1];
    }

    Serial.println(""); Serial.print("Obfuscated    : ");
    for(byte i = 0; i < 7; i++) {
        if(frame[i] >> 4 == 0) {
            Serial.print("0");
        }
        Serial.print(frame[i],HEX); Serial.print(" ");
    }
    Serial.println("");
    Serial.print("Rolling Code  : ");
    Serial.println(code);

    #ifdef ESP32
        preferences.putUInt( (String(remote.id) + "rolling").c_str(), code + 1); // Increment and store the rolling code
    #elif ESP8266
        EEPROM.put( remote.eeprom_address, code + 1 );
        EEPROM.commit();
    #endif
}

void SendCommand(byte *frame, byte sync) {
    if(sync == 2) { // Only with the first frame.
        //Wake-up pulse & Silence
        SIG_HIGH;
        delayMicroseconds(9415);
        SIG_LOW;
        delayMicroseconds(89565);
    }

    // Hardware sync: two sync for the first frame, seven for the following ones.
    for (int i = 0; i < sync; i++) {
        SIG_HIGH;
        delayMicroseconds(4*SYMBOL);
        SIG_LOW;
        delayMicroseconds(4*SYMBOL);
    }

    // Software sync
    SIG_HIGH;
    delayMicroseconds(4550);
    SIG_LOW;
    delayMicroseconds(SYMBOL);

    //Data: bits are sent one by one, starting with the MSB.
    for(byte i = 0; i < 56; i++) {
        if(((frame[i/8] >> (7 - (i%8))) & 1) == 1) {
            SIG_LOW;
            delayMicroseconds(SYMBOL);
            SIG_HIGH;
            delayMicroseconds(SYMBOL);
        }
        else {
            SIG_HIGH;
            delayMicroseconds(SYMBOL);
            SIG_LOW;
            delayMicroseconds(SYMBOL);
        }
    }

    SIG_LOW;
    delayMicroseconds(30415); // Inter-frame silence
}
