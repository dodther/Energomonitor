//#include <SoftwareSerial.h> // Arduino IDE <1.6.6
//#define BLYNK_PRINT Serial    // Comment this out to disable prints and save space
#include <PZEM004T.h>
#include <Ethernet.h>
#include <BlynkSimpleEthernet.h>
#include <TimeLib.h>
#include <EthernetUdp.h>
#include <EEPROM.h>
#define W5100_CS  10
int intConnect;
PZEM004T pzem(4,5);  // (RX,TX) connect to TX,RX of PZEM
IPAddress ip[3];
uint8_t ports[3][5] ={{0,1,2,13,3},{4,5,6,14,7},{8,9,10,15,11}}; // номера портов в массив. по время перебора будут в блинк улетать

//BlynkTimer timer;

byte FullReset = 0; 
byte DN; // параметр День1, ночь0
byte DNAdresse = 0; //Адресс для хранения параметра День1, ночь0
byte dAdresse = 1; // начала адресса куда буду писать показаня за день там тип данных int32_t 4 байта занимает
byte nAdresse = 5; // начала адресса куда буду писать показаня за ночь там тип данных int32_t 4 байта занимает
byte TarrifAdresseD = 9; // начала адресса куда буду писать время перехода на день
byte TarrifAdresseN = 11; // начала адресса куда буду писать время перехода на ночь
uint32_t kWhDayAll;
uint32_t kWhDayAllERROM;
uint32_t kWhNightAll;
uint32_t kWhNightAllERROM;
char auth[] = "you_token";
// мак должен быть разный для всех устройств в локальной сети
byte arduino_mac[] = { 0xDE, 0xED, 0xBA, 0xFE, 0xFE, 0xDD };
char server[]          = "blynk-cloud.com";
unsigned int port      = 8442;
float tarifD; // цена на дневное ЭЭ введённая на телефоне
float tarifN; // цена на ночное ЭЭ введённая на телефоне
int TimeD;  // время перехода на день
int TimeN;  // время перехода на ночь
float vipe[3][4];

unsigned long ChkConn =0; // переменная для проверки сколько времени прошло с момента последней попытки подключения к серверу блинка
unsigned long TimeChkConn = 60000; // време для повторной проверки соединения с сервером блинк.
unsigned long LstRd1 =0;  
byte Ncycle1 = 1;
byte ResetWh = 0; // флаг для запуска функции сброса показания ватт. при 0 сброса нет, 1 - сброс
unsigned long LstRdWh =0; // счётчик времени для функции сброса.
byte NcycleWh = 1; // флаг для цикла сброса
unsigned int RdDlyWh = 0; // задержка межди командами сброса.
#define RdDly  1000  // задержка мс между обращениями к PZEM004T
bool isFirstConnect = true;


IPAddress timeServer(88, 147, 254, 234);
const int timeZone = 10; 
EthernetUDP Udp;
unsigned int localPort = 8888;  // local port to listen for UDP packets

void setup() {
  pinMode(18, OUTPUT); // управление сбросом дачиков
  pinMode(19, OUTPUT); // управление питанием дачиков

  Serial.begin(9600);
  // В файле библиотеки Ethernet.h  82 строка. заменил константу timeout с 60000 на 10000. Чтобы быстрее думало при 
  // попытке получить адресс без воткнутого кабеля.
  intConnect = Ethernet.begin(arduino_mac); 
  //Serial.println(intConnect);
  Serial.println(Ethernet.localIP());
  Blynk.config(auth, server, port);
  Blynk.connect();
  //Blynk.begin(auth);
  ip[0] = IPAddress(192, 168, 1, 1);
  ip[1] = IPAddress(192, 168, 1, 2);
  ip[2] = IPAddress(192, 168, 1, 3);
  DN = EEPROM.read(DNAdresse);
  firstRun(DN);
  kWhDayAllERROM = EEPROM.get(dAdresse, kWhDayAllERROM); //считаем  из памяти количество Вт за дни
  kWhNightAllERROM = EEPROM.get(nAdresse,kWhNightAllERROM);
  kWhNightUpdate(); // разовая сработка при запуске. Чтобы считаные из памяти показания засчитались. А то если день, то ночные не считались и наоборот
  kWhDayUpdate();
  //Serial.print(kWhDayAllERROM);Serial.print("  ");Serial.println(kWhNightAllERROM);
  Udp.begin(localPort); // это для часов реального времени
 // Serial.println("waiting for sync");
  setSyncProvider(getNtpTime);  // это для часов реального времени

  setSyncInterval(10 *60); // Sync interval in seconds (10 minutes) // это для часов реального времени
  //timer.setInterval(60000L, CheckConnection); 
  
}


void loop() {
 
    if(Blynk.connected()){
    Blynk.run();
    }
//   timer.run();


    
   if(ResetWh == 0){
      readEnergy();
    }else
    {
      ResetWatH();
   }

   CheckConnection();

}

void CheckConnection(){    // check every 11s if connected to Blynk server
  if(!Blynk.connected() && (millis()-ChkConn) > TimeChkConn ){
  
    Serial.println("Not connected to Blynk server"); 
    if(intConnect == 0) intConnect = Ethernet.begin(arduino_mac);
    bool isFirstConnect = true;
    Blynk.connect();  // try to connect to server with default timeout
    ChkConn = millis();
  }
  
}


BLYNK_CONNECTED() // runs every time Blynk connection is established
{
//   rtc.begin();
if (isFirstConnect)
{
// Request server to re-send latest values for all pins
Blynk.syncAll();
isFirstConnect = false;
}
} 


BLYNK_WRITE(V12)
{
int i=param.asInt();
if (i==1 & ResetWh == 0)   // что-бы небыло двойного срабатывания
{
  ResetWh = 1; 
  LstRdWh = millis();
  FullReset = 1;

}
}

  BLYNK_WRITE(V18)
{
tarifD=param.asFloat();

}
  BLYNK_WRITE(V19)
{
tarifN=param.asFloat();

}

  BLYNK_WRITE(V25)
{
TimeN=param.asInt();
//Blynk.virtualWrite(27,TimeN);

}

  BLYNK_WRITE(V26)
{
TimeD=param.asInt();
//Blynk.virtualWrite(28,TimeD);

}



float ReactivePower(float arr[3]) // считаем сколько Вт реактивно
{
  float RP = ((arr[0]*arr[1])-arr[2])/1000;
  if (RP < 0.0) RP = 0.0;
  return RP;
  
}

void MomentCost(){
  float tarifM;
  if(DN == 1)  {tarifM = tarifD;} else {tarifM = tarifN;}
  float cost = ((vipe[0][2] + vipe[1][2] + vipe[2][2])/1000)*tarifM;
  Blynk.virtualWrite(16,cost); // убрать "/3". так как у меня все 3 одну замеряют на время отладки сделал так
}


void kWhDayUpdate(){
  //Serial.print(kWhDayAllERROM); Serial.print("  "); Serial.println(kWhNightAllERROM);
  uint32_t kWhDay = (vipe[0][3] + vipe[1][3] + vipe[2][3]);
  kWhDayAll=kWhDayAllERROM+kWhDay;
  //Serial.println(sizeof(vipe));
  Blynk.virtualWrite(22,float(kWhDayAll)/1000);
}

void kWhNightUpdate(){
  //Serial.print(kWhDayAllERROM); Serial.print("  "); Serial.println(kWhNightAllERROM);
  uint32_t kWhNight = (vipe[0][3] + vipe[1][3] + vipe[2][3]);
  kWhNightAll=kWhNightAllERROM+kWhNight;
  //Serial.println(TimeD);
  Blynk.virtualWrite(23,float(kWhNightAll)/1000);
}

void kWhAll(){
 // Serial.print(kWhDayAll); Serial.print(":");Serial.print(kWhDayAllERROM);Serial.print("  "); Serial.print(kWhNightAll);Serial.print(":");Serial.println(kWhNightAllERROM);
  Blynk.virtualWrite(24,float(kWhNightAll+kWhDayAll)/1000);
  
}

void Money(){
 //Serial.println(float(kWhDayAll*tarifD/1000));
  float mnight = kWhNightAll*tarifN/1000;
  float mday = kWhDayAll*tarifD/1000;
  Blynk.virtualWrite(20,mnight);
  Blynk.virtualWrite(17, mday);
  Blynk.virtualWrite(21,mnight+mday);
}

void NightToDay(){
  if((TimeN>hour() && hour()>=TimeD) && DN == 0){
  DN = 1;
  memset(vipe, 0,sizeof(vipe)); // заполняем нулями при переходе. Так как функция подсчёта находится ниже и при смене DN посчитает день за ночь и наоборот
  Serial.println(kWhNightAll);
  kWhNightAllERROM = kWhNightAll;
  EEPROM.put(nAdresse,kWhNightAll);
  EEPROM.update(DNAdresse,DN);
  //Serial.print("DN -> "); Serial.println(DN);
  Serial.print(hour());Serial.print(":");Serial.println(minute());
  Serial.println("Go to day time");
  ResetWh = 1; 
  LstRdWh = millis();
  //Serial.println("---------------------------");
      }
}

void DayToNight(){
    
    
      if((TimeN<=hour() || hour()<TimeD) && DN == 1){ 
      Serial.println(kWhDayAll);
      //Blynk.virtualWrite(22,float(kWhDayAll)/1000);
      DN = 0;
      memset(vipe, 0,sizeof(vipe)); 
      EEPROM.put(dAdresse,kWhDayAll);
      kWhDayAllERROM = kWhDayAll;
      EEPROM.update(DNAdresse,DN);
      //Serial.print("DN -> "); Serial.println(DN);                                   
      Serial.print(hour());Serial.print(":");Serial.println(minute());
      Serial.println("Go to night time");
      ResetWh = 1; 
      LstRdWh = millis();
      //Serial.println("---------------------------");
  } 
  
}


void readEnergy(){

if (millis() - LstRd1 > RdDly )
  {
    switch (Ncycle1) {
      case 1:
        for(uint8_t i = 0; i <3; i++)
        {
          vipe[i][0] = pzem.voltage(ip[i]); if (vipe[i][0] < 0.0) vipe[i][0] = 0.0; 
          Blynk.virtualWrite(ports[i][0],int(round(vipe[i][0])));
         } 
      break;
      case 2:
        for(uint8_t i = 0; i <3; i++)
        {
         vipe[i][1] = pzem.current(ip[i]); if (vipe[i][1] < 0.0) vipe[i][1] = 0.0;
         Blynk.virtualWrite(ports[i][1],vipe[i][1]);
         
        } 
      break;
      case 3:
        for(uint8_t i = 0; i <3; i++)
        {
          vipe[i][2] = pzem.power(ip[i]); if (vipe[i][2] < 0.0) vipe[i][2] = 0.0; 
          Blynk.virtualWrite(ports[i][2],vipe[i][2]/1000); // ватты в киловатты
         Blynk.virtualWrite(ports[i][3],ReactivePower(vipe[i]));
        }
       break;
       case 4:
        for(uint8_t i = 0; i <3; i++)
        {
          vipe[i][3] = pzem.energy(ip[i]); if (vipe[i][3] < 0.0) vipe[i][3] = 0.0; 
           Blynk.virtualWrite(ports[i][4],vipe[i][3]/1000);
        }
       break;
       default:
       
        for(uint8_t i = 0; i <3; i++)
        {
          // Serial.print(i); Serial.print(": "); Serial.print(vipe[i][0]);Serial.print("V "); // Serial.print(" ");   Serial.print(" ("); Serial.print(millis()- LstRd1); Serial.print(") ");
          // Serial.print(vipe[i][1]);Serial.println("A ");  // Serial.print(" ("); Serial.print(millis()- LstRd1); Serial.print(") "); 
         //  Serial.print(vipe[i][2]);Serial.print("W ");  //  Serial.print(" ("); Serial.print(millis()- LstRd1); Serial.print(") "); 
        //  Serial.print(vipe[i][3]);Serial.println("Wh ");  // Serial.print(" ("); Serial.print(millis()- LstRd1); Serial.println(") ");
          
        }
      Ncycle1 = 0;
      LstRd1 = 0;
      MomentCost(); // ткнём пока сюда
      DayToNight();
      NightToDay();
      Money();
      if(DN == 1)kWhDayUpdate();
      if(DN == 0)kWhNightUpdate();
      kWhAll();
      //digitalClockDisplay();
     //Serial.print(hour());Serial.print(":");Serial.println(minute());
     //Serial.println("--------------------------------------------------");
    }
   
   // ..
   // Serial.print(" ("); Serial.print(millis()- LstRd1); Serial.println(") "); 
     Ncycle1++; LstRd1 = millis();
  }

  
}
void ResetWatH(){

   if (millis() - LstRdWh  > RdDlyWh )
  {
    switch(NcycleWh){
      case 1:
         Serial.println("Reset start");
         digitalWrite(19, HIGH); // отрубаем питание датчика
         RdDlyWh = 200; // delay(200);
      break;
      case 2:
         digitalWrite(18, HIGH); // замыкаем сброс через оптореле
         RdDlyWh = 6000;// delay(6000);         // на 6 сек
      break;
      case 3:
         digitalWrite(18, LOW); 
         RdDlyWh = 500;//delay(500);
      break;
     case 4:
         digitalWrite(18, HIGH);
         RdDlyWh = 100;//delay(100);
     break;
     case 5:
         digitalWrite(18, LOW);
         digitalWrite(19, LOW);
     break;
     default:
         RdDlyWh = 0;
         ResetWh = 0;
         NcycleWh = 0;
         if(FullReset == 1){
              Serial.println("Full Reset");
              kWhDayAll = 0; kWhNightAll = 0; kWhDayAllERROM = 0; kWhNightAllERROM = 0;
              Blynk.virtualWrite(V22,float(0)); Blynk.virtualWrite(V23,float(0)); Blynk.virtualWrite(V24,float(0));
              EEPROM.put(dAdresse,kWhDayAll);
              EEPROM.put(nAdresse,kWhNightAll);
              FullReset = 0;
         }
         Serial.println("Reset stop");
                   }
      NcycleWh++; LstRdWh = millis(); 
  
  }
  
}

void firstRun(byte DD){
  
  if(DD > 1){
    EEPROM.update( DNAdresse, 1);
    EEPROM.put(dAdresse, float(0));
    EEPROM.put(nAdresse , float(0));
    EEPROM.put(TarrifAdresseD , int(0));
    EEPROM.put(TarrifAdresseN  , int(0));
    DN = 1;
    
  }
  
}

/*
void digitalClockDisplay(){
  // digital clock display of the time
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(" ");
  Serial.print(month());
  Serial.print(" ");
  Serial.print(year()); 
  Serial.println(); 
}

void printDigits(int digits){
  // utility for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if(digits < 10)
    Serial.print('0');
  Serial.print(digits);
}
*/
/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  //Serial.println("Transmit NTP Request");
  sendNTPpacket(timeServer);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      //Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:                 
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}
