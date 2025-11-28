#include <Adafruit_SCD30.h>
#include <SPI.h>
#include <Wire.h>
#include <ESP8266WiFiMulti.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <ESP8266HTTPClient.h>

#include "config.h"

WiFiServer server(80);
String header;
ESP8266WiFiMulti wifiMulti;

// Time tracking variables
unsigned long currentTime = millis();
unsigned long previousTime = 0;
const long timeoutTime = 2000;

// OLED Display
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET     -1 // Reset pin # (or -1 if sharing Arduino reset pin)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

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
    delay(1000);

    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
      Serial.println(F("SSD1306 not found. Please comment out if you do not have a Display attatched."));
      for(;;);
    }

    display.display();
    delay(1000);

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

void drawToDisplay() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.print("Temp: "); display.println(scd30.temperature - 3.5f);
  display.print("Humi: "); display.println(scd30.relative_humidity);
  display.print("CO2: "); display.println(scd30.CO2);

  if (WiFi.status() == WL_CONNECTED) {
    display.println("");
    display.println(WiFi.SSID());
    display.print("http://"); display.println(WiFi.localIP());
  }
  display.display();
}

void tryConnectWifi() {
    Serial.println("Adding WiFi networks...");

    for (int i = 0; i < WIFI_NETWORK_COUNT; i++) {
        wifiMulti.addAP(WIFI_SSIDS[i], WIFI_PASSWORDS[i]);
        Serial.print("  Added: ");
        Serial.println(WIFI_SSIDS[i]);
    }

    Serial.println("Connecting...");
    while (wifiMulti.run() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("\nConnected to WiFi!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    server.begin();
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
        temperature = scd30.temperature - 3.5f;
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

void loop() {
    unsigned long currentMillis = millis();

    // Read sensor data every 2 seconds
    if (currentMillis - lastLoopTime >= 2000) {
        lastLoopTime = currentMillis;
        readSensor();
        printSensorToSerial();
        drawToDisplay();
        if (wifiMulti.run() != WL_CONNECTED) {
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
                        client.println("<head><meta charset=\"utf-8\" name=\"ESP32 SCD30\" content=\"width=device-width, initial-scale=1\">");
                        client.println("<script src=\"https://cdn.jsdelivr.net/npm/chart.js\"></script>");
                        client.println("<style>html { font-family: Helvetica; text-align: center; }</style></head>");
                        client.println("<body><h1>ESP32 - SCD30 Data (last 24h)</h1>");

                        client.print("<p> CO2: ");
                        client.print(scd30.CO2);
                        client.print(" ppm, ");

                        client.print(" Temp: ");
                        client.print(scd30.temperature - 3.5f, 1);
                        client.print(" °C, ");

                        client.print(" Humidity: ");
                        client.print(scd30.relative_humidity, 1);
                        client.print(" %, ");

                        client.print(" Minutes running: ");
                        client.print(stampsTaken);
                        client.println("</p>");

                        client.println("<button onclick=\"dlCSV()\">CSV</button> <button onclick=\"dlJSON()\">JSON</button>");

                        client.println("<div style=\"width: 100%; margin: 0 auto;\"><canvas id=\"line-chart\"></canvas></div>");
                        
                        client.println("<script>function dlCSV(){console.log('CSV Download started');const csvContent='Time;CO2 (ppm);Temperature (°C);Humidity (%)\\n'+");
                        client.println("data.map((d,i)=>`${i};${d.toLocaleString()};${tdata[i].toLocaleString()};${hdata[i].toLocaleString()}`).join('\\n');");
                        client.println("const link=document.createElement('a');link.href='data:text/csv;charset=utf-8,'+encodeURIComponent(csvContent);link.download='data.csv';");
                        client.println("link.click();}function dlJSON(){console.log('JSON Download started');const jsonData={time:Array.from({length:data.length},(_,i)=>i),co2:data,temperature:tdata,humidity:hdata};");
                        client.println("const jsonString=JSON.stringify(jsonData);const link=document.createElement('a');link.href='data:application/json;charset=utf-8,'+encodeURIComponent(jsonString);link.download='data.json';link.click();}");

                        client.println("const data = [" + arrayToString(stampsCO2) + "];");
                        client.println("const tdata = [" + arrayToString(stampsTemperature) + "];");
                        client.println("const hdata = [" + arrayToString(stampsHumidity) + "];");
                        
                        client.println("const data_smooth=new Array(data.length);");
                        client.println("const smoothFactor=5;");
                        client.println("data.forEach((element,index)=>{const startIndex=Math.max(0,index-smoothFactor+1);");
                        client.println("const endIndex=index+1;const subArray=data.slice(startIndex,endIndex);const average=subArray.reduce((acc,val)=>acc+val,0)/subArray.length;data_smooth[index]=average});");
                        client.println("const now=new Date();const timestamps=[];");
                        client.println("for(let i=data.length-1;i>=0;i--){const time=new Date(now-i*60*1000);const formattedTime=time.toLocaleTimeString();");
                        client.println("timestamps.push(formattedTime)}const ctx=document.getElementById('line-chart').getContext('2d');");
                        client.println("const lineChart=new Chart(ctx,{type:'line',data:{labels:timestamps,datasets:[{label:'CO2 - 5-Minuten Mittel',");
                        client.println("data:data_smooth,borderColor:'rgba(75, 192, 192, 1)',borderWidth:2,fill:false,yAxisID:'y1'},");
                        client.println("{label:'CO2',data:data,borderColor:'rgba(192, 192, 70, .4)',borderWidth:2,fill:false,yAxisID:'y1'},");
                        client.println("{label:'Temperatur (°C)',data:tdata,borderColor:'rgba(255, 99, 132, 1)',borderWidth:2,fill:false,yAxisID:'y2'},");
                        client.println("{label:'Luftfeuchtigkeit (%)',data:hdata,borderColor:'rgba(153, 102, 255, 1)',borderWidth:2,fill:false,yAxisID:'y3'}]},");
                        client.println("options:{responsive:true,scales:{x:{display:true,scaleLabel:{display:true,labelString:'Time'}},y:{id:'y1',type:'linear',position:'left',");
                        client.println("scaleLabel:{display:true,labelString:'CO2 (ppm)'}},y2:{id:'y2',type:'linear',position:'right',scaleLabel:{display:true,");
                        client.println("labelString:'Temperatur (°C)'}},y3:{id:'y3',type:'linear',position:'right',scaleLabel:{display:true,labelString:'Luftfeuchtigkeit (%)'}}}}});</script>");
                        

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
