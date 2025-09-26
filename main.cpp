#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Pin
const int measurePin  = 34; // GPIO34
const int ledPower    = 18; // LED sensor
const int switchPin   = 19; // ปุ่มเปิดปิด
const int modePin     = 23; // ปุ่มเลือกโหมด (Auto/Manual)
const int fanButtonPin = 17; // ปุ่มปรับ Fan Speed ใน Manual
const int filterPin = 33; // ต่อ potentiometer ที่ GPIO13
const int fanLedPin = 25;


// กำหนดเวลา
int samplingTime = 280;
int deltaTime    = 40;
int sleepTime    = 9680;
unsigned long menuStartTime = 0;

// ค่าเซ็นเซอร์
float voMeasured = 0;
float calcVoltage = 0;
float dustDensity = 0;

int fanSpeed = 1;

// FSM
int mode = 0;         
String modes[] = {"Auto", "Manual"};
String state = "init";

bool modeButtonPressed = false;

//call ฟังก์ชัน
void handleModeButton();
bool switchModeButton();
void readSensor();
bool fanButtonPressed();
String checkFilterStatus();
void updateFan();

void setup() {
  Serial.begin(115200);
  pinMode(ledPower, OUTPUT);
  digitalWrite(ledPower, HIGH);

  pinMode(switchPin, INPUT_PULLUP);   
  pinMode(modePin, INPUT_PULLUP);    
  pinMode(fanButtonPin, INPUT_PULLUP);
  pinMode(fanLedPin, OUTPUT);
 

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
    Serial.println(F("SSD1306 allocation failed"));
    for (;;);
  }
  display.clearDisplay();
  display.display();
}

void loop() {
  int switchState = digitalRead(switchPin);

  // ปิดเครื่องด้วย switch
  if (switchState == LOW) {  
    display.clearDisplay();
    display.display();
    state = "init";          
  }

  //state init เริ่มการทำงาน
  if (state == "init") {
    updateFan();
    if (switchState == HIGH) {  
      state = "loading";          
    }
  }

  //state loading เป็นหน้า loading
  else if (state == "loading") {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(0, 10);
    display.println("Air Quality");
    display.setCursor(0, 30);
    display.println("Opening...");
    display.display();
    delay(2000);
    state = "filter";   
    menuStartTime = 0;
  }

  // state filter เป็นตัวเช็คฟิลเตอร์กรองในเครื่อง
  else if (state == "filter") {
  String filterStatus = checkFilterStatus();

  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 10);
  display.println("Filter Status:");
  display.setCursor(0, 30);
  display.println(filterStatus);
  display.display();

  delay(2000);  

  state = "menu";   
  menuStartTime = 0;
}

  // state menu เลือกโหมด auto หรือ manual
  else if (state == "menu") {
    handleModeButton();    

    if (menuStartTime == 0) {
      menuStartTime = millis();
    }

    if (modeButtonPressed) {
      menuStartTime = millis();
    }


    if (millis() - menuStartTime >= 3000) {
      state = "mode";       
      menuStartTime = 0;
    }

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 10);
    display.println("Select Mode");
    display.setCursor(0, 40);
    display.println(modes[mode]);

    int remaining = 3 - (millis() - menuStartTime)/1000;
    if (remaining < 0) remaining = 0;
    display.setCursor(100, 10);
    display.print(remaining);
    display.println("s");

    display.display();
  }

  //state mode เป็นตัวจัดการเรื่องการเลือกโหมด
  else if (state == "mode") {
    if (mode == 0) state = "auto";
    else state = "manual";
  }

  //state auto เป็นการทำงานของโหมด auto
  else if (state == "auto") {
    readSensor();

    if (switchModeButton()) {
      state = "manual"; 
    }

  
    if (dustDensity <= 50) fanSpeed = 1;
    else if (dustDensity < 100) fanSpeed = 2;
    else fanSpeed = 3;

    updateFan();

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Mode: Auto");
    display.setTextSize(2);
    display.setCursor(0, 20);
    display.print(dustDensity, 0);
    display.println(" ug/m3");

    display.setTextSize(1);
    display.setCursor(0, 50);
    display.print("Fan Speed: ");
    display.println(fanSpeed);

    display.display();
    delay(200); 
  }

  //state manual เป็นการทำงานของโหมด manual
  else if (state == "manual") {
    readSensor();
    if (switchModeButton()) {
      state = "auto"; 
    }

    if (fanButtonPressed()) {   
      fanSpeed++;
      if (fanSpeed > 3) fanSpeed = 1;
    }

    updateFan();

    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Mode: Manual");
    display.setTextSize(2);
    display.setCursor(0, 20);
    display.print(dustDensity, 0);
    display.println(" ug/m3");

    display.setTextSize(1);
    display.setCursor(0, 50);
    display.print("Fan Speed: ");
    display.println(fanSpeed);

    display.display();
    delay(200);
  }
}

// ฟังก์ชันต่างๆ

// อ่านค่าเซ็นเซอร์
void readSensor() {
  digitalWrite(ledPower, LOW);
  delayMicroseconds(samplingTime);

  voMeasured = analogRead(measurePin);

  delayMicroseconds(deltaTime);
  digitalWrite(ledPower, HIGH);
  delayMicroseconds(sleepTime);

  calcVoltage = voMeasured * (3.3 / 4095.0);
  dustDensity = 0.17 * calcVoltage - 0.1;
  if (dustDensity < 0) dustDensity = 0;
  dustDensity *= 1000;

  Serial.print("Dust: "); Serial.print(dustDensity);
  Serial.println(" ug/m3");
}

//จัดการเรื่องการกดปุ่มของการเลือกโหมด
void handleModeButton() {
  int buttonState = digitalRead(modePin);

  if (buttonState == LOW && !modeButtonPressed) {
    modeButtonPressed = true;
  }
  if (buttonState == HIGH && modeButtonPressed) {
    mode = (mode + 1) % 2; 
    modeButtonPressed = false;
  }
}

//จัดการเรื่องการกดปุ่มของการเปลี่ยนโหมดระหว่างทำงาน
bool switchModeButton() {
  static bool buttonPressed = false;
  static unsigned long lastDebounceTime = 0;
  const unsigned long debounceDelay = 50; 

  int reading = digitalRead(modePin);

  if (reading == LOW && !buttonPressed && (millis() - lastDebounceTime > debounceDelay)) {
    buttonPressed = true;
    lastDebounceTime = millis();
  }

  if (reading == HIGH && buttonPressed) {
    buttonPressed = false;
    lastDebounceTime = millis();
    return true; 
  }

  return false;
}

//จัดการเรื่องการกดปุ่มของการเปลี่ยนความเร็วพัดลมในโหมด manual
bool fanButtonPressed() {
  static bool buttonPressed = false;
  static unsigned long lastDebounceTime = 0;
  const unsigned long debounceDelay = 50; 

  int reading = digitalRead(fanButtonPin);

  if (reading == LOW && !buttonPressed && (millis() - lastDebounceTime > debounceDelay)) {
    buttonPressed = true;
    lastDebounceTime = millis();
  }

  if (reading == HIGH && buttonPressed) {
    buttonPressed = false;
    lastDebounceTime = millis();
    return true; 
  }

  return false;
}

//เช็คฟิลเตอร์กรองว่าใกล้จะหมดหรือไม่
String checkFilterStatus() {
  int rawValue = analogRead(filterPin);
  String status;

  if (rawValue < 1365) {
    status = "Filter Empty";
  } else if (rawValue < 2730) {
    status = "Filter Near End";
  } else {
    status = "Filter OK";
  }

  Serial.print("Filter raw: ");
  Serial.print(rawValue);
  Serial.print(" -> ");
  Serial.println(status);

  return status;
}

//ปรับพัดลมตามค่า fanspeed
void updateFan() {
  int pwmValue = 0;
  if(state == "loading" || state == "menu" || state == "filter" || state == "init"){
    pwmValue = 0;
  }
  else{
    if (fanSpeed == 1) pwmValue = 30;  
    else if (fanSpeed == 2) pwmValue = 100; 
    else if (fanSpeed == 3) pwmValue = 255;
  }
  analogWrite(fanLedPin, pwmValue);  
}


