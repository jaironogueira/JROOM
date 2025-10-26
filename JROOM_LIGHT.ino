/*
______/\\\\\\\\\\\____/\\\\\\\\\___________/\\\\\____________/\\\\\_______/\\\\____________/\\\\____________/\\\________/\\\_____/\\\\\\\\\\____________         
 _____\/////\\\///___/\\\///////\\\_______/\\\///\\\________/\\\///\\\____\/\\\\\\________/\\\\\\___________\/\\\_______\/\\\___/\\\///////\\\___________        
  _________\/\\\_____\/\\\_____\/\\\_____/\\\/__\///\\\____/\\\/__\///\\\__\/\\\//\\\____/\\\//\\\___________\//\\\______/\\\___\///______/\\\____________       
   _________\/\\\_____\/\\\\\\\\\\\/_____/\\\______\//\\\__/\\\______\//\\\_\/\\\\///\\\/\\\/_\/\\\____________\//\\\____/\\\___________/\\\//_____________      
    _________\/\\\_____\/\\\//////\\\____\/\\\_______\/\\\_\/\\\_______\/\\\_\/\\\__\///\\\/___\/\\\_____________\//\\\__/\\\___________\////\\\____________     
     _________\/\\\_____\/\\\____\//\\\___\//\\\______/\\\__\//\\\______/\\\__\/\\\____\///_____\/\\\______________\//\\\/\\\_______________\//\\\___________    
      __/\\\___\/\\\_____\/\\\_____\//\\\___\///\\\__/\\\_____\///\\\__/\\\____\/\\\_____________\/\\\_______________\//\\\\\_______/\\\______/\\\____________   
       _\//\\\\\\\\\______\/\\\______\//\\\____\///\\\\\/________\///\\\\\/_____\/\\\_____________\/\\\________________\//\\\_______\///\\\\\\\\\/_____________  
        __\/////////_______\///________\///_______\/////____________\/////_______\///______________\///__________________\///__________\/////////_______________ 
*/

#define ENABLE_USER_AUTH
#define ENABLE_DATABASE

#include <Arduino.h>
#include <DHT.h>
#include "time.h"
#include <neotimer.h>
#include <FirebaseClient.h>
#include "ExampleFunctions.h" // Provides the functions used in the examples.

#define WIFI_SSID "SUA REDE WIFI"
#define WIFI_PASSWORD "SENHA WIFI"

#define API_KEY "API KEY DO FIREBASE"
#define USER_EMAIL "EMAIL FIREBASE"
#define USER_PASSWORD "SENHA FIREBASE"
#define DATABASE_URL "URL FIREBASE"

void processData(AsyncResult &aResult);
SSL_CLIENT ssl_client;
using AsyncClient = AsyncClientClass;
AsyncClient aClient(ssl_client);
UserAuth user_auth(API_KEY, USER_EMAIL, USER_PASSWORD, 3000 /* expire period in seconds (<3600) */);
FirebaseApp app;
RealtimeDatabase Database;
AsyncResult databaseResult;
bool taskComplete = false;

// VARIAVEIS DO SERVIDOR DE HORAS
struct tm timeinfo;
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -3*3600;
const int   daylightOffset_sec = 0;
long startsecondswd;   
String startTime;      
long stopsecondswd;             
String stopTime;
long nowseconds; 

// DEFINIÇÕES DE PINOS DA PLACA
#define DHTPIN 25 
#define BOMBA 32 
#define SETOR1 12

/* VARIAVEIS DO SENSOR DHT */
#define DHTTYPE DHT11
float Temp;
float Umidade;
DHT dht(DHTPIN, DHTTYPE); 

/* VARIAVEIS DO TIMER */
Neotimer timerTemp;
Neotimer timerConfigs;
Neotimer timerSinalWifi;


/* VARIAVEIS PARA CONTROLE DOS RELES E DADOS */
String WifiPath = "/WIFI/Sinal";
String bombaPath = "/BOMBA/";
String setorPath1 = "/SETOR1/";

String bombaID = "bomba1";
String setorID = "setor1";
int wifisignal;

void setup()
{
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  /* configuração dos pinos */
  pinMode(BOMBA, OUTPUT);
  pinMode(SETOR1, OUTPUT);

  /* TEMPO DE EXECUCAO DOS TIMERS */
  timerSinalWifi.set(31000);
  timerTemp.set(15500);
  timerConfigs.set(4300);
  Serial.begin(115200);
  dht.begin();
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.print("Connecting to Wi-Fi");
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(300);
    }
    Serial.println();
    Serial.print("Connected with IP: ");
    Serial.println(WiFi.localIP());
    Serial.println();
    Firebase.printf("Firebase Client v%s\n", FIREBASE_CLIENT_VERSION);
    set_ssl_client_insecure_and_buffer(ssl_client);
    Serial.println("Initializing app...");
    initializeApp(aClient, app, getAuth(user_auth), auth_debug_print, "🔐 authTask");
    app.getApp<RealtimeDatabase>(Database);
    Database.url(DATABASE_URL);
}

void loop()
{
    // To maintain the authentication and async tasks
    app.loop();
    if (app.ready() && !taskComplete)
    {
      if (timerTemp.repeat()) {leTemp();}
      if (timerConfigs.repeat()) {      
      getConfigs(bombaPath, bombaID, BOMBA); // She call me Mr. Boombastic, tell me fantastic
      getConfigs(setorPath1, setorID, SETOR1);
      }
      if (timerSinalWifi.repeat()) {sinalWifi();}
      
    }

}

void processData(AsyncResult &aResult)
{
    // Exits when no result is available when calling from the loop.
    if (!aResult.isResult())
        return;

    if (aResult.isEvent())
    {
        Firebase.printf("Event task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.eventLog().message().c_str(), aResult.eventLog().code());
    }

    if (aResult.isDebug())
    {
        Firebase.printf("Debug task: %s, msg: %s\n", aResult.uid().c_str(), aResult.debug().c_str());
    }

    if (aResult.isError())
    {
        Firebase.printf("Error task: %s, msg: %s, code: %d\n", aResult.uid().c_str(), aResult.error().message().c_str(), aResult.error().code());
    }

    if (aResult.available())
    {
        Firebase.printf("task: %s, payload: %s\n", aResult.uid().c_str(), aResult.c_str());
    }
}


void leTemp()
{
  Umidade = dht.readHumidity();
  Temp = dht.readTemperature();   
  if (isnan(Umidade) || isnan(Temp) ) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }
  else
  {
  Serial.println("Temperatura: "+ String(Temp)+ " | Umidade: "+ String(Umidade));
  setFloat("/Temp/",Temp);
  setFloat("/Umid/",Umidade);
  }
}

String getData(String path)
{
  Serial.println("Obtendo valor de: " + path);
  String valor = Database.get<String>(aClient, path);
  return valor;
}

bool getBoolData(String path)
{
   Serial.println("Obtendo valor de: " + path);
   bool valor = Database.get<bool>(aClient, path);
   return valor;
  }

String getValue(String data, char separator, int index)
{
    int found = 0;
    int strIndex[] = { 0, -1 };
    int maxIndex = data.length() - 1;

    for (int i = 0; i <= maxIndex && found <= index; i++) {
        if (data.charAt(i) == separator || i == maxIndex) {
            found++;
            strIndex[0] = strIndex[1] + 1;
            strIndex[1] = (i == maxIndex) ? i+1 : i;
        }
    }
    return found > index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void setData(String path, String stringValor)
{
  Database.set<String>(aClient, path, stringValor, processData, "setString Task");
}
void setFloat (String path, float value)
{
  Database.set<number_t>(aClient, path, number_t(value, 2), processData, "setFloat Task");
}

void getConfigs(String path, String pathID, int pinOp)
{   
  if(!getLocalTime(&timeinfo)){
  Serial.println("Failed to obtain time");
  return;
  } 
  bool Status = getBoolData(path+pathID);  
  if(Status==true)
  { Serial.println("\n\n\n\n\n\n\n\n\n\n\n\n\n\n");
  Serial.print("BOTAO ");Serial.print(path);Serial.print(pathID);Serial.print(" ESTA LIGADO"); Serial.println();  
  nowseconds = ((timeinfo.tm_hour * 3600) + (timeinfo.tm_min * 60) + timeinfo.tm_sec);
  startTime = getData(path+"start");
  stopTime = getData(path+"stop");
  String xval = getValue(startTime, ':', 0); String yval = getValue(startTime, ':', 1);String zval = getValue(startTime, ':', 2);
  int xvalue = xval.toInt(); int yvalue = yval.toInt();int zvalue = zval.toInt();
  startsecondswd = ((xvalue * 3600) + (yvalue* 60) + zvalue);
  String aval = getValue(stopTime, ':', 0);String bval = getValue(stopTime, ':', 1);String cval = getValue(stopTime, ':', 2);
  int avalue = aval.toInt();int bvalue = bval.toInt();int cvalue = cval.toInt();
  stopsecondswd = ((avalue * 3600) + (bvalue* 60) + cvalue);
  if(nowseconds >= startsecondswd){    
  if(nowseconds <= startsecondswd + 90)
      {
       Serial.println("OPERADOR ");Serial.print(path);Serial.print(pathID);Serial.print(" SERA LIGADO");           
       digitalWrite(pinOp, LOW); // set LED ON          
      }      
      }
      else{ 
      Serial.println("OPERADOR ");Serial.print(path);Serial.print(pathID);Serial.print(" SERA DESLIGADO"); 
      digitalWrite(pinOp, HIGH);
      }
 
    
    if(nowseconds >= stopsecondswd){ 
      Serial.println("OPERADOR ");Serial.print(path);Serial.print(pathID);Serial.print(" SERA DESLIGADO"); 

      digitalWrite(pinOp, HIGH); // set LED OFF
    
      if(nowseconds <= stopsecondswd + 90)
      {          
        Serial.println("OPERADOR ");Serial.print(path);Serial.print(pathID);Serial.print(" SERA DESLIGADO"); 
        digitalWrite(pinOp, HIGH); // set LED OFF
             
      }              
    }
    else{
        if(nowseconds >= startsecondswd){  
        Serial.println("OPERADOR ");Serial.print(path);Serial.print(pathID);Serial.print(" SERA LIGADO"); 
        digitalWrite(pinOp, LOW); // set LED ON 
               
      }          
    }
  }
  else if(Status==false)
  { Serial.println("BOTAO ");Serial.print(path);Serial.print(pathID);Serial.print(" ESTA DESLIGADO"); 
    digitalWrite(pinOp, HIGH);}
}

void sinalWifi()
{
  String hora = String(timeinfo.tm_hour)+":" + String(timeinfo.tm_min)+":" + String(timeinfo.tm_sec);
  wifisignal = map(int(WiFi.RSSI()), -90, -25 , 0, 100);
  String sinal = WiFi.SSID()+": "+String(wifisignal)+"% - "+WiFi.RSSI()+ "dBm - "+hora ;// + timeinfo.tm_hour +":"+timeinfo.tm_min+":"+timeinfo.tm_sec ;
  setData(WifiPath, sinal);
}


