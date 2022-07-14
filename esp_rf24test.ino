#include <ArduinoOTA.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>

#include "RF24.h"
#include "printf.h"
#include "rtc_mem.hpp"
#include "wifi.hpp"
#include <SPI.h>

ESaveWifi eWifi;

ESP8266WebServer server(80);

String header;

// instantiate an object for the nRF24L01 transceiver
RF24 radio(2, 4);

uint8_t counter = 0;

void setup() {
    using rtcMem::gRTC;

    Serial.begin(115200);
    // some boards need to wait to ensure access to serial over USB
    while (!Serial) {
    }

    // initialize the transceiver on the SPI bus
    if (!radio.begin()) {
        Serial.println(F("radio hardware is not responding!!"));
        while (1) {
        }   // hold in infinite loop
    }

    radio.setChannel(68);
    radio.setDataRate(RF24_2MBPS);
    radio.disableCRC();
    radio.disableDynamicPayloads();
    radio.setPayloadSize(17);
    radio.setAutoAck(false);
    radio.setPALevel(RF24_PA_LOW);
    radio.setRetries(15, 15);

    // set the TX address of the RX node into the TX pipe
    uint8_t address[6] = {0xAA, 0xAA, 0xAA, 0xAA, 0xAA, 0xAA};
    radio.openWritingPipe(address);   // always uses pipe 0

    // set the RX address of the TX node into a RX pipe
    radio.openReadingPipe(0, address);   // using pipe 1

    radio.stopListening();

    radio.printDetails();
    // For debugging info
    // printf_begin();             // needed only once for printing details
    // radio.printDetails();       // (smaller) function that prints raw register values
    // radio.printPrettyDetails(); // (larger) function that prints human readable data
    Serial.println("Starting wifi: OTA Enabled.");
    eWifi.turnOn();

    ArduinoOTA.onStart([]() { Serial.println("Start"); });
    ArduinoOTA.onEnd([]() { Serial.println("\nEnd"); });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR)
            Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR)
            Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR)
            Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR)
            Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR)
            Serial.println("End Failed");
    });
    ArduinoOTA.begin();
    server.on("/", HTTP_GET, handleRoot_GET);
    server.on("/", HTTP_POST, handleRoot);
    server.on("/toggle", HTTP_GET, handleRoot);
    server.on("/force_toggle", handleForce);
    server.begin();

    if (!rtcMem::read()) {
        Serial.println("Reading RTC data failed.");
        gRTC.is_on = false;
    }
}

String get_state() {
    using rtcMem::gRTC;
    String res = "{\"state\": ";
    if (gRTC.is_on) {
        res += "\"true\"";
    } else {
        res += "\"false\"";
    }
    res += "}";
    return res;
}

void handleRoot_GET() {
    server.send(200, "application/json", get_state());
}

void handleRoot() {
    using rtcMem::gRTC;
    sendCommand();
    gRTC.is_on = !gRTC.is_on;
    rtcMem::write();
    server.send(200, "application/json", get_state());
}

void handleForce() {
    sendCommand();
    server.send(200, "application/json", get_state());
}

void loop() {
    ArduinoOTA.handle();
    server.handleClient();
}

void listenCommands() {
    radio.startListening();   // put radio in RX mode
    byte data[18] = {0};

    radio.read(&data, sizeof(data));

    // Byte 13 contains the command:
    // 0x20 on/off
    // 0x40 color temperature + (more blue)
    // 0x7F color temperature - (more red)
    // 0x80 brightness +
    // 0xBF brightness -

    if (data[0] == 0x67 && data[1] == 0x22)   // Capture all events from the remote
    {
        Serial.println("=========================");
        Serial.println("Light Bar Packet received");
        Serial.println("=========================");

        for (unsigned long i = 0; i < sizeof(data); i++) {
            if (i == 0) Serial.println("------ address");
            if (i == 7) Serial.println("------ product serial");
            if (i == 11) Serial.println("------ packet ID counter, I think. Avoids duplicate execution");
            if (i == 13) Serial.println("------ action");
            if (i == 14) Serial.println("------ checksum? parameters? noise? idk");
            Serial.print(i, DEC);
            Serial.print(": 0x");
            Serial.print(data[i], HEX);
            if (i == 13 && data[i] == 0x80) Serial.print(" | brightness +");
            if (i == 13 && data[i] == 0xBF) Serial.print(" | brightness -");
            if (i == 13 && data[i] == 0x20) Serial.print(" | on/off");
            if (i == 13 && data[i] == 0x7F) Serial.print(" | temperature - ");
            if (i == 13 && data[i] == 0x40) Serial.print(" | temperature + ");
            Serial.println();
        }
        for (unsigned long i = 11; i < sizeof(data); i++) {
            Serial.print(data[i], HEX);
            Serial.print(", ");
        }
        Serial.println();

        Serial.println("=========================");
        Serial.println();
    }
}

void sendCommand() {
    uint8_t command_base[11] = {
        0x67,
        0x22,
        0x9B,
        0xA3,
        0x89,
        0x26,
        0x82,
        0x55,
        0x96,
        0xA8,
        0x3F,
    };
    // Captured sequences from remote
    uint8_t commands[2][6] = {
        // {0xEA, 0x20, 0x20, 0x30, 0x26, 0xAB},

        {0xF0, 0xE0, 0x20, 0x2A, 0xC0, 0x23},

        {0xF5, 0x60, 0x20, 0x34, 0xB4, 0xD6},

        // {0xF5, 0x60, 0x20, 0x34, 0xB4, 0xD6},

        // {0xF5, 0x60, 0x20, 0x34, 0xB4, 0xD6},

        // {0xF5, 0x60, 0x20, 0x34, 0xB4, 0xD6},
    };

    counter = ((counter + 1) % (sizeof(commands) / sizeof(commands[0])));

    // Send repeatedly, response is unreliable otherwise
    for (int i = 0; i < 17; i++) {
        uint8_t full_command[17];
        memcpy(&full_command[0], &command_base[0], sizeof(command_base));
        memcpy(&full_command[11], &commands[counter][0], sizeof(commands[0]));
        bool report = radio.write(full_command, sizeof(full_command), true);

        if (!report) {
            Serial.println("write fail...");
        }

        // Without the delay, it'll work some times and not others
        delay(30);
    }
    Serial.print("Send command ");
    Serial.println(counter, HEX);
}
