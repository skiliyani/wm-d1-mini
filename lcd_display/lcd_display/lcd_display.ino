#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <LiquidCrystal_I2C.h>

// WiFi credentials
const char* ssid = "SAYANI_WIFI_IoT";
const char* password = "IoooT543";

// MQTT broker settings
const char* mqtt_server = "192.168.68.10";
const int mqtt_port = 1883;
const char* mqtt_topic = "home/ui/water-tank/level";

// LCD display settings
LiquidCrystal_I2C lcd(0x27, 20, 4); // I2C address 0x27

// Wifi Client
WiFiClient wifiClient;

// MQTT client
PubSubClient mqttClient(wifiClient);

// Flag to indicate if "No data" is already shown
bool noDataShown = false;

// Last time a message was received
unsigned long lastMessageReceivedTime = 0;

// Value received in topic
int receivedValue = 0;

byte zero[] = {
  B11111,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B11111
};
byte one[] = {
  B11111,
  B10000,
  B10000,
  B10000,
  B10000,
  B10000,
  B10000,
  B11111
};

byte two[] = {
  B11111,
  B11000,
  B11000,
  B11000,
  B11000,
  B11000,
  B11000,
  B11111
};

byte three[] = {
  B11111,
  B11100,
  B11100,
  B11100,
  B11100,
  B11100,
  B11100,
  B11111
};

byte four[] = {
  B11111,
  B11110,
  B11110,
  B11110,
  B11110,
  B11110,
  B11110,
  B11111
};

byte five[] = {
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111,
  B11111
};

byte customTapSymbol[] = {
  B01110,
  B00100,
  B11110,
  B11111,
  B00011,
  B00011,
  B00000,
  B00000
};


void setup_progressbar() {
  lcd.createChar(0, zero);
  lcd.createChar(1, one);
  lcd.createChar(2, two);
  lcd.createChar(3, three);
  lcd.createChar(4, four);
  lcd.createChar(5, five);
}

// Reconnect to WiFi and MQTT broker
void reconnect() {
  if (!WiFi.isConnected()) {
    Serial.println("Reconnecting to WiFi...");
    clearDisplayAndPrintText("WiFi Connecting");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("Connected to the WiFi network");
    clearDisplayAndPrintText("WiFi Connected");
  }

  if (!mqttClient.connected()) {
    Serial.println("Reconnecting to MQTT broker...");
    clearDisplayAndPrintText("MQTT Connecting");
    String clientId = "ESP8266-";
    clientId += String(random(0xffff), HEX);
    while (!mqttClient.connect(clientId.c_str())) {
      delay(500);
      Serial.print(".");
    }
    mqttClient.subscribe(mqtt_topic);
    Serial.println("Connected to the MQTT broker");
    clearDisplayAndPrintText("MQTT Connected");
  }
}

void updateDisplay(int percentage) {
  if (noDataShown) {
    lcd.backlight();
    // Set the flag to indicate that "No data" is not shown
    noDataShown = false;
  }

  updateNumber(percentage);
  updateProgressBar(percentage, 100, 3);

  // Set the flag to indicate that "No data" is not shown
  noDataShown = false;
}

void clearDisplayAndPrintText(const char* text) {
  // Clear the display
  lcd.clear();

  // Set the cursor position to the desired location
  lcd.setCursor(0, 0); // This will place the text at the top left corner.

  // Print the text
  lcd.print(text);
}

// MQTT callback function
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println("Message arrived on topic: " + String(topic));

  //lcd.clear();

  // Convert the payload to an integer
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  receivedValue = message.toInt();
  Serial.println("Received value: " + String(receivedValue));

  // Display the progress bar and percentage
  updateDisplay(receivedValue);

  // Update the last message received time
  lastMessageReceivedTime = millis();
}

// Setup function
void setup() {
  // Initialize serial port
  Serial.begin(115200);

  // Initialize LCD display
  lcd.init();
  lcd.backlight();
  
  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(callback);

  // Connect to WiFi and MQTT
  reconnect();

  // Setup progress bar
  setup_progressbar();

  // setup custom chars
  lcd.createChar(6, customTapSymbol);
 
}

// Loop function
void loop() {
  // Reconnect to WiFi and MQTT if necessary
  reconnect();
  
  // Check for incoming messages on MQTT topic
  mqttClient.loop();

  // Display "No data" if no message has been received for 1 minute and "No data" is not already shown
  if (millis() - lastMessageReceivedTime > 60000 && !noDataShown) {
    lcd.clear();
    lcd.print("No data");
    lcd.noBacklight();
    noDataShown = true;
  }

  // Blink the LCD display light if the received value is less than 20
  // And the device is not starting up
  if (receivedValue < 20 && lastMessageReceivedTime != 0) {
    lcd.noBacklight();
    delay(500);
    lcd.backlight();
    delay(500);
  }
}

/*
   This is the method that does all the work on the progress bar.
   Please feel free to use this in your own code.
   @param count = the current number in the count progress
   @param totalCount = the total number to count to
   @param lineToPrintOn = the line of the LCD to print on.

   Because I am using a 16 x 2 display, I have 16 characters.  
   Each character has 5 sections.  Therefore, I need to declare the number 80.0.
   If you had a 20 x 4 display, you would have 25 x 5 = 100 columns.  Therefore you would change the 80.0 to 100.0
   You MUST have the .0 in the number.  If not, it will be treated as an int and will not calculate correctly

   The factor is the totalCount/divided by the number of columns.
   The percentage is the count divided by the factor (so for 80 columns, this will give you a number between 0 and 80)
   the number gives you the character number (so for a 16 x 2 display, this will be between 0 and 16)
   the remainder gives you the part character number, so returns a number between 0 and 4

   Based on the number and remainder values, the appropriate character is drawn on the screen to show progress.
   This will work with fluctuating values!
*/
void updateProgressBar(unsigned long count, unsigned long totalCount, int lineToPrintOn)
{
  double factor = totalCount / 100.0;        //See note above!
  int percent = count / factor;
  int number = percent / 5;
  int remainder = percent % 5;
  if (number > 0)
  {
    for (int j = 0; j < number; j++)
    {
      lcd.setCursor(j, lineToPrintOn);
      lcd.write(5);
    }
  }
  lcd.setCursor(number, lineToPrintOn);
  if (number < 20)
  {
    lcd.write(remainder);
    for (int j = number + 1; j < 20; j++)
    {
      lcd.setCursor(j, lineToPrintOn);
      lcd.write(0);
    }
  }
}

void updateNumber(int number) {
  lcd.setCursor(0, 0);
  lcd.print("                    "); //clear line
  
  lcd.setCursor(0, 0);
  lcd.write(6); //display tap icon
  
  int digits = numDigits(number);
  int startCol = max(0, 20 - 1 - digits); 
  lcd.setCursor(startCol, 0);
  lcd.print(number);

  lcd.setCursor(19, 0);
  lcd.print("%");
}

int numDigits(int number) {
  if (number == 0) return 1;
  int digits = 0;
  while (number > 0) {
    number /= 10;
    digits++;
  }
  return digits;
}
