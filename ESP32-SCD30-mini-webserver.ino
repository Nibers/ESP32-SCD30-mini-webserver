#include <Adafruit_SCD30.h>
#include <SPI.h>
#include <Wire.h>
#include <WiFi.h>

#include "config.h"

WiFiServer server(80);
String header;

// Time tracking variables
unsigned long currentTime = millis();
unsigned long previousTime = 0;
const long timeoutTime = 2000;

// Sensor instance
Adafruit_SCD30 scd30;

// Global variables for sensor data
float co2 = 0;
float temperature = 0;
float humidity = 0;

// Data storage arrays for 24 hours (1440 samples at 1-minute intervals)
float stampsCO2[1440] = {};
float stampsTemperature[1440] = {};
float stampsHumidity[1440] = {};
int stampsTaken = 0;

unsigned long lastLoopTime = 0;
unsigned long lastSampleTime = 0;

void setup() {
    Serial.begin(115200);
    delay(2000);

    if (!scd30.begin()) {
        Serial.println("SCD30 sensor not found!");
        while (1);
    }
    Serial.println("SCD30 sensor initialized.");

    if (!scd30.setMeasurementInterval(2)) {
        Serial.println("Failed to set measurement interval!");
        while (1);
    }
    Serial.print("Measurement interval: ");
    Serial.print(scd30.getMeasurementInterval());
    Serial.println(" seconds");
}

void tryConnectWifi() {
    Serial.print("Connecting to ");
    Serial.println(ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected. IP address: ");
    Serial.println(WiFi.localIP());
    server.begin();
}

void loop() {
    unsigned long currentMillis = millis();

    // Read sensor data every 2 seconds
    if (currentMillis - lastLoopTime >= 2000) {
        lastLoopTime = currentMillis;
        readSensor();
        printSensorToSerial();
        if (WiFi.status() != WL_CONNECTED) {
            tryConnectWifi();
        }
    }

    // Save a data sample every 60 seconds
    if (currentMillis - lastSampleTime >= 60000) {
        lastSampleTime = currentMillis;
        takeSample();
    }

    serverLoop();
}

// Convert array to comma-separated string
String arrayToString(float arr[]) {
    String result = "";
    for (int i = 0; i < stampsTaken; i++) {
        result += String(arr[i]);
        if (i < stampsTaken - 1) result += ",";
    }
    return result;
}

// Handle client requests
void serverLoop() {
    WiFiClient client = server.available();

    if (client) {
        currentTime = millis();
        previousTime = currentTime;
        String currentLine = "";

        while (client.connected() && currentTime - previousTime <= timeoutTime) {
            currentTime = millis();
            if (client.available()) {
                char c = client.read();
                header += c;
                if (c == '\n') {
                    if (currentLine.length() == 0) {
                        client.println("HTTP/1.1 200 OK");
                        client.println("Content-type:text/html");
                        client.println("Connection: close");
                        client.println();

                        // Serve HTML content
                        client.println("<!DOCTYPE html><html>");
                        client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
                        client.println("<script src=\"https://cdn.jsdelivr.net/npm/chart.js\"></script>");
                        client.println("<style>html { font-family: Helvetica; text-align: center; }</style></head>");
                        client.println("<body><h1>ESP32 - SCD30 Data (last 24h)</h1>");

                        client.print("<p> CO2: ");
                        client.print(scd30.CO2);
                        client.print(" ppm, ");

                        client.print(" Temp: ");
                        client.print(scd30.temperature, 1);
                        client.print(" °C, ");

                        client.print(" Humidity: ");
                        client.print(scd30.relative_humidity, 1);
                        client.print(" %, ");

                        client.print(" Minutes running: ");
                        client.print(stampsTaken);
                        client.println("</p>");

                        client.println("<div><canvas id=\"line-chart\"></canvas></div>");
                        client.println("<script>const data = [" + arrayToString(stampsCO2) + "];");
                        client.println("const tdata = [" + arrayToString(stampsTemperature) + "];");
                        client.println("const hdata = [" + arrayToString(stampsHumidity) + "];");

                        client.println("const ctx = document.getElementById('line-chart').getContext('2d');");
                        client.println("new Chart(ctx, {");
                        client.println("type: 'line',");
                        client.println("data: {");
                        client.println("labels: Array.from({length: data.length}, (_, i) => new Date(Date.now() - i * 60000).toLocaleTimeString()),");
                        client.println("datasets: [");
                        client.println("{label: 'CO2 (ppm)', data: data, borderColor: 'rgba(75, 192, 192, 1)', borderWidth: 2, fill: false},");
                        client.println("{label: 'Temperature (°C)', data: tdata, borderColor: 'rgba(255, 99, 132, 1)', borderWidth: 2, fill: false},");
                        client.println("{label: 'Humidity (%)', data: hdata, borderColor: 'rgba(153, 102, 255, 1)', borderWidth: 2, fill: false}");
                        client.println("]}, options: {responsive: true}});");
                        client.println("</script></body></html>");

                        client.println();
                        break;
                    } else {
                        currentLine = "";
                    }
                } else if (c != '\r') {
                    currentLine += c;
                }
            }
        }
        header = "";
        client.stop();
        Serial.println("Client disconnected.");
    }
}

// Read sensor data
void readSensor() {
    if (scd30.dataReady()) {
        if (!scd30.read()) {
            Serial.println("Error reading sensor data.");
            return;
        }
        co2 = scd30.CO2;
        humidity = scd30.relative_humidity;
        temperature = scd30.temperature;
    }
}

// Print sensor data to Serial Monitor
void printSensorToSerial() {
    Serial.printf("CO2: %.2f ppm, Temperature: %.2f °C, Humidity: %.2f%%\n", co2, temperature, humidity);
}

// Store sensor data in arrays
void takeSample() {
    if (stampsTaken < 1440) {
        stampsCO2[stampsTaken] = co2;
        stampsTemperature[stampsTaken] = temperature;
        stampsHumidity[stampsTaken] = humidity;
        stampsTaken++;
        Serial.println("Sample stored.");
    } else {
        Serial.println("Memory full. Resetting samples.");
        stampsTaken = 0;
    }
}
