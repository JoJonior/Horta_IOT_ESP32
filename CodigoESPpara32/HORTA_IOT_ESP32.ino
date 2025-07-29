#include <NTPClient.h>
#include <WiFiUdp.h>
#include <WiFi.h>
#include <ESPSupabase.h>
#include <DHT11.h>

#include <ArduinoOTA.h>
#include <ESPmDNS.h>


#include "secrets.h"




WiFiUDP ntpUDP;


NTPClient timeClient(ntpUDP,"pool.ntp.br",-3 * 3600, 60000);

Supabase db;
bool regaHorario = true;
bool regaUmidade = false; 





const String table = "Hortas";

bool valvula = false;
bool isRegando = false;
int pinValvula = 33; 
DHT11 dht11(4);
const int pinHigrometro = 34;
const int  pinDht11 = 4;

int temperatura = 0;
int umidadeDoArdht = 0;

const int pinVazao = 23;
unsigned long tempoLigado = 30;
unsigned long inicioTempo = 0;

volatile int pulseCount = 0;
unsigned long previousMillis = 0;
float flowRate = 0.0;
int totalPulso = 0; // remover
float water_use = 0.0;
int  umidadeSolo =0;
int start_irriga=0;
int end_irriga=0;
int time_or_humidty = 0; /// 1 time, 2, humidty, 0 app
float erroAcumulado = 0;
int flow_calibration = 432;

unsigned long unix_time = 0;



void pulseCounter() {
  pulseCount++;
}

void setup(){


  Serial.begin(115200);
  equipsSetup();
  conecta_wifi();

  

}

void loop() {

  Serial.println("INICIO Loop");
  unsigned long currentMillis = millis();


  //100

   
    

   if (currentMillis - previousMillis > 1000) {

   


    float segundosPassados = (currentMillis - previousMillis) / 1000.0;

    erroAcumulado += segundosPassados;
    int segundosInteiros = int(erroAcumulado); 
    erroAcumulado -= segundosInteiros; 

    //Serial.println(segundosPassados);
    unix_time+=(segundosInteiros);
    
    noInterrupts(); 

    totalPulso = totalPulso + pulseCount;
    if(pulseCount!=0){
      flowRate = ((pulseCount / flow_calibration) * 60.0);
    }
    
    pulseCount = 0;
    interrupts(); 
    float totlalitro = 0;
    if(totalPulso!=0){
       totlalitro = totalPulso / flow_calibration ;
    }

    ////Concertar
      
        water_use = totlalitro;
     
      //water_use = totlalitro;
      
    
    //Ser
    previousMillis = currentMillis;
    atualizarvalores();
    irrigar();
   }


  
  
  ///delay(1000);
  //irrigar();


  if( WiFi.status() != WL_CONNECTED ){
    ///Serial.print ( "WiFi desconectado. tentando reconectar." );
    conecta_wifi();
    
  }
  yield();
  Serial.println("FIM Loop");
  
}

void conecta_wifi(){
  
  WiFi.begin(ssid, password);

  while ( WiFi.status() != WL_CONNECTED ) {
    delay ( 500 );
    Serial.print ( "." );
  }
  ///Serial.print ( "conectou a rede WiFi." );
  timeClient.begin();

  

  db.begin(supabase_url, anon_key);
  db.login_email(email, password_user);

  ///Serial.begin("esp32");  // Nome do ESP no Telnet
  ///Serial.setSerialEnabled(true);  // Permite visualizar no Serial e no Telnet

  timeClient.update();
  unix_time=timeClient.getEpochTime();
  while(unix_time < 0) {  // Verifica se o Epoch é menor que 01/01/2020
        Serial.println("Erro: NTP não sincronizou corretamente.");
        timeClient.update();
        unix_time = timeClient.getEpochTime();
  }
      ///Serial.print("Tempo sincronizado! Epoch: ");
      //Serial.println(unix_time);


  

  ///Serial.println(unix_time);
  String hostname = String("esp32-") + id_horta;

  if (MDNS.begin(hostname.c_str())) {
      ///Serial.println("mDNS iniciado com sucesso!");
      ///Serial.println(hostname);
  }


}

String edit_string_time(unsigned long unix){
  String horas_str ="";
  String minutos_str = "";

  //int horas = (unix % 86400L) / 3600;
  int horas = (unix  / 3600) % 24;


  //int minutos = (unix % 3600) / 60;

  int minutos = (unix % 3600) / 60;

  horas_str = String(horas);
  minutos_str = String(minutos);
  
  return String(horas_str+":"+minutos_str);
}

void atualizarvalores(){

  String hora_certa = edit_string_time(unix_time);
  ///Serial.println(hora_certa);
  
  
  // GETS
  String read = db.from("Hortas").select("*").eq("id", id_horta).limit(1).doSelect();
  //Serial.println(read);
  db.urlQuery_reset();


  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, read);
  if (error) {
    ///Serial.println("ERROR json");
    //doc.clear();
    return;
  }
  if (doc.size() <= 0) {
    return;
  }
  String horarios =  doc[0]["horarios"];
  ///16012025
  
  valvula = doc[0]["valvula"];
  
  
  JsonArray horasLista = doc[0]["horarios"];
  ///Sensores

  int original =  analogRead(pinHigrometro);

  int molhado =doc[0]["calibragem"][0];
  int seco= doc[0]["calibragem"][1];
  umidadeSolo = int(map(original,molhado,seco,100,0));
  
  int umidadeMinima = doc[0]["calibragem"][2];
  int umidadeIdeal = doc[0]["calibragem"][3];
  tempoLigado = doc[0]["tempoLigado"];
  flow_calibration = doc[0]["flow_calibration"];
  //Serial.print("  flow_calibration");
  //Serial.println(flow_calibration);



  regaHorario = doc[0]["settings"][0];
  regaUmidade =  doc[0]["settings"][1];
  //Serial.println(String(umidadeSolo)+"%");
  ///Ligar ou Desligar Rega
  // if (higrometro):
  
  //doc.clear();
  ///Serial.println("Memoria do ESP");
  ///Serial.println(ESP.getFreeHeap());
  if (regaUmidade){  
    if(umidadeSolo < umidadeIdeal ){
        if(umidadeSolo < umidadeMinima){
          time_or_humidty = 2;
          valvula = true;
        }
    }else if (umidadeSolo >= umidadeIdeal ){
        valvula = false;
        inicioTempo = tempoLigado;
    }
  }
  if(regaHorario){   
    if(!valvula){
      for(JsonVariant  horaAtual : horasLista){
        if(horaAtual.as<String>() == hora_certa){
            //Serial.println("Ligou");
            time_or_humidty = 1;
            valvula = true;
            break;
          }
        }
      }
  }

         
  irrigar();
  temperatura = 0;
  umidadeDoArdht = 0;
  int result = dht11.readTemperatureHumidity(temperatura, umidadeDoArdht);
  //umidadeDoArdht =  dht11.readHumidity();
  //temperatura = dht11.readTemperature();
  //if (!result){
     //temperatura = 0;
     //umidadeDoArdht=0; 
     
    // }else{
      //umidadeDoArdht =  dht11.readHumidity();
      //temperatura = dht11.readTemperature();
      
     ///}
  //char payload_Set[200];
  //sprintf(payload_Set, "{\"hora_certa\":\"%s\", \"higrometro\" : [%d,%d], \"dht11\" : [%d,%d],\"flow_rate\": %.2f, \"total_pulso\": %d}",
    ///    hora_certa.c_str(), umidadeSolo, original, temperatura, umidadeDoArdht, flowRate, totalPulso);
  String payload_Set= "{\"hora_certa\":\""+hora_certa+"\", \"higrometro\" : ["+umidadeSolo+","+original+"], \"dht11\" : ["+temperatura +","+umidadeDoArdht+"],\"flow_rate\": "+flowRate+", \"total_pulso\": "+ totalPulso +"}"  ;

  int code = db.from("Hortas").eq("id", id_horta).doUpdate(payload_Set);
  //Serial.println(code);
  db.urlQuery_reset();  

  //String payload_Set = "";
  //payload_Set = "{\"hora_certa\":\""+hora_certa+"\", \"higrometro\" : ["+umidadeSolo+","+original+"], \"dht11\" : ["+temperatura +","+umidadeDoArdht+"],\"flow_rate\": "+flowRate+"} ";
  //payload_Set=       "{\"hora_certa\":\""+hora_certa+"\", \"valvula\": "+valvula+ ", \"higrometro\" : ["+umidadeSolo+","+original+"], \"dht11\" : ["+temperatura +","+umidadeDoArdht+"],\"flow_rate\": "+flowRate+" } "  ;
  //  payload_Set=     "{\"hora_certa\":\""+hora_certa+"\", \"valvula\": "+valvula+ ", \"higrometro\" : ["+umidadeSolo+","+original+"], \"dht11\" : ["+temperatura +","+umidadeDoArdht+"],\"flow_rate\": "+flowRate+" } "  ;
 
  //int code = db.from("Hortas").eq("id", id_horta).doUpdate(payload_Set);
   //db.urlQuery_reset();  
 // Serial.print("CODE ");
  //Serial.println(code);
 // Serial.println(payload_Set);
 


}
void irrigar(){

  unsigned long tempoAtual = millis();
  ///if(isRegando){
    ///.println("TEMPO IRRIGANDO");
    //Serial.println((tempoAtual - inicioTempo));
    //Serial.println("VALVULA");
    //Serial.println(valvula);
  ///}

  if(!valvula ){
      
      inicioTempo = tempoAtual;
      desligarRele();
    }
  else if (valvula && (tempoAtual - inicioTempo >= ( tempoLigado * 1000)))
  {
    ///inicioTempo = tempoAtual;
      //Serial.println("TEMPO IRRIGANDO");
      //Serial.println((tempoAtual - inicioTempo));
      ///valvula = false;
      desligarRele();
      
  }else if(valvula && !isRegando){
      ligarRele();
      
  }



}

void ligarRele(){
    digitalWrite(pinValvula, LOW); // Liga o relé
    
    if(!isRegando){
      totalPulso=0;
      //Serial.println("LIGOU PRIMEIRA");
      valvula = true;
      start_irriga = unix_time;
      String payload_Setvalvula= "{\"valvula\": true}";



      int code = db.from("Hortas").eq("id", id_horta).doUpdate(payload_Setvalvula);
      db.urlQuery_reset();  
      ////////////////////////////////DEL

      
      
      
    }
    isRegando = true;
    //Serial.println("Relé ligado!");
  
}
void desligarRele(){

  digitalWrite(pinValvula, HIGH);
  valvula = false;
  if(isRegando){
      valvula = false;
      String payload_Setvalvula= "{\"valvula\": false, \"total_pulso\": "+ String(totalPulso) + " }";
      int code = db.from("Hortas").eq("id", id_horta).doUpdate(payload_Setvalvula);
      db.urlQuery_reset();  

      end_irriga = unix_time;
      ///Serial.println("DESLIGOU PRIMEIRA");
      
      String payload_historico ="{\"rega_start\" : "+ String(start_irriga) +" ,\"rega_end\" : "+String(end_irriga)+",\"water_usage\":"+water_use +", \"id_horta\": \""+id_horta+"\" , \"time_or_humidity\" : "+time_or_humidty+", \"duration_sett\": "+tempoLigado+",  \"dht11\" : ["+temperatura +","+umidadeDoArdht+"], \"total_pulso\": "+totalPulso+" , \"higrometro\" : "+umidadeSolo+"}";
      int codeHist = db.insert("Historico", payload_historico, false);
      ///Serial.print("INSERT ");
     /// Serial.println(codeHist);
     /// Serial.print("PAYLOAD:  ");
     /// Serial.println(payload_historico);
      db.urlQuery_reset(); 
      time_or_humidty = 0;
      //totalPulso=0;
      

      

      

      
  }
  end_irriga=0;
  
  start_irriga=0;
  isRegando = false;
  time_or_humidty = 0;
    //inicioTempo=0;
  //Serial.print("  TOTAL PULSO:  ");
  //Serial.println(totalPulso);
}
void equipsSetup(){
  pinMode(pinValvula, OUTPUT);
  pinMode(pinHigrometro, INPUT);
  digitalWrite(pinValvula, HIGH);
  pinMode(pinVazao, INPUT_PULLUP);



  attachInterrupt(digitalPinToInterrupt(pinVazao), pulseCounter, FALLING);


}




