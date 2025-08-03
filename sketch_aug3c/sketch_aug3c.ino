#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

// Pin definitions for NodeMCU/Wemos D1 Mini
#define ROOM1_LED D0    // GPIO16
#define ROOM2_LED D1    // GPIO5
#define BUZZER    D5    // GPIO14
#define GAS_LED   D8    // GPIO15
#define MQ5_PIN   A0    // Analog pin
#define TRIG_PIN  D6    // GPIO12
#define ECHO_PIN  D7    // GPIO13

// WiFi credentials
const char* ssid = "Jihadxyz";
const char* password = "101336ROJ";

// Server
ESP8266WebServer server(80);

// Thresholds
const int GAS_THRESHOLD = 200;     // Changed to 200
const int DISTANCE_THRESHOLD = 20; // cm

// State flags
bool gasAlertActive = false;
bool ultrasonicAlertActive = false;
bool gasLedState = false;

// Timing variables
unsigned long lastGasCheck = 0;
unsigned long lastUltrasonicCheck = 0;
unsigned long lastGasBeep = 0;
unsigned long lastGasLedToggle = 0;

void setup() {
  Serial.begin(115200);
  delay(100);
  
  Serial.println("ESP8266 Home Defender Starting...");

  // Initialize pins
  pinMode(ROOM1_LED, OUTPUT);
  pinMode(ROOM2_LED, OUTPUT);
  pinMode(GAS_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  // Set all outputs to LOW
  digitalWrite(ROOM1_LED, LOW);
  digitalWrite(ROOM2_LED, LOW);
  digitalWrite(GAS_LED, LOW);
  digitalWrite(BUZZER, LOW);

  // Connect to WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // Start mDNS
  if (MDNS.begin("homedefender")) {
    Serial.println("Access: http://homedefender.local");
  }

  // Setup routes
  setupWebRoutes();
  server.begin();
  Serial.println("Server Started");
}

void setupWebRoutes() {
  server.on("/", handleRoot);
  
  // LED controls
  server.on("/led1on", []() { digitalWrite(ROOM1_LED, HIGH); redirect(); });
  server.on("/led1off", []() { digitalWrite(ROOM1_LED, LOW); redirect(); });
  server.on("/led2on", []() { digitalWrite(ROOM2_LED, HIGH); redirect(); });
  server.on("/led2off", []() { digitalWrite(ROOM2_LED, LOW); redirect(); });
  
  // Gas alert
  server.on("/gasoff", []() { 
    gasAlertActive = false; 
    digitalWrite(BUZZER, LOW); 
    digitalWrite(GAS_LED, LOW); 
    Serial.println("Gas Alert STOPPED"); 
    redirect(); 
  });
  
  // Ultrasonic
  server.on("/uson", []() { 
    ultrasonicAlertActive = true; 
    Serial.println("Ultrasonic Alert ON"); 
    redirect(); 
  });
  server.on("/usoff", []() { 
    ultrasonicAlertActive = false; 
    digitalWrite(BUZZER, LOW); 
    Serial.println("Ultrasonic Alert OFF"); 
    redirect(); 
  });
  
  // All controls
  server.on("/allon", []() { 
    digitalWrite(ROOM1_LED, HIGH); 
    digitalWrite(ROOM2_LED, HIGH); 
    ultrasonicAlertActive = true;
    Serial.println("ALL ON"); 
    redirect(); 
  });
  server.on("/alloff", []() { 
    digitalWrite(ROOM1_LED, LOW); 
    digitalWrite(ROOM2_LED, LOW); 
    ultrasonicAlertActive = false;
    gasAlertActive = false;
    digitalWrite(BUZZER, LOW);
    digitalWrite(GAS_LED, LOW);
    Serial.println("ALL OFF"); 
    redirect(); 
  });
}

void redirect() {
  server.sendHeader("Location", "/", true);
  server.send(302, "text/plain", "");
}

void loop() {
  server.handleClient();
  MDNS.update();

  unsigned long now = millis();
  
  // Check gas sensor every 2 seconds
  if (now - lastGasCheck >= 2000) {
    checkGasSensor();
    lastGasCheck = now;
  }
  
  // Check ultrasonic every 1 second
  if (now - lastUltrasonicCheck >= 1000) {
    checkUltrasonicSensor();
    lastUltrasonicCheck = now;
  }
  
  // Handle gas alert beeping and LED flashing
  handleGasAlert();
  
  yield();
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<title>Home Defender</title>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<style>";
  html += "body{font-family:Arial;text-align:center;margin:20px;background:#222;color:#fff}";
  html += "h1{color:#4CAF50;margin-bottom:30px}";
  html += "button{padding:15px 25px;margin:10px;font-size:18px;border:none;border-radius:8px;cursor:pointer;min-width:120px}";
  html += ".on{background:#4CAF50;color:white}.off{background:#f44336;color:white}";
  html += ".ctrl{background:#2196F3;color:white}.all{background:#ff9800;color:white}";
  html += ".status{margin:20px;padding:15px;background:#333;border-radius:8px}";
  html += "</style></head><body>";
  
  html += "<h1>🏠 Home Defender</h1>";
  
  // Room controls
  html += "<div>";
  html += "<button class='on' onclick=\"location.href='/led1on'\">Room 1 ON</button>";
  html += "<button class='off' onclick=\"location.href='/led1off'\">Room 1 OFF</button>";
  html += "</div><div>";
  html += "<button class='on' onclick=\"location.href='/led2on'\">Room 2 ON</button>";
  html += "<button class='off' onclick=\"location.href='/led2off'\">Room 2 OFF</button>";
  html += "</div>";
  
  // Security controls
  html += "<div style='margin-top:20px'>";
  html += "<button class='on' onclick=\"location.href='/uson'\">Ultrasonic ON</button>";
  html += "<button class='off' onclick=\"location.href='/usoff'\">Ultrasonic OFF</button>";
  html += "</div><div>";
  html += "<button class='ctrl' onclick=\"location.href='/gasoff'\">Stop Gas Alert</button>";
  html += "</div>";
  
  // All controls
  html += "<div style='margin-top:30px'>";
  html += "<button class='all' onclick=\"location.href='/allon'\">ALL ON</button>";
  html += "<button class='all' onclick=\"location.href='/alloff'\">ALL OFF</button>";
  html += "</div>";
  

  
  html += "<script>setTimeout(function(){location.reload()},10000);</script>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

void checkGasSensor() {
  int gasValue = analogRead(MQ5_PIN);
  Serial.print("Gas Level: ");
  Serial.println(gasValue);

  if (gasValue > GAS_THRESHOLD) {
    if (!gasAlertActive) {
      gasAlertActive = true;
      Serial.println("⚠️ GAS ALERT TRIGGERED! Level: " + String(gasValue));
    }
  } else {
    if (gasAlertActive) {
      gasAlertActive = false;
      digitalWrite(GAS_LED, LOW);
      digitalWrite(BUZZER, LOW);
      Serial.println("✅ Gas Alert CLEARED. Level: " + String(gasValue));
    }
  }
}

void checkUltrasonicSensor() {
  if (!ultrasonicAlertActive) return;

  // Get distance
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  int distance = duration * 0.034 / 2;

  if (distance > 0 && distance < 200) { // Valid reading
    Serial.print("Distance: ");
    Serial.print(distance);
    Serial.println(" cm");
    
    if (distance < DISTANCE_THRESHOLD) {
      Serial.println("🚨 OBJECT DETECTED! Distance: " + String(distance) + " cm");
      // Single beep for object detection
      digitalWrite(BUZZER, HIGH);
      delay(100);
      digitalWrite(BUZZER, LOW);
    }
  } else {
    Serial.println("Distance: Out of range");
  }
}

void handleGasAlert() {
  if (!gasAlertActive) return;
  
  unsigned long now = millis();
  
  // Flash GAS LED every 500ms
  if (now - lastGasLedToggle >= 500) {
    gasLedState = !gasLedState;
    digitalWrite(GAS_LED, gasLedState);
    lastGasLedToggle = now;
  }
  
  // Beep beep pattern every 1 second
  if (now - lastGasBeep >= 1000) {
    // First beep
    digitalWrite(BUZZER, HIGH);
    delay(100);
    digitalWrite(BUZZER, LOW);
    delay(100);
    // Second beep
    digitalWrite(BUZZER, HIGH);
    delay(100);
    digitalWrite(BUZZER, LOW);
    
    lastGasBeep = now;
  }
}