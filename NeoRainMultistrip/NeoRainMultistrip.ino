/***********************************************************************************************
 * Programa: Neo Rain Multistrip
 * 
 * Autor: Juanjo Soriano
 * Contacto: juanjosoriano[arroba]gmail.com
 * 
 * Descripción:
 *** Teniendo un número configurable de tiras neopixel con el mismo número de puntos se crea
 *** un efecto lluvia.
 ***
 *** Se puede actualizar vía OTA.
 *** 
 *** Inicialmente está configurado para 5 tiras de 30 LED para un Wemos D1 Mini en los pines
 *** indicados más abajo. El tiempo de espera está entre 1 y 5 segundos y la velocidad entre
 *** 3 y 10 LED por segundo. El efecto tendrá 4 LED (más el quinto que se deja apagado siempre).
 *
 **********************************************************************************************/
#include <FastLED.h>
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

#ifndef STASSID
#define STASSID "HOME_SWEET_HOME"   // OPCION: El nombre de tu WiFi para poder usar OTA
#define STAPSK  "M4ndr4k312345"     // OPCION: La password de tu WiFi para poder usar OTA
#endif

const char* ssid = STASSID;     // ¡¡¡NO TOCAR!!!
const char* password = STAPSK;  // ¡¡¡NO TOCAR!!!

#define NUM_LEDS 30           // OPCION: El número de píxeles que tiene cada tira
#define NUM_STRIPS 5          // OPCION: El número de tiras que hay en total (anotar los pins de control más abajo)
#define NUM_COLORS 5          // OPCION: Número total de colores a usar para el efecto de la lluvia (incluye negro)
#define NUM_SETS 4            // OPCION: Número de combinaciones de colores entre los que elegir
#define MIN_WAIT_TIME 200     // OPCION: Tiempo mínimo de espera (en ms)
#define MAX_WAIT_TIME 1000    // OPCION: Tiempo máximo de espera (en ms)
#define MIN_RAIN_SPEED 5      // OPCION: Velocidad mínima de la lluvia (en LED/seg)
#define MAX_RAIN_SPEED 10     // OPCION: Velocidad máxima de la lluvia (en LED/seg)

#define STRIP_1_PIN 4         // OPCION: El pin de cada tira (modifica el setup() para que coincida con el número de tiras) (D2)
#define STRIP_2_PIN 5         // OPCION: El pin de cada tira (modifica el setup() para que coincida con el número de tiras) (D1)
#define STRIP_3_PIN 12        // OPCION: El pin de cada tira (modifica el setup() para que coincida con el número de tiras) (D6)
#define STRIP_4_PIN 13        // OPCION: El pin de cada tira (modifica el setup() para que coincida con el número de tiras) (D7)
#define STRIP_5_PIN 14        // OPCION: El pin de cada tira (modifica el setup() para que coincida con el número de tiras) (D5)

/**** VARIABLES GLOBALES PARA CONTROLAR EL EFECTO ****/
uint8_t currentLed[NUM_STRIPS];       // Controla en que LED está el punto más luminoso
uint8_t rainSpeed[NUM_STRIPS];        // Controla la velocidad a la que irá la lluvia
uint8_t colorSet[NUM_STRIPS];         // Controla el set de colores para cada tira
long waitTime[NUM_STRIPS];            // Controla cuánto se debe esperar para redibujar
long initialTime[NUM_STRIPS];         // Controla desde cuándo se debe contar el tiempo de espera
bool isRaining[NUM_STRIPS];           // Controla cuándo ha acabado la caída de la gota

CRGB ledColors[NUM_STRIPS][NUM_LEDS]; // El color que debe mostrar cada LED en una matriz bidimensional (tabla)

CRGB rainColors[NUM_SETS][NUM_COLORS] = {       // OPCION: Colores para el efecto, de más a menos luminoso (incluye apagado) ¡CAMBIAR NUM_COLORS arriba!
  {                                             // Color azul
    CRGB(40, 40, 150),
    CRGB(20, 20, 75),
    CRGB(2, 2, 10),
    CRGB(0, 0, 1),
    CRGB::Black
  },
  {                                             // Color verde
    CRGB(15, 70, 15),
    CRGB(7, 50, 7),
    CRGB(1, 6, 1),
    CRGB(0, 1, 0),
    CRGB::Black
  },
  {                                             // Color naranja
    CRGB(150, 35, 0),
    CRGB(75, 15, 0),
    CRGB(10, 2, 0),
    CRGB(6, 1, 0),
    CRGB::Black
  },
  {                                             // Color amarillo
    CRGB(150, 70, 0),
    CRGB(75, 37, 0),
    CRGB(10, 5, 0),
    CRGB(6, 3, 0),
    CRGB::Black
  }
};

long currentTime;                     // El tiempo medido al inicio del loop()


/*************************************************************
 * Función de preparación para poder usar OTA (Over The Air) *
 * No se detalla ya que es un copia/pega de la librería      *
 * Crédito: Ejemplo de la librería estándar                  *
 ************************************************************/
void OTASetup(){
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
  }

  ArduinoOTA.setHostname("NeoRainMulti");

  ArduinoOTA.onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH) {
      type = "sketch";
    } else { // U_FS
      type = "filesystem";
    }
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      Serial.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      Serial.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      Serial.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      Serial.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      Serial.println("End Failed");
    }
  });
  
  ArduinoOTA.begin();
  Serial.println("Listo");
  Serial.print("Direccion IP: ");
  Serial.println(WiFi.localIP());
}

/********************************************************************************
 * Función setup obligatoria en Arduino donde preparamos las tiras LED y el OTA *
 *******************************************************************************/

void setup() {
  Serial.begin(115200);                                                 // Inicializamos el puerto serie para los mensajes de OTASetup()

  OTASetup();                                                           // Preparar el OTA

  randomSeed(analogRead(0));                                            // Inicializamos la semilla del código pseudo-aleatorio

  FastLED.addLeds<NEOPIXEL, STRIP_1_PIN>(ledColors[0], NUM_LEDS);       // Inicializamos las tiras LED
  FastLED.addLeds<NEOPIXEL, STRIP_2_PIN>(ledColors[1], NUM_LEDS);       //
  FastLED.addLeds<NEOPIXEL, STRIP_3_PIN>(ledColors[2], NUM_LEDS);       // OPCION: Añadir o quitar líneas si se cambia el número de tiras
  FastLED.addLeds<NEOPIXEL, STRIP_4_PIN>(ledColors[3], NUM_LEDS);       //
  FastLED.addLeds<NEOPIXEL, STRIP_5_PIN>(ledColors[4], NUM_LEDS);       //
  for (int i = 0; i < NUM_STRIPS; i++){                                 // Bucle de inicialización de las tiras
    currentLed[i] = 0;                                                    // La posición de la gota estará al principio
    isRaining[i] = false;                                                 // No está realizando la lluvia
    for (int j = 0; j < NUM_LEDS; j++){                                   // Bucle para apagar todos los LED
      ledColors[i][j] = CRGB::Black;                                        // Cada LED en negro
    }
  }
  FastLED.show();                                                       // Mostrar los LED como los hemos preparado (todo apagado)
}

/************************************************************
 * Función para preparar cada tira LED antes del redibujado *
 ************************************************************/

void prepareStrip(uint8_t strip){
  if (currentTime - initialTime[strip] >= waitTime[strip]){                     // Si ya ha pasado el tiempo de espera
    waitTime[strip] = 1000 / rainSpeed[strip];                                    // Recalculamos el tiempo de espera según la velocidad
    initialTime[strip] = currentTime;                                             // El nuevo punto de inicio de espera
    for (int i = 0; i < NUM_COLORS; i++){                                         // Bucle para cada color del efecto
      if (currentLed[strip] - i < 0) break;                                        // Si estamos al principio desechamos la cola saliendo del bucle
      if (currentLed[strip] - i < NUM_LEDS){                                        // Si estamos al final los puntos más lumnisos no se pintan
        ledColors[strip][currentLed[strip] - i] = rainColors[colorSet[strip]][i];   // Pintamos el LED
      }
    }
    currentLed[strip]++;                                                          // Avanzamos la posición del inicio de la gota
    if (currentLed[strip] >= NUM_LEDS + NUM_COLORS - 1){                          // Si ya ha acabado
      isRaining[strip] = false;                                                     // Ya no está lloviendo y se deberá resetear con nueva velocidad
    }
  }
}

/***********************************************************************************
 * Bucle principal obligatorio en Arduino donde se realiza el control del programa *
 **********************************************************************************/

void loop() {
  ArduinoOTA.handle();                                          // Cedemos el control por si ha venido una actualización OTA

  currentTime = millis();                                       // Guardamos el tiempo actual para posteriores cálculos de tiempos de espera
  for (uint8_t i = 0; i < NUM_STRIPS; i++){                     // Bucle para pasar por cada tira LED
    if (!isRaining[i]){                                           // Si no está dibujando (primera pasada o tras acabar una)
      initialTime[i] = currentTime;                                 // El tiempo a contar para la espera será desde el tiempo actual
      currentLed[i] = 0;                                            // Reseteamos la posición de la gota
      waitTime[i] = random(MIN_WAIT_TIME, MAX_WAIT_TIME);           // Calculamos un tiempo de espera inicial aleatorio
      rainSpeed[i] = random(MIN_RAIN_SPEED, MAX_RAIN_SPEED);        // Calculamos una velocidad aleatoria
      colorSet[i] = random(0, NUM_SETS);                            // Calculamos un set de colores aleatorio
      isRaining[i] = true;                                          // Indicamos que está lloviendo en esta tira
    }
    prepareStrip(i);                                             // Llamamos a la función para dibujar la tira
  }
  FastLED.show();                                               // Actualizamos los LED que hemos preparado
}
