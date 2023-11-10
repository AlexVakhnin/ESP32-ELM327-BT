#include <Arduino.h>
#include "BluetoothSerial.h"
#include <U8g2lib.h>

//Timings
#define t_atz_init 5000 //время для команды ATZ
#define t_send_receive 250 //задержка между посылаемой командой и приемом ответа от ELM
#define t_main_loop 1800  //интервал между командами в общем цикле
#define t_show_error 5000  //время для показа окна с ошибкой
#define temp_cooler_on 98  //температура включения вентилятора (98)


//I2C for SH1106 OLED 128x64
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

 //декларация функций..
void ReadELM();
void serial2_clear();
String _elm_hex_calibr();
void disp_str(String vstr);
void state_logo();
boolean disp_val(int val);
int elmdecode(int hchar,int lchar);
int asciicode(int vchar);
void disp_dump(String vstr);
void sound (int ntik);
void disp2_err(String str1, String str2);

BluetoothSerial SerialBT;
const char *pin = "1234";
String myName = "ESP32-BT-Master";
String slaveName = "Android-Vlink";

String RxBuffer = ""; //Буфер заполняется из ELM
byte mcalibr[24] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}; //Буфер риема ELM
char hexChar[150]; //массив для sprintf() функции (150 - на строку)
int count_c = 0; //количество принятых байт от ELM
boolean flag_ok =false; 

void setup() {
  bool connected;

  Serial.begin(115200);

  pinMode(16, OUTPUT);
  digitalWrite(16, LOW);
  sound(1);

  //OLED SH1106 128x64
  u8g2.begin();
  state_logo();
  delay(2000);
//-----------------------------------------------------------------TESTS

  //disp_str("AB CD EF 12 34 3FS123456789ABCDEF12341 42 43 44 45 46.47 48 49 50 51.xx.Hallo World!"); //показать на дисплее число 00h (0)
  //delay(4000);

  //disp_dump("[01 05] E1 Error..");
  /*
     if (!disp_val(elmdecode(0x46,0x46)-40)) { //0xFF - 40
      disp_str("E3"); //ошибка при вычислении параметра от ELM327
      delay(5000);
      disp_dump("[01 05] E3 Error");
     }
  */
  //disp2_err("NO DATA","[01 05] E0 Error");
  //while(true){delay(10000);} //STOP..
  
//------------------------------------------------------------------
  disp_str("BT");//state_bt(); //состояние подключения к блютуз

  SerialBT.begin(myName, true); //Инициализируем как мастер
  SerialBT.setPin(pin); //Устанавливаем PIN

  Serial.println("---------------------------------");
  Serial.printf("Total heap:\t%d \r\n", ESP.getHeapSize());
  Serial.printf("Free heap:\t%d \r\n", ESP.getFreeHeap());
  Serial.println("I2C_SDA= "+String(SDA));
  Serial.println("I2C_SCL= "+String(SCL));
  Serial.println("---------------------------------");


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

  disp_str("ELM");//состояние инициализации ELM327

  serial2_clear(); SerialBT.println("ATZ"); delay(t_atz_init);
  ReadELM(); //16 -> 0D 0D 45 4C 4D 33 32 37 20 76 32 2E 33 0D 0D 3E
  serial2_clear(); SerialBT.println("ATE0"); delay(t_send_receive);
  ReadELM(); //10 -> 41 54 45 30 0D 4F 4B 0D 0D 3E
  serial2_clear(); SerialBT.println("ATL0"); delay(t_send_receive);
  ReadELM(); //5 -> 4F 4B 0D 0D 3E

  serial2_clear(); SerialBT.println("01 05"); delay(t_send_receive);
  ReadELM(); //12 -> 43 41 4E 20 45 52 52 4F 52 0D 0D 3E

  //проверка на успешный ответ 34 31 20 30 35 20 33 38 20 0D 0D 3E (12) = ['4','1', ,'0','5', ,'3','8', ,0D,0D,3E]
  if( count_c == 12 && mcalibr[0] == 0x34 && mcalibr[4] == 0x35) {
    if(disp_val(elmdecode(mcalibr[6],mcalibr[7])-40)){
      flag_ok = true;
      delay(t_main_loop);
    } else {
      disp_str("E3"); //ошибка при вычислении параметра от ELM327
      delay(t_show_error);
      disp_dump("[01 05] E3 Error");
      flag_ok = false; //выключаем цикл..  
      if (SerialBT.disconnect()) {
      Serial.println("Disconnected Successfully!");
      }
    }
  } else if (count_c == 12 && mcalibr[0] == 67 ){
    disp_str("CAN");//Получили ошибку по шине CAN
    delay(t_show_error);
    disp_dump("[01 05] CAN Error");
    if (SerialBT.disconnect()) {
      Serial.println("Disconnected Successfully!");
    }
  } else {
    disp_str("E1");//Неопределенная ошибка
    delay(t_show_error);
    disp_dump("[01 05] E1 Error");
    if (SerialBT.disconnect()) {
      Serial.println("Disconnected Successfully!");
    }
  }
}

void loop() {

if (flag_ok){
  serial2_clear(); SerialBT.println("01 05"); delay(t_send_receive);
  ReadELM(); //6 -> 41 05 58 0D 0D 3E

  //проверка на успешный ответ 34 31 20 30 35 20 33 38 20 0D 0D 3E (12) = ['4','1', ,'0','5', ,'3','8', ,0D,0D,3E]
  if( count_c == 12 && mcalibr[0] == 0x34 && mcalibr[4] == 0x35) { 
    //ответ успешный
    int vresp = elmdecode(mcalibr[6],mcalibr[7])-40; //вычисляем результат по формуле 
    if (!disp_val(vresp)) {   //если не в диапазоне то..
      disp_str("E3");         //ошибка при вычислении параметра от ELM327
      delay(t_show_error);
      disp_dump("[01 05] E3 Error");
      flag_ok = false; //выключаем цикл..  
      if (SerialBT.disconnect()) {
      Serial.println("Disconnected Successfully!");
      }
    } else {                  //результат датчика выдан на дислей
      if(vresp == temp_cooler_on ) sound(2); // сигнал два коротких в цикле опроса
      if(vresp > temp_cooler_on) digitalWrite(16, HIGH); //непрерывный сигнал включить
      if(vresp < temp_cooler_on) digitalWrite(16, LOW); //все сигналы выключить (контрольный)
    }
  } else if( count_c == 10 && mcalibr[0] == 0x4E && mcalibr[3] == 0x44) { //NO DATA ERROR
    disp2_err("NO DATA","[01 05] E0 Error");
    flag_ok = false; //выключаем цикл..
    if (SerialBT.disconnect()) {
      Serial.println("Disconnected Successfully!");
    }
  } else {
    disp_str("E0");//неопределенная ошибка цикла опроса
    delay(t_show_error);
    disp_dump("[01 05] E0 Error");
    flag_ok = false; //выключаем цикл..
    if (SerialBT.disconnect()) {
      Serial.println("Disconnected Successfully!");
    }
  }

}
  delay(t_main_loop);
}

//Функция чтения из ELM327
void ReadELM() {
  count_c=0;
  for (int i = 0; i < 24; i++){ //читаем 24 байт в массив без проверки
    mcalibr[i] = SerialBT.read();
    if (mcalibr[i] != 255) count_c++;
    }        
  Serial.println(_elm_hex_calibr()+" ("+String(count_c)+")");
  //delay(200);
}

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

void state_logo(){
  u8g2.clearBuffer();					// clear display buffer
  u8g2.setFont(u8g2_font_open_iconic_embedded_4x_t);//open iconic
  u8g2.drawStr(48,64-15,"N");
  u8g2.sendBuffer();
}

/*распечатка дампа ответа от ELM327*/
/*параметр - 5 строка*/
void disp_dump(String vstr){
  String strdump = _elm_hex_calibr(); //дамп в виде строки
  disp_str(strdump+vstr);
}

/* Вывод строковых значений на ЖК дисплей*/
void disp_str(String vstr){ 
  u8g2.clearBuffer();
  if(vstr.length() <= 2 ) {           //2 знака
    u8g2.setFont(u8g2_font_inb49_mf);
    u8g2.drawStr(23,64-5,vstr.c_str());
  } else if ( vstr.length() == 3 ){   //3 знака
    u8g2.setFont(u8g2_font_inb49_mf);
    u8g2.drawStr(2,64-5,vstr.c_str());
  } else {                            //больше 3х знаков -----------------------
    u8g2.setFont(u8g2_font_7x13B_tr);
    if (vstr.length() <= 18){u8g2.drawStr(0,9,vstr.c_str());} //1 строка
    else if (vstr.length() > 18 && vstr.length() <= 18*2){  //2 строки
      u8g2.drawStr(0,9,vstr.substring(0,18).c_str());
      u8g2.drawStr(0,9*2+4,vstr.substring(18).c_str());
    }
    else if (vstr.length() > 18*2 && vstr.length() <= 18*3){  //3 строки "AB CD EF 12 34 3FS.123456789ABCDEF123.4"
      u8g2.drawStr(0,9,vstr.substring(0,18).c_str());
      u8g2.drawStr(0,9*2+4,vstr.substring(18,18*2).c_str());
      u8g2.drawStr(0,9*3+4*2,vstr.substring(18*2).c_str());
    } 
    else if(vstr.length() > 18*3 && vstr.length() <= 18*4){  //4 строки
      u8g2.drawStr(0,9,vstr.substring(0,18).c_str());
      u8g2.drawStr(0,9*2+4,vstr.substring(18,18*2).c_str());
      u8g2.drawStr(0,9*3+4*2,vstr.substring(18*2,18*3).c_str());
      u8g2.drawStr(0,9*4+4*3,vstr.substring(18*3).c_str());
    }
    else if(vstr.length() > 18*4 && vstr.length() <= 18*5){  //5 строк
      u8g2.drawStr(0,9,vstr.substring(0,18).c_str());
      u8g2.drawStr(0,9*2+4,vstr.substring(18,18*2).c_str());
      u8g2.drawStr(0,9*3+4*2,vstr.substring(18*2,18*3).c_str());
      u8g2.drawStr(0,9*4+4*3,vstr.substring(18*3,18*4).c_str());
      u8g2.drawStr(0,9*5+4*4+3,vstr.substring(18*4).c_str());  //максимально снизу экрана(9*5+4*4+3=64)
    }
  }
  u8g2.sendBuffer();
}

void disp2_err(String str1, String str2){
  int ind_sp = str1.indexOf(' ');
  String disp_str1 = str1.substring(0,ind_sp);
  String disp_str2 = str1.substring(ind_sp+1);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_inb21_mf);
  u8g2.drawStr(0,40-5,disp_str1.c_str());
  u8g2.drawStr(46+2,40-5,disp_str2.c_str());

  u8g2.setFont(u8g2_font_7x13B_tr);
  u8g2.drawStr(0,9*5+4*4+3,str2.c_str());  //максимально снизу экрана(9*5+4*4+3=64)
  u8g2.sendBuffer();
}

boolean disp_val(int val){
  if (val > 255 || val < -99){  //не цифровое либо больше 3х знаков
    //disp_str("E3"); //ошибка при вычислении пераметра от ELM327
    return false;
  } else {
    String str_value = String(val);
    u8g2.clearBuffer();					// clear display buffer
    u8g2.setFont(u8g2_font_logisoso62_tn);//big font for clock
    u8g2.drawStr(2,63,str_value.c_str());
    u8g2.sendBuffer();
    return true;
  }
}

int elmdecode(int hchar,int lchar) { //результат может быть от 0 до 255 только!!!
  int resbyte = 999;
  int hbyte = asciicode(hchar);
  int lbyte = asciicode(lchar);
  
  //Проверка на ошибку..
  if (hbyte > 15 || lbyte > 15)
    return resbyte;
  else
    return hbyte*16+lbyte;
}
int asciicode(int vchar){ //результат может быть от 0 до 15 только!!!
  int resbyte =16;
  if (vchar >= 48 && vchar <= 57) { //цифры (0-9)
    resbyte = vchar-48;
  } else if (vchar >= 65 && vchar <= 70) { //буквы (10-15)
    resbyte = vchar-65+10;
  }
  return resbyte;
}

void sound (int ntik){
  if (ntik==1){
    digitalWrite(16, HIGH);
    delay(100);
    digitalWrite(16, LOW);
  } else if (ntik==2){
    digitalWrite(16, HIGH);
    delay(80);
    digitalWrite(16, LOW);
    delay(40);
    digitalWrite(16, HIGH);
    delay(80);
    digitalWrite(16, LOW);
  }
}