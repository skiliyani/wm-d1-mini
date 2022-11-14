#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <LiquidCrystal_I2C.h>

const char* SSID = "SAYANI_WIFI";
const char* PASSWORD = "00011101";
const char* MQTT_SERVER = "192.168.8.10"; // pi4

const int LCD_NB_ROWS = 2;
const int LCD_NB_COLUMNS = 16;

// Set the LCD address to 0x27 for a 16 chars and 2 line display
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Wifi Client
WiFiClient wifiClient;

//MQTT Client
PubSubClient pubSubClient(wifiClient);

// Keep track of time
unsigned long mqtt_last_message_millis = millis();

// Track NoData state
bool noData = false;

// Last read data
byte lastReceivedData = -1;

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

bool backlight = true;
unsigned long previousMillis = 0; 
void blinkBackLight() {
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= 1000) {
    previousMillis = currentMillis;

    if(backlight) {
      lcd.noBacklight(); 
    } else {
      lcd.backlight();
    }
   backlight = !backlight;
  }
 
}

void setup_wifi() {

  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(SSID);
  
  lcd.clear();
  lcd.print("WiFi Connecting");

  WiFi.begin(SSID, PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());

  lcd.clear();
  lcd.print("WiFi connected");
  lcd.setCursor(0,1);
  lcd.print(WiFi.localIP());
  
  delay(2000);
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();
  
  if(strcmp(topic, "home/ui/water-tank/level") == 0) {

    char buffer[length];
    
    // Form a C-string from the payload
    memcpy(buffer, payload, length);
    buffer[length] = '\0';
    
    // Convert it to integer
    char *end = nullptr;
    long value = strtol(buffer, &end, 10);
  
    // Check for conversion errors
    if (end == buffer)
      ; // Conversion error occurred
    else
      Serial.println(value);
  
    if(noData == true) {
      noData = false;
      lcd.backlight();
    }      
  
    if(lastReceivedData <= 15) {
      lcd.backlight();
    }
  
    lastReceivedData = value;
    draw_progressbar(value);
  
    // update last message received time
    mqtt_last_message_millis = millis();
    
  } else if (strcmp(topic, "home/ui/kwa-water/pressure") == 0) {
    char buffer[length];
    
    // Form a C-string from the payload
    memcpy(buffer, payload, length);
    buffer[length] = '\0';
    
    // Convert it to integer
    char *end = nullptr;
    long value = strtol(buffer, &end, 10);
  
    // Check for conversion errors
    if (end == buffer)
      ; // Conversion error occurred
    else
      Serial.println(value);


    if(value >= 40) {
      digitalWrite(D0, HIGH); //LED on
    } else {
      digitalWrite(D0, LOW); 
    }
  }
  
}

void reconnect() {
  // Loop until we're reconnected
  while (!pubSubClient.connected()) {
    Serial.print("Attempting MQTT connection...");

    lcd.clear();
    lcd.setCursor(0,1);
    lcd.print("MQTT Connecting");
        
    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (pubSubClient.connect(clientId.c_str())) {
      Serial.println("connected");
      pubSubClient.subscribe("home/ui/#");
      lcd.clear();
      //lcd.print("MQTT Connected");
      lcd.setCursor(0,1);
      lcd.print("Waiting for data");      
    } else {
      Serial.print("failed, rc=");
      Serial.print(pubSubClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void setup() {
  pinMode(D0, OUTPUT);
  Serial.begin(115200);  
  
  lcd.init();  
  lcd.backlight();
  setup_progressbar();

  setup_wifi();
  
  pubSubClient.setServer(MQTT_SERVER, 1883);
  pubSubClient.setCallback(callback);
}


void loop() {
  if (!pubSubClient.connected()) {
    reconnect();
  }
  pubSubClient.loop();

  /* Don't show wrong readings if data not received for sometime */
  if (millis() - mqtt_last_message_millis > 5 * 60 * 1000 && noData == false) {
    noData = true;
    lcd.clear();
    lcd.print("No data");   
    lcd.noBacklight();
  }

  if (lastReceivedData > -1 and lastReceivedData <= 15) {
    blinkBackLight();
  }
} 
