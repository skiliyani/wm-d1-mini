#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <LiquidCrystal_I2C.h>

// WiFi credentials
const char* ssid = "SAYANI_WIFI";
const char* password = "00011101";

// MQTT broker settings
const char* mqtt_server = "192.168.8.10";
const int mqtt_port = 1883;
const char* mqtt_topic = "home/ui/water-tank/level";

// LCD display settings
LiquidCrystal_I2C lcd(0x27, 16, 2); // I2C address 0x27

// Wifi Client
WiFiClient wifiClient;

// MQTT client
PubSubClient mqttClient(wifiClient);

// Progress bar variables
int progress_bar_length = 10;
int progress_bar_position = 0;

// Flag to indicate if "No data" is already shown
bool noDataShown = false;

// Last time a message was received
unsigned long lastMessageReceivedTime = 0;

int receivedValue = 0;

// Configure progress bar
const int LCD_NB_ROWS = 2;
const int LCD_NB_COLUMNS = 16;

/* Caractères personnalisés */
byte START_DIV_0_OF_1[8] = {
  B01111, 
  B11000,
  B10000,
  B10000,
  B10000,
  B10000,
  B11000,
  B01111
}; // Char début 0 / 1

byte START_DIV_1_OF_1[8] = {
  B01111, 
  B11000,
  B10011,
  B10111,
  B10111,
  B10011,
  B11000,
  B01111
}; // Char début 1 / 1

byte DIV_0_OF_2[8] = {
  B11111, 
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B00000,
  B11111
}; // Char milieu 0 / 2

byte DIV_1_OF_2[8] = {
  B11111, 
  B00000,
  B11000,
  B11000,
  B11000,
  B11000,
  B00000,
  B11111
}; // Char milieu 1 / 2

byte DIV_2_OF_2[8] = {
  B11111, 
  B00000,
  B11011,
  B11011,
  B11011,
  B11011,
  B00000,
  B11111
}; // Char milieu 2 / 2

byte END_DIV_0_OF_1[8] = {
  B11110, 
  B00011,
  B00001,
  B00001,
  B00001,
  B00001,
  B00011,
  B11110
}; // Char fin 0 / 1

byte END_DIV_1_OF_1[8] = {
  B11110, 
  B00011,
  B11001,
  B11101,
  B11101,
  B11001,
  B00011,
  B11110
}; // Char fin 1 / 1


/**
 * Fonction de configuration de l'écran LCD pour la barre de progression.
 * Utilise les caractères personnalisés de 0 à 6 (7 reste disponible).
 */
void setup_progressbar() {

  /* Enregistre les caractères personnalisés dans la mémoire de l'écran LCD */
  lcd.createChar(0, START_DIV_0_OF_1);
  lcd.createChar(1, START_DIV_1_OF_1);
  lcd.createChar(2, DIV_0_OF_2);
  lcd.createChar(3, DIV_1_OF_2);
  lcd.createChar(4, DIV_2_OF_2);
  lcd.createChar(5, END_DIV_0_OF_1);
  lcd.createChar(6, END_DIV_1_OF_1);
}

/**
 * Fonction dessinant la barre de progression.
 *
 * @param percent Le pourcentage à afficher.
 */
void draw_progressbar(byte percent) {
 
  /* Affiche la nouvelle valeur sous forme numérique sur la première ligne */
  lcd.setCursor(0, 0);
  lcd.print(F("WATER TANK ")); /* 11 chars */
  lcd.print(percent);/* 1 to 3 chars */
  lcd.print(F("%   "));/* 4 chars */
  
  // N.B. Les deux espaces en fin de ligne permettent d'effacer les chiffres du pourcentage
  // précédent quand on passe d'une valeur à deux ou trois chiffres à une valeur à deux ou un chiffres.
 
  /* Déplace le curseur sur la seconde ligne */
  lcd.setCursor(0, 1);
 
  /* Map la plage (0 ~ 100) vers la plage (0 ~ LCD_NB_COLUMNS * 2 - 2) */
  byte nb_columns = map(percent, 0, 100, 0, LCD_NB_COLUMNS * 2 - 2);
  // Chaque caractère affiche 2 barres verticales, mais le premier et dernier caractère n'en affiche qu'une.

  /* Dessine chaque caractère de la ligne */
  for (byte i = 0; i < LCD_NB_COLUMNS; ++i) {

    if (i == 0) { // Premiére case

      /* Affiche le char de début en fonction du nombre de colonnes */
      if (nb_columns > 0) {
        lcd.write(1); // Char début 1 / 1
        nb_columns -= 1;

      } else {
        lcd.write((byte) 0); // Char début 0 / 1
      }

    } else if (i == LCD_NB_COLUMNS -1) { // Derniére case

      /* Affiche le char de fin en fonction du nombre de colonnes */
      if (nb_columns > 0) {
        lcd.write(6); // Char fin 1 / 1

      } else {
        lcd.write(5); // Char fin 0 / 1
      }

    } else { // Autres cases

      /* Affiche le char adéquat en fonction du nombre de colonnes */
      if (nb_columns >= 2) {
        lcd.write(4); // Char div 2 / 2
        nb_columns -= 2;

      } else if (nb_columns == 1) {
        lcd.write(3); // Char div 1 / 2
        nb_columns -= 1;

      } else {
        lcd.write(2); // Char div 0 / 2
      }
    }
  }
}

// Reconnect to WiFi and MQTT broker
void reconnect() {
  if (!WiFi.isConnected()) {
    Serial.println("Reconnecting to WiFi...");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.println("Connected to the WiFi network");
  }

  if (!mqttClient.connected()) {
    Serial.println("Reconnecting to MQTT broker...");
    while (!mqttClient.connect("ESP8266")) {
      delay(500);
      Serial.print(".");
    }
    mqttClient.subscribe(mqtt_topic);
    Serial.println("Connected to the MQTT broker");
  }
}

// Progress bar display function
void displayProgressBar(int percentage) {
  if (noDataShown) {
    lcd.backlight();
    // Set the flag to indicate that "No data" is not shown
    noDataShown = false;
  }

  draw_progressbar(percentage);

  // Set the flag to indicate that "No data" is not shown
  noDataShown = false;
}

// MQTT callback function
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.println("Message arrived on topic: " + String(topic));

  // Convert the payload to an integer
  String message = "";
  for (int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  receivedValue = message.toInt();
  Serial.println("Received value: " + String(receivedValue));

  // Display the progress bar and percentage
  displayProgressBar(receivedValue);

  // Update the last message received time
  lastMessageReceivedTime = millis();
}

// Setup function
void setup() {
  // Initialize serial port
  Serial.begin(115200);

  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(callback);

  // Connect to WiFi and MQTT
  reconnect();

  // Initialize LCD display
  lcd.init();
  lcd.backlight();

  // Setup progress bar
  setup_progressbar();
}

// Loop function
void loop() {
  // Check for incoming messages on MQTT topic
  mqttClient.loop();

  // Display "No data" if no message has been received for 1 minute and "No data" is not already shown
  if (millis() - lastMessageReceivedTime > 60000 && !noDataShown) {
    lcd.clear();
    lcd.print("No data");
    lcd.noBacklight();
    noDataShown = true;
  }

  // Reconnect to WiFi and MQTT if necessary
  reconnect();
}
