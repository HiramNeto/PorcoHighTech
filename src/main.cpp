#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <time.h>
#include <sys/time.h>
#include <coredecls.h>
#include "DHT.h"
#include "HTTPSRedirect.h"
#include <ArduinoOTA.h>

//DHT Constants
#define DHTPIN D2
#define DHTTYPE DHT22

//IO Constants
#define ACRELAYPIN D1
#define HUMRELAYPIN D5
#define EXHRELAYPIN D6
#define DEHUMRELAYPIN D7

DHT dht(DHTPIN, DHTTYPE);

const char* ssid = "Hiram";
const char* password = "a32482154";

// TZ string setup
// TZ string information:
// https://www.gnu.org/software/libc/manual/html_node/TZ-Variable.html
const char *TZstr = "GMT-3";//"CST+6CDT,M3.2.0/2,M11.1.0/2";

static float tempSum[10];
static float humSum[10];
timeval cbtime;			// when time set callback was called
bool cbtime_set = false;

void time_is_set (void);

void time_is_set (void)
{
	time_t t = time (nullptr);

	gettimeofday (&cbtime, NULL);
	cbtime_set = true;
	Serial.println
		("------------------ settimeofday() was called ------------------");
	printf (" local asctime: %s\n",
	 asctime (localtime (&t)));	// print formated local time

	// set RTC using t if desired
}


HTTPSRedirect* client = nullptr;
const char* GScriptId = "AKfycbxZlcVlvaYalXT9I1oDoj5aAz5WOTmOY_inW2-wzB9j3NjVEfAf1K_BgbFYYl_dav-5sw";
String payload_base = "{\"command\": \"append_row\", \"sheet_name\": \"TemperaturaHumidade\", \"values\": ";
String payload = "";

const char* host        = "script.google.com";
const int   httpsPort   = 443;
String url = String("/macros/s/") + GScriptId + "/exec?cal";

void setup() {
  pinMode(ACRELAYPIN, OUTPUT);
  pinMode(HUMRELAYPIN, OUTPUT);
  pinMode(EXHRELAYPIN, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(DEHUMRELAYPIN,OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
  // put your setup code here, to run once:
  Serial.begin(115200);

  //Connecting to wifi
  WiFi.persistent(false);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(200);
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
  }
  digitalWrite(LED_BUILTIN, HIGH);

  ArduinoOTA.setHostname("PresuntoHighTech");
  ArduinoOTA.onStart([]() {
    Serial.println("Inicio...");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("nFim!");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progresso: %u%%r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Erro [%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Autenticacao Falhou");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Falha no Inicio");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Falha na Conexao");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Falha na Recepcao");
    else if (error == OTA_END_ERROR) Serial.println("Falha no Fim");
  });
  ArduinoOTA.begin();

  // optional: set function to call when time is set
	// is called by NTP code when NTP is used and NTP sets the time
	// it is called just *after* NTP sets the time
  settimeofday_cb (time_is_set);

  // DO NOT attempt to use the timezone offsets in configTime() !!!!!
	// configTime(tzoffset, dstflg, "ntp-server1", "ntp-server2", "ntp-server3");
	// The timezone offset code is really broken.
	// if used, then localtime() and gmtime() won't work correctly.
	// set both to zero and get real timezone & DST support by using TZ string
	// enable NTP by setting NTP server
	// up to 3 ntp servers can be specified
	// configTime(0, 0, "server1", "server2", "server3");

	// enable NTP by setting up NTP server(s)
	// up to 3 ntp servers can be specified
	// set both timezone offet and dst parameters to zero 
	// and get real timezone & DST support by using a TZ string
	// If not using a network, or NTP, don't use configTime()
  configTime (TZstr, "pool.ntp.org","time.nist.gov");

  Serial.println("DHTxx test!");
  dht.begin();
  for(uint8_t i=0;i<9;i++){
    tempSum[i]=0;
    humSum[i]=0;
  }

  //Coisas do google sheets q eu n entendi direito mas copiei do exemplo da internet em
  //https://www.filipeflop.com/blog/como-enviar-dados-do-esp8266-para-o-google-sheets/
  //HTTPS Redirect Setup
  client = new HTTPSRedirect(httpsPort);
  client->setInsecure();
  client->setPrintResponseBody(true);
  client->setContentTypeHeader("application/json");
  Serial.print("Conectando ao Google...");
 
  bool flag = false;
  for (int i=0; i<5; i++)
  { 
    int retval = client->connect(host, httpsPort);
    if (retval == 1)
    {
       flag = true;
       Serial.println("[OK]");
       break;
    }
    else
      Serial.println("[Error]");
  }
  if (!flag)
  {
    Serial.print("[Error]");
    Serial.println(host);
    return;
  }
  delete client;
  client = nullptr;
}

// for testing purpose:
extern "C" int clock_gettime (clockid_t unused, struct timespec *tp);
#define PTM(w) \
  Serial.print(":" #w "="); \
  Serial.print(tm->tm_##w);

void printTm (const char* what, const tm* tm)
{
  Serial.print(what);
  PTM(isdst); PTM(yday); PTM(wday);
  PTM(year);  PTM(mon);  PTM(mday);
  PTM(hour);  PTM(min);  PTM(sec);
}


timeval tv;
struct timezone tz;
timespec tp;
time_t tnow;

void printTimeNTP();
bool fiveMinuesHasPassed();
bool sendDataToGoogleSheets(float humidity, float temperature, uint32_t exhaustCounter, uint32_t acCounter, uint32_t humCounter, uint32_t deHumCounter);
void keepConnAlive();

void loop() {
  static bool firstMeasLoop = true;
  static uint8_t loopAvgCounter = 0;
  static float temperatureAvg = 0;
  static float humidityAvg = 0;
  static uint32_t acRelayCounter = 0;     //ToDo: this must be a RTC memory variable, to avoid value changes when resetting
  static uint32_t exhRelayCounter = 0;    //ToDo: this must be a RTC memory variable, to avoid value changes when resetting
  static uint32_t humRelayCounter = 0;    //ToDo: this must be a RTC memory variable, to avoid value changes when resetting
  static uint32_t deHumRelayCounter = 0;  //ToDo: this must be a RTC memory variable, to avoid value changes when resetting
  
  //Wait 500ms between measurements. Run ArduinoOTA during this time to avoid problems...
  for(uint8_t j = 1;j<=50;j++){
    ArduinoOTA.handle();
    delay(10);
  }

  // Reading temperature or humidity takes about 250 milliseconds!
  float h = dht.readHumidity();
  ArduinoOTA.handle();
  // Reading temperature or humidity takes about 250 milliseconds!
  float t = dht.readTemperature();
  ArduinoOTA.handle();
  // Read temperature as Celsius (the default)
  float f = dht.readTemperature(true);
  ArduinoOTA.handle();
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
  float tempSetpoint = 17;
  float tempSetpointHisteresys = 1;
  static unsigned long acRelayOnActivatedTime = 0;
  static unsigned long acRelayOffActivatedTime = millis();
  static bool acRelayLastState = false;
  unsigned long switchingInterval = 300000; //5 minutes
 
  if(temperatureAvg < tempSetpoint-tempSetpointHisteresys){
    //Check if at least switchingInterval has passed since the AC was turned on (avoid switching on/off too often)
    if((millis()-acRelayOnActivatedTime)>=switchingInterval){
      digitalWrite(ACRELAYPIN, LOW);
      digitalWrite(LED_BUILTIN, HIGH);
    }
  }else if(temperatureAvg >= tempSetpoint+tempSetpointHisteresys){
    //Check if at least switchingInterval has passed since the AC was turned on (avoid switching on/off too often)
    if((millis()-acRelayOffActivatedTime)>=switchingInterval){
      digitalWrite(ACRELAYPIN, HIGH);
      digitalWrite(LED_BUILTIN, LOW);
    }
  }

  //Get On/Off activation/deactivation times...
  if(digitalRead(ACRELAYPIN)){
    if(!acRelayLastState){
      acRelayLastState = true;
      acRelayOnActivatedTime = millis();
    }
  }else{
    if(acRelayLastState){
      acRelayLastState = false;
      acRelayOffActivatedTime = millis();
    }
  }

  /************************************************/

  /************************************************/
  //Humidity controller
  float humSetpoint = 80;
  float humSetpointHisteresys = 2;
 
  if(humidityAvg > humSetpoint+humSetpointHisteresys){
    digitalWrite(HUMRELAYPIN, LOW);
  }else if(humidityAvg <= humSetpoint-humSetpointHisteresys){
    digitalWrite(HUMRELAYPIN, HIGH);
  }
  /************************************************/

  /************************************************/
  //Dehumidifier + Exhaust Controller
  float exhaustHumSetpoint = 88;
  float exhaustHumSetpointHisteresys = 2;
  static uint32_t highHumidityCounter = 0;

  if(humidityAvg < exhaustHumSetpoint-exhaustHumSetpointHisteresys){
    //Check if we need to enable the exhaust because of the time...
    digitalWrite(EXHRELAYPIN, LOW);
    digitalWrite(DEHUMRELAYPIN, LOW);
    highHumidityCounter = 0;
  }else if(humidityAvg >= exhaustHumSetpoint+exhaustHumSetpointHisteresys){
    digitalWrite(DEHUMRELAYPIN, HIGH);
    if(highHumidityCounter>3600){//loop runs every 500ms, so if humidity is high for more than 30 minutes
      digitalWrite(EXHRELAYPIN, HIGH);
    }else{
      highHumidityCounter++;
    }
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
  if(fiveMinuesHasPassed()){
    if(!sendDataToGoogleSheets(humidityAvg,temperatureAvg,digitalRead(EXHRELAYPIN),digitalRead(ACRELAYPIN),digitalRead(EXHRELAYPIN))){
      //Microcontroller failed to send google sheet data...
    }
    acRelayCounter = 0;
    exhRelayCounter = 0;
    humRelayCounter = 0;
    deHumRelayCounter = 0;
  }else{
    //increase counter if output is enabled!
    acRelayCounter = (digitalRead(ACRELAYPIN))?acRelayCounter+1:acRelayCounter;
    exhRelayCounter = (digitalRead(EXHRELAYPIN))?exhRelayCounter+1:exhRelayCounter;
    humRelayCounter = (digitalRead(HUMRELAYPIN))?humRelayCounter+1:humRelayCounter;
    deHumRelayCounter = (digitalRead(DEHUMRELAYPIN))?deHumRelayCounter+1:deHumRelayCounter;
  }
  keepConnAlive();
}

void keepConnAlive(){
  static unsigned long previousMillis = 0;
  unsigned long interval = 30000;
  unsigned long currentMillis = millis();
  static unsigned long disconnectionTime = 0;
  static bool connectionState = true;
  
  // if WiFi is down, try reconnecting
  if ((WiFi.status() != WL_CONNECTED)) {
    //Get disconnection time
    if(connectionState){
      connectionState = false;
      disconnectionTime = millis();
    }

    //Check if reconnection time has passed since last reconnection try...
    if((currentMillis - previousMillis >=interval)){
      Serial.print(millis());
      Serial.println("Reconnecting to WiFi...");
      WiFi.disconnect();
      //WiFi.reconnect();
      WiFi.begin(ssid,password);
      previousMillis = currentMillis;
    }

    //Reset the microcontroller if there are more than 5 minutes that we cannot get wifi connection...
    if(millis()-disconnectionTime >= 300000){
      //Restart the microcontroller...
      ESP.reset();
    }
  }
  else{
    connectionState = true;
  }
}

bool sendDataToGoogleSheets(float humidity, float temperature, uint32_t exhaustCounter, uint32_t acCounter, uint32_t humCounter, uint32_t deHumCounter){
  bool returnValue = true;
  static bool flag = false;
  if (!flag)
  {
    client = new HTTPSRedirect(httpsPort);
    client->setInsecure();
    flag = true;
    client->setPrintResponseBody(true);
    client->setContentTypeHeader("application/json");
  }
  if (client != nullptr) { if (!client->connected()){ client->connect(host, httpsPort); } }
  else { Serial.println("[Error]"); returnValue = false;}
   
  payload = payload_base + "\"" + humidity + "," + temperature +  "," + exhaustCounter +  "," + acCounter +  "," + humCounter + "," + deHumCounter + "\"}";
   
  Serial.println("Enviando...");
  if(client->POST(url, host, payload)){ Serial.println(" [OK]"); }
  else { Serial.println("[Error]"); returnValue = false;}
  return returnValue;
}

bool fiveMinuesHasPassed(){
  gettimeofday (&tv, &tz);
	clock_gettime (0, &tp);	// also supported by esp8266 code
	tnow = time (nullptr);

	// localtime / gmtime every 5 minutes
	static time_t lastv = tv.tv_sec;
	if(tv.tv_sec >= lastv+300){
    lastv = tv.tv_sec;
    return true;
  }else{
    return false;
  }
}

void printTimeNTP(){
  Serial.println();
  Serial.println();
  #if 0
	if(!cbtime_set)		// don't do anything until NTP has set time
		return;
#endif

	gettimeofday (&tv, &tz);
	clock_gettime (0, &tp);	// also supported by esp8266 code
	tnow = time (nullptr);

	// localtime / gmtime every minute change
	static time_t lastv = tv.tv_sec;
	Serial.println(lastv);
  Serial.println(tv.tv_sec);
  if(tv.tv_sec >= lastv+60)
	{
		lastv = tv.tv_sec;
#if 0
		printf ("tz_minuteswest: %d, tz_dsttime: %d\n",
			tz.tz_minuteswest, tz.tz_dsttime);
		printf ("gettimeofday() tv.tv_sec : %ld\n", lastv);
		printf ("time()            time_t : %ld\n", tnow);
		Serial.println ();
#endif

		printf ("         ctime: %s", ctime (&tnow));	// print formated local time
		printf (" local asctime: %s", asctime (localtime (&tnow)));	// print formated local time
		printf ("gmtime asctime: %s", asctime (gmtime (&tnow)));	// print formated gm time

		// print gmtime and localtime tm members
		printTm ("      gmtime", gmtime (&tnow));
		Serial.println ();
		printTm ("   localtime", localtime (&tnow));
		Serial.println ();
		Serial.println ();
	}
}