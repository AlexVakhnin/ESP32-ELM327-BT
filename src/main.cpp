#include <Arduino.h>
#include "BluetoothSerial.h"
#include <U8g2lib.h>

//I2C for SH1106 OLED 128x64
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

void ReadELM(); //декларация функций..
void serial2_clear();
String _elm_hex_calibr();
void state_bt(); //состояние на экран..
void state_res();
void state_elm();
void state_can();
void state_e0();
void state_e1();
void disp_val(int val);

BluetoothSerial SerialBT;
#define BT_DISCOVER_TIME  10000
//static bool btScanSync = true;
const char *pin = "1234";
String myName = "ESP32-BT-Master";
String slaveName = "Android-Vlink";

String RxBuffer = ""; //Буфер заполняется из ELM
byte mcalibr[24] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}; //Буфер риема ELM
char hexChar[150]; //массив для sprintf() функции (150 - на строку)
int count_c = 0; //количество принятых символов на строку
boolean flag_ok =false; 

void setup() {
  bool connected;

  Serial.begin(115200);

  //OLED SH1106 128x64
  u8g2.begin();

  state_bt(); //состояние подключения к блютуз

  SerialBT.begin(myName, true); //Инициализируем как мастер
  SerialBT.setPin(pin); //Устанавливаем PIN

  Serial.println("---------------------------------");
  Serial.printf("Total heap:\t%d \r\n", ESP.getHeapSize());
  Serial.printf("Free heap:\t%d \r\n", ESP.getFreeHeap());
  Serial.println("I2C_SDA= "+String(SDA));
  Serial.println("I2C_SCL= "+String(SCL));
  Serial.println("---------------------------------");

//----------------------------СКАНИРУЕМ СЕТЬ-------------------------------
/*
  if (btScanSync) {
    Serial.println("Starting discover...");
    BTScanResults *pResults = SerialBT.discover(BT_DISCOVER_TIME);
    if (pResults)
      pResults->dump(&Serial);
    else
      Serial.println("Error on BT Scan, no result!");
  }
*/
//-------------------------------------------------------------------------

  connected = SerialBT.connect(slaveName);
  Serial.printf("Connecting to slave BT device named \"%s\"\n", slaveName.c_str());
  if(connected) {
    Serial.println("Connected Successfully!");
  } else {
    while(!SerialBT.connected(10000)) {
      Serial.println("Failed to connect. Make sure remote device is available and in range, then restart app.");
      //disp_val(327); //TEST
      delay(3000);
      SerialBT.disconnect();
      SerialBT.connect();

      //ESP.restart();
    }
  }

  state_elm();//состояние инициализации ELM327

  serial2_clear(); SerialBT.println("ATZ"); delay(5000);
  ReadELM(); //16 -> 0D 0D 45 4C 4D 33 32 37 20 76 32 2E 33 0D 0D 3E
  serial2_clear(); SerialBT.println("ATE0"); delay(500);
  ReadELM(); //10 -> 41 54 45 30 0D 4F 4B 0D 0D 3E
  serial2_clear(); SerialBT.println("ATL0"); delay(500);
  ReadELM(); //5 -> 4F 4B 0D 0D 3E

  serial2_clear(); SerialBT.println("01 05"); delay(500);
  ReadELM(); //12 -> 43 41 4E 20 45 52 52 4F 52 0D 0D 3E

  //Проверим ответ от ELM327 на "01 05"
  if( count_c == 12 && mcalibr[0] == 52 ) { //проверка на успешный ответ 34 31 20 30 35 20 33 38 20 0D 0D 3E (12)
    //disp_val(elmdecode(mcalibr[6],mcalibr[7])-40); //показать на дислее число 
    disp_val(mcalibr[7]);  //TEST
    flag_ok = true;
    delay(2000);
  } else if (count_c == 12 && mcalibr[0] == 67 ){
    state_can();//Получили ошибку по шине CAN
    if (SerialBT.disconnect()) {
      Serial.println("Disconnected Successfully!");
    }
      //flag_ok = true; //TEST
  } else {
    state_e1();//Неопределенная ошибка
    if (SerialBT.disconnect()) {
      Serial.println("Disconnected Successfully!");
    }
  }

}

void loop() {

if (flag_ok){
  serial2_clear(); SerialBT.println("01 05"); delay(500);
  ReadELM(); //6 -> 41 05 58 0D 0D 3E
  //Проверим ответ от ELM327 на "01 05"
  if( count_c == 12 && mcalibr[0] == 52 ) { //проверка на успешный ответ 34 31 20 30 35 20 33 38 20 0D 0D 3E (11)
    //disp_val(elmdecode(mcalibr[6],mcalibr[7])-40); //показать на дислее число
    disp_val(mcalibr[7]);
  } else {
    state_e0();//ошибка цикла опроса
    flag_ok = false; //выключаем цикл..
    if (SerialBT.disconnect()) {
      Serial.println("Disconnected Successfully!");
    }
  }

}
  delay(2000);
}

//Функция чтения из ELM327
void ReadELM() {
  count_c=0;
  for (int i = 0; i < 24; i++){ //читаем 24 байт в массив без проверки
    mcalibr[i] = SerialBT.read();
    if (mcalibr[i] != 255) count_c++;
    }        
  Serial.println(_elm_hex_calibr()+" ("+String(count_c)+")");
  delay(200);
}


/*
int ReadELM() {
  RxBuffer = "";
  char RxByte;
  int count_c = 0;

  while (SerialBT.available()){
    RxByte = SerialBT.read();  //Читаем байт
    RxBuffer += String(RxByte); //в буфер
    count_c++;
    Serial.println("Byte counter: "+String(count_c));
    if (RxByte == '\n' || count_c >= 32) break;
  }

  return count_c;
}
*/
void serial2_clear() {

      while (SerialBT.available())  //очищаем буфер приема
      SerialBT.read();  
}

String _elm_hex_calibr(){  //hex mcalibr[] string
      sprintf(hexChar,"%02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X ",
        mcalibr[0],mcalibr[1],mcalibr[2],mcalibr[3],mcalibr[4],mcalibr[5],mcalibr[6],mcalibr[7],
        mcalibr[8],mcalibr[9],mcalibr[10],mcalibr[11],mcalibr[12],mcalibr[13],mcalibr[14],mcalibr[15],
        mcalibr[16],mcalibr[17],mcalibr[18],mcalibr[19],mcalibr[20],mcalibr[21],mcalibr[22],mcalibr[23]);
      return hexChar;
}

void state_bt(){
  u8g2.clearBuffer();					// clear display buffer
  u8g2.setFont(u8g2_font_inb49_mf);//big font
  u8g2.drawStr(23,64-5,"BT");
  u8g2.sendBuffer();
}
void state_e0(){
  u8g2.clearBuffer();					// clear display buffer
  u8g2.setFont(u8g2_font_inb49_mf);//big font
  u8g2.drawStr(23,64-5,"E0");
  u8g2.sendBuffer();
}
void state_e1(){
  u8g2.clearBuffer();					// clear display buffer
  u8g2.setFont(u8g2_font_inb49_mf);//big font
  u8g2.drawStr(23,64-5,"E1");
  u8g2.sendBuffer();
}
void state_res(){
  u8g2.clearBuffer();					// clear display buffer
  u8g2.setFont(u8g2_font_inb49_mf);//big font
  u8g2.drawStr(2,64-5,"RES");
  u8g2.sendBuffer();
}
void state_elm(){
  u8g2.clearBuffer();					// clear display buffer
  u8g2.setFont(u8g2_font_inb49_mf);//big font
  u8g2.drawStr(2,64-5,"ELM");
  u8g2.sendBuffer();
}
void state_can(){
  u8g2.clearBuffer();					// clear display buffer
  u8g2.setFont(u8g2_font_inb49_mf);//big font
  u8g2.drawStr(2,64-5,"CAN");
  u8g2.sendBuffer();
}
void disp_val(int val){
  if(val >= 0){   //положительные значения
    String str_value = String(val);
    u8g2.clearBuffer();					// clear display buffer
    u8g2.setFont(u8g2_font_logisoso62_tn);//big font for clock
    u8g2.drawStr(2,63,str_value.c_str());
    u8g2.sendBuffer();
  }
  //else {}
}
