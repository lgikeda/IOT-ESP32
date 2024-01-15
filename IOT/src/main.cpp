#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

#define DHTPIN 35     
#define DHTTYPE DHT11

const char* ssid = "iPhone de Luis Gustavo";
const char* password = "Contra.12345";
const char* serverURL = "https://djangoiot.onrender.com/control_estado/iluminacion/";

bool estadoiluminacion;

const int sensorPin = 34;
const int motorPin = 5;
const int humedadUmbral = 20;
const int pinLed = 2;

//////  Definicion de objetos //////
DHT dht(DHTPIN, DHTTYPE);

//////  Funciones de comunicación con aplicación Django  //////
void enviarDatos(const String& nombre_sensor, int valor);
bool obtenerEstadoControl();

//////  Funciones de control con sensores //////
void riegoSuelo();
void temp();
void cambiaEstado();

////////////////////////  Bloque principal  /////////////////////////
void setup() {
  Serial.begin(115200);
  pinMode(sensorPin, INPUT_PULLDOWN);
  pinMode(motorPin, OUTPUT);
  pinMode(pinLed, OUTPUT);

  dht.begin();

  //////
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Conectando a WiFi...");
  }
  Serial.println("Conectado a la red WiFi");
}

void loop() {
  
  Serial.println("////////////////////////////////");
  Serial.println("Estado del control iluminacion es: " + String(estadoiluminacion));
  Serial.println("////////////////////////////////");
  riegoSuelo();
  temp();
  cambiaEstado();
  delay(1000);
}
////////////////////////////////////////////////////////////////////

//////  Definición de funciones //////
void cambiaEstado(){
  estadoiluminacion = obtenerEstadoControl();
  digitalWrite(pinLed,estadoiluminacion);
}

void temp(){
  float temperatura = dht.readTemperature();
  Serial.print("Temperatura: ");
  Serial.println(temperatura);

  // Verificar si la lectura fue exitosa
  if (!isnan(temperatura)) {
    Serial.print("Temperatura: ");
    Serial.print(temperatura);
    Serial.println(" °C");
    enviarDatos("temperatura", temperatura);
  } else {
    Serial.println("Error al leer la temperatura");
  }
  // En la plataforma analiza el valor, por encima de 40 grados, se enviará 
  // un correo al usuario.
}

void riegoSuelo(){
  int humedadSuelo = analogRead(sensorPin);
  int mapeado = map(humedadSuelo,4095,0,0,100);
  Serial.print("Humedad del suelo: ");
  Serial.println(mapeado);

  if (mapeado < humedadUmbral) {
    // Si la humedad es menor que el umbral, activar la bomba
    digitalWrite(motorPin, 1);
    Serial.println("Activando la bomba de agua");
    delay(500);
    digitalWrite(motorPin, 0);
  } else {
    // Si la humedad es suficiente, apagar la bomba
    digitalWrite(motorPin, 0);
    Serial.println("Desactivando la bomba de agua");
  }

  enviarDatos("humedad", mapeado);
  
  delay(5000);
}

void enviarDatos(const String& nombre_sensor, int valor) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("No hay conexión WiFi");
    return;
  }

  HTTPClient http;
  http.begin("https://djangoiot.onrender.com/recibirDatos/");
  http.addHeader("Content-Type", "application/json");

  String jsonPayload = "{\"nombre_sensor\":\"" + nombre_sensor + "\",\"valor\":" + String(valor) + "}";

  int httpCode = http.POST(jsonPayload);

  if (httpCode == HTTP_CODE_OK) {
    Serial.println("Solicitud HTTP exitosa");
    String payload = http.getString();
    Serial.println("Respuesta del servidor:");
    Serial.println(payload);
  } else if (httpCode == HTTP_CODE_MOVED_PERMANENTLY || httpCode == HTTP_CODE_FOUND) {
    // Manejar redirección manualmente
    String newLocation = http.header("Location");
    Serial.println("Redireccionando a: " + newLocation);

    http.end(); // Cerrar la conexión actual

    // Hacer una nueva solicitud a la ubicación redirigida
    http.begin(newLocation);
    http.addHeader("Content-Type", "application/json");
    httpCode = http.POST(jsonPayload);

    if (httpCode == HTTP_CODE_OK) {
      Serial.println("Solicitud HTTP exitosa después de redireccionamiento");
      String payload = http.getString();
      Serial.println("Respuesta del servidor:");
      Serial.println(payload);
    } else {
      Serial.println("Error en la solicitud HTTP después de redireccionamiento");
      Serial.println("Código de respuesta HTTP: " + String(httpCode));
    }
  } else {
    Serial.println("Error en la solicitud HTTP");
    Serial.println("Código de respuesta HTTP: " + String(httpCode));
  }

  http.end();
}

bool obtenerEstadoControl() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("No hay conexión WiFi");
    //return;
  }

  HTTPClient http;
  http.begin(serverURL);

  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    Serial.println("Solicitud HTTP exitosa");
    DynamicJsonDocument jsonDoc(1024);
    DeserializationError error = deserializeJson(jsonDoc, http.getString());

    if (error) {
      Serial.println("Error al analizar el JSON");
      //return;
    }

    String nombre = jsonDoc["nombre"];
    bool estado = jsonDoc["estado"];
    estadoiluminacion = estado;
    Serial.println("Nombre del control: " + nombre);
    Serial.println("Estado del control: " + String(estado ? "Encendido" : "Apagado"));
  } else {
    Serial.println("Error en la solicitud HTTP");
    Serial.println("Código de respuesta HTTP: " + String(httpCode));
  }

  http.end();

  return estadoiluminacion;
}