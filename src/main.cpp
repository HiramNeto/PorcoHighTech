#include <Arduino.h>
#include "DHT.h"
#define DHTPIN D2
#define DHTTYPE DHT22
#define ACRELAYPIN D1
#define HUMRELAYPIN D5
#define EXHRELAYPIN D6

DHT dht(DHTPIN, DHTTYPE);

static float tempSum[10];
static float humSum[10];

void setup() {
  pinMode(ACRELAYPIN, OUTPUT);
  pinMode(HUMRELAYPIN, OUTPUT);
  pinMode(EXHRELAYPIN, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("DHTxx test!");
  dht.begin();
  for(uint8_t i=0;i<9;i++){
    tempSum[i]=0;
    humSum[i]=0;
  }
}

void loop() {
  static bool firstMeasLoop = true;
  static uint8_t loopAvgCounter = 0;
  static float temperatureAvg = 0;
  static float humidityAvg = 0;
  
  // put your main code here, to run repeatedly:
  delay(500); // Wait a few seconds between measurements
  float h = dht.readHumidity();
  // Reading temperature or humidity takes about 250 milliseconds!
  float t = dht.readTemperature();
  // Read temperature as Celsius (the default)
  float f = dht.readTemperature(true);
  // Read temperature as Fahrenheit (isFahrenheit = true)
  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t) || isnan(f)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }
  
  if(loopAvgCounter<9){
    tempSum[loopAvgCounter] = t;
    humSum[loopAvgCounter] = h;
    loopAvgCounter++;
  }else{
    loopAvgCounter = 0;
    firstMeasLoop = false;
  }

  if(firstMeasLoop){
    return;
  }

  for(uint8_t i = 0; i<9;i++){
    temperatureAvg += tempSum[i];
    humidityAvg += humSum[i];
  }
  temperatureAvg = temperatureAvg/10;
  humidityAvg = humidityAvg/10;

  /************************************************/
  //Temperature controller
  float tempSetpoint = 24;
  float tempSetpointHisteresys = 2;
 
  if(temperatureAvg < tempSetpoint-tempSetpointHisteresys){
    digitalWrite(ACRELAYPIN, LOW);
    digitalWrite(LED_BUILTIN, HIGH);
  }else if(temperatureAvg >= tempSetpoint+tempSetpointHisteresys){
    digitalWrite(ACRELAYPIN, HIGH);
    digitalWrite(LED_BUILTIN, LOW);
  }
  /************************************************/

  /************************************************/
  //Humidity controller
  float humSetpoint = 85;
  float humSetpointHisteresys = 2;
 
  if(humidityAvg > humSetpoint+humSetpointHisteresys){
    digitalWrite(HUMRELAYPIN, LOW);
  }else if(humidityAvg <= humSetpoint-humSetpointHisteresys){
    digitalWrite(HUMRELAYPIN, HIGH);
  }
  /************************************************/

  /************************************************/
  //Exhaust Controller
  float exhaustHumSetpoint = 90;
  float exhaustHumSetpointHisteresys = 2;

  if(humidityAvg < exhaustHumSetpoint-exhaustHumSetpointHisteresys){
    digitalWrite(ACRELAYPIN, LOW);
  }else if(humidityAvg >= exhaustHumSetpoint+exhaustHumSetpointHisteresys){
    digitalWrite(ACRELAYPIN, HIGH);
  }
  /************************************************/

  // Compute heat index in Fahrenheit (the default)
  float hif = dht.computeHeatIndex(f, h);
  // Compute heat index in Celsius (isFahreheit = false)
  float hic = dht.computeHeatIndex(t, h, false);
  Serial.print ("Humidity: ");
  Serial.print (humidityAvg);
  Serial.print (" %\t");
  Serial.print ("Temperature: ");
  Serial.print (temperatureAvg);
  Serial.print (" *C ");
  Serial.print (f);
  Serial.print (" *F\t");
  Serial.print ("Heat index: ");
  Serial.print (hic);
  Serial.print (" *C ");
  Serial.print (hif);
  Serial.println (" *F");
}