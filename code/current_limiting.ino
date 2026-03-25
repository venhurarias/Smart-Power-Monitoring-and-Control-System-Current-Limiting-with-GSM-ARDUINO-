#include <SoftwareSerial.h>
#include <Streaming.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Chrono.h>
#include <Bounce2.h>

#define SIM800_TX 9
#define SIM800_RX 8
#define BREAKER_PIN 2
#define BUZZER_PIN 3
#define CURRENT_PIN A0
#define POTENTIOMETER A2
#define RESET_PIN A1

LiquidCrystal_I2C lcd(0x27, 20, 4);

SoftwareSerial sim800(SIM800_TX, SIM800_RX);
String number;
String testMode;

float nVPP;                  // Voltage measured across resistor
float nCurrThruResistorPP;   // Peak Current Measured Through Resistor
float nCurrThruResistorRMS;  // RMS current through Resistor
bool triggered = false;
bool doneInit = false;

Chrono myChrono;
Bounce debouncer = Bounce();

void setup() {

  Wire.begin();
  lcd.begin();
  lcd.backlight();

  Serial.begin(9600);
  Serial.setTimeout(100);
  sim800.begin(9600);

  pinMode(BREAKER_PIN, OUTPUT), digitalWrite(BREAKER_PIN, HIGH);
  pinMode(BUZZER_PIN, OUTPUT), digitalWrite(BUZZER_PIN, HIGH);
  pinMode(POTENTIOMETER, INPUT);
  pinMode(RESET_PIN, INPUT_PULLUP);

  debouncer.attach(RESET_PIN);
  debouncer.interval(50);


  // RecieveMessage();

  lcd.setCursor(0, 0), lcd << F("====================");
  lcd.setCursor(0, 1), lcd << F(" CURRENT DETECTING  ");
  lcd.setCursor(0, 2), lcd << F("      DEVICE        ");
  lcd.setCursor(0, 3), lcd << F("====================");
  delay(3000);
  lcd.clear();

  myChrono.restart();
}

void loop() {
  GSMParser();
  // if (sim800.available()) {
  //   String receivedMessage = sim800.readString();
  //   receivedMessage.trim();
  //   Serial<<receivedMessage<<endl;
  // }
  // testing();
  if (!doneInit) {
    lcd.setCursor(0, 0), lcd << F("====================");
    lcd.setCursor(0, 1), lcd << F("    INITIALIZING    ");
    lcd.setCursor(0, 2), lcd << F("                    ");
    lcd.setCursor(0, 3), lcd << F("====================");
    if (myChrono.hasPassed(1000)) {
      myChrono.restart();
      checkGSM();
    }
    return;
  }
  // if (digitalRead(BUTTON_PIN) == LOW) {
  //   // Serial.println("HAHAH");
  //   // sendMessage();
  //   printPhonebookList();
  //   delay(1000);  // Debounce
  // }
  if (triggered) {
    if (resetPressed()) {
      triggered = false;
     
      buzzerOff();
      delay(1000);
      breakerOn();
      delay(1000);
       lcd.clear();
      myChrono.restart();
    }
  } else {
    float max = getKnobValue();
    float current = getAmpere();
    // Serial << getAmpere() << endl;
    lcd.setCursor(0, 0), lcd << F("====================");
    lcd.setCursor(0, 1), lcd << F("Current: ") << current << F("    ");
    lcd.setCursor(0, 2), lcd << F("Max: ") << max << F("    ");
    lcd.setCursor(0, 3), lcd << F("====================");


    if (current > max) {
      if (myChrono.hasPassed(3000)) {
        triggered = true;
        buzzerOn();
        breakerOff();
        lcd.setCursor(0, 0), lcd << F("====================");
        lcd.setCursor(0, 1), lcd << F("      OVERLOAD      ");
        lcd.setCursor(0, 2), lcd << F("      DETECTED!     ");
        lcd.setCursor(0, 3), lcd << F("====================");
        printPhonebookList(11, current);
        printPhonebookList(12, current);
        printPhonebookList(13, current);
        printPhonebookList(14, current);
        printPhonebookList(15, current);
        printPhonebookList(16, current);
        printPhonebookList(17, current);
        printPhonebookList(18, current);
        printPhonebookList(19, current);
      }
    } else {
      myChrono.restart();
    }
  }
}

bool resetPressed() {
  debouncer.update();
  return debouncer.fell();
}


void checkGSM() {  //pag check kung ready na yung sim, yan isesend na command
  sim800 << "AT+CPIN?" << endl;
}

float mapfloat(long x, long in_min, long in_max, long out_min, long out_max) {  //function natin para maconvert or map yung value
  return (float)(x - in_min) * (out_max - out_min) / (float)(in_max - in_min) + out_min;
}

float getKnobValue() {
  return round(constrain(mapfloat(analogRead(POTENTIOMETER), 0, 1023, 1, 5.2), 1, 5) * 10.0) / 10.0;
}
void buzzerOn() {
  digitalWrite(BUZZER_PIN, LOW);
}

void buzzerOff() {
  digitalWrite(BUZZER_PIN, HIGH);
}

void breakerOn() {
  digitalWrite(BREAKER_PIN, LOW);
}

void breakerOff() {
  digitalWrite(BREAKER_PIN, HIGH);
}

void allOff() {
  breakerOff();
  buzzerOff();
}

void sendMessage() {
  Serial.println("Sending text message...");

  // Replace these with your SIM card information
  String phoneNumber = "+639760337858";  // Replace with the recipient's phone number

  // Set SMS mode
  sim800.println("AT+CMGF=1");
  delay(1000);
  readSerial();

  // Set phone number
  sim800.println("AT+CMGS=\"" + phoneNumber + "\"");
  delay(1000);
  readSerial();

  // Message content
  String message = "Hello, this is a test message from Arduino!";
  sim800.print(message);
  delay(1000);

  // Send Ctrl+Z to indicate the end of the message
  sim800.println((char)26);
  delay(1000);
  readSerial();
}


void testing() {  //ito function gamit natin sa manual process parang mamanual natin pag control sa mga pump, vale, LED, at GSM
  if (Serial.available()) {
    String reading = Serial.readString();
    Serial << reading << endl;

    reading.trim();
    if (reading == "x") {
      Serial << "SMS Send" << endl;
      sendSMS("+639564794717", "Hello");
    } else if (reading == "b") {
      Serial << "Recieve message" << endl;
      RecieveMessage();
    } else if (reading == "n") {
      Serial << "Check GSM" << endl;
      checkGSM();
    }
  }
}
float getAmpere() {
  nVPP = getVPP();
  nCurrThruResistorPP = (nVPP / 200.0) * 1000.0;
  nCurrThruResistorRMS = nCurrThruResistorPP * 0.707;
  return constrain(nCurrThruResistorPP - 0.03, 0, 100);
}

float getVPP() {
  float result;
  int readValue;
  int maxValue = 0;  // store max value here
  uint32_t start_time = millis();
  while ((millis() - start_time) < 1000)  //sample for 1 Sec
  {
    readValue = analogRead(CURRENT_PIN);
    if (readValue > maxValue) {
      /*record the maximum sensor value*/
      maxValue = readValue;
    }
  }

  // Convert the digital data to a voltage
  result = (maxValue * 5.0) / 1024.0;

  return result;
}


void readSerial() {
  while (sim800.available()) {
    Serial.write(sim800.read());
  }
}

void RecieveMessage() {
  sim800 << "AT+CMGF=1" << endl;
  delay(1000);
  sim800 << "AT+CNMI=2,2,0,0,0" << endl;
}
void GSMParser() {
  if (sim800.available()) {
    String receivedMessage = sim800.readString();
    receivedMessage.trim();
    String phoneNumber, emptyField, date, time, message;
    Serial << "receivedMessage  ::" << receivedMessage << endl;
    if (receivedMessage.startsWith("+CMT:")) {
      int firstQuote = receivedMessage.indexOf('\"');
      int secondQuote = receivedMessage.indexOf('\"', firstQuote + 1);
      phoneNumber = receivedMessage.substring(firstQuote + 1, secondQuote);

      int thirdQuote = receivedMessage.indexOf('\"', secondQuote + 1);
      int fourthQuote = receivedMessage.indexOf('\"', thirdQuote + 1);

      int commaIndex = receivedMessage.indexOf(',', fourthQuote + 3);
      int newlineIndex = receivedMessage.indexOf('\n');
      Serial << "firstQuote :: " << firstQuote << "\t\tsecondQuote :: " << secondQuote << "\t\tthirdQuote :: " << thirdQuote << "\t\tfourthQuote :: " << fourthQuote + 3 << "\t\tComma Index :: " << commaIndex << endl;

      date = receivedMessage.substring(fourthQuote + 3, commaIndex);
      time = receivedMessage.substring(commaIndex + 1, newlineIndex);


      message = receivedMessage.substring(newlineIndex + 1);
      Serial << "phoneNumber :: " << phoneNumber << "\t\tdate :: " << date << "\t\ttime :: " << time << "\t\tmessage :: " << message << endl;
      if (message == "*REGISTER#") {
        savePhoneNumber(phoneNumber.c_str());
        delay(1000);
        RecieveMessage();
        delay(1000);
        sendSMS(phoneNumber, "Successfully registered!");
      }
    } else if (receivedMessage.startsWith("AT+CPBR=10,10")) {
      int newlineIndex = receivedMessage.indexOf('\n');
      message = receivedMessage.substring(newlineIndex + 1);
      Serial << "Message :: " << message << endl;
      if (message.startsWith("+CPBR: 10,")) {
        int firstQuote = receivedMessage.indexOf('\"');
        int secondQuote = receivedMessage.indexOf('\"', firstQuote + 1);
        phoneNumber = receivedMessage.substring(firstQuote + 1, secondQuote);
        Serial << "Phone Number :: " << phoneNumber << endl;
        number = phoneNumber;
      }
    } else if (receivedMessage.startsWith("AT+CPIN?") && receivedMessage.indexOf("READY") != -1) {  //kung meesage ay nagsisimula sa AT+CPIN? meaning may sinend tayo command para icheck kung ready na yung sim, tas kung may nakalagay na ready sa message meaning ay ready na yung sim
      Serial << "GSM OK" << endl;
      delay(2000);
      RecieveMessage();
      doneInit = true;
      buzzerOff();
      breakerOn();
    }
  }
}
void sendSMS(String phoneNumber, String message) {
  Serial << "SEND :: " << phoneNumber << message << endl;
  // sim800 << "AT+CMGF=1" << endl;
  // delay(1000);
  sim800 << "AT+CMGS=\"" << phoneNumber << "\"" << endl;
  delay(100);
  sim800 << message << endl;
  delay(100);
  sim800 << (char)26 << endl;
  delay(100);
  sim800 << endl;
}


void savePhoneNumber(String phoneNumber) {
  Serial << "Get Available index" << endl;
  int nextIndex = findNextAvailableIndex();

  // If an available index is found, save the phone number
  if (nextIndex > 0) {
    String command = "AT+CPBW=" + String(nextIndex) + ",\"" + phoneNumber + "\"";
    // Send the command to the SIM800 module
    sim800.println(command);
    delay(1000);

    // Check the response from the SIM800 module
    while (sim800.available()) {
      Serial.write(sim800.read());
    }

    Serial.println("Phone number saved at index " + String(nextIndex));
  } else {
    Serial.println("No available index found.");
  }
}

int findNextAvailableIndex() {
  // Iterate through phonebook entries to find the next available index
  for (int i = 11; i <= 20; i++) {  // Assuming a phonebook size of 100; adjust as needed
    sim800 << "AT+CPBR=" << i << "," << i << endl;

    while (!sim800.available())
      ;

    // Check the response from the SIM800 module
    if (sim800.available()) {
      String receivedMessage = sim800.readString();
      int newlineIndex = receivedMessage.indexOf('\n');
      String message = receivedMessage.substring(newlineIndex + 1);
      Serial << "Message :: " << message << endl;
      if (message.startsWith("+CPBR: " + String(i) + ",")) {


      } else {
        return i;
      }
    }
  }

  // Return -1 if no available index is found
  return -1;
}
void printPhonebookList(int i, float current) {
  Serial.println("Phonebook List:");

  String phoneNumber = "";
  sim800 << "AT+CPBR=" << i << "," << i << endl;
  while (!sim800.available())
    ;
  if (sim800.available()) {
    String response = sim800.readString();

    if (response.indexOf("+CPBR:") != -1) {
      // Extract the phone number from the response
      int startIndex = response.indexOf("\"") + 1;
      int endIndex = response.lastIndexOf("\"");
      phoneNumber = response.substring(startIndex, endIndex - 7);
      Serial.println(phoneNumber);
      if (phoneNumber != "") {
        delay(200);
        sendSMS(phoneNumber.c_str(), "Circuit Overload Detected! last current reading is "+String(current));
        while (!sim800.available())
          ;
        if (sim800.available()) {
          String response = sim800.readString();
        }
      }

      delay(1000);
      Serial << "NEXT!!!" << endl;
    }
  }
}
