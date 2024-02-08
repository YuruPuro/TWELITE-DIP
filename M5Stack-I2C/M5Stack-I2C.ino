#include <M5Stack.h>

#define TEXT_HEIGHT 30
#define READ_DATA_LEN 25
#define CMD_DATA_LEN 14
#define CMD_STR_LEN 32

uint8_t readData[READ_DATA_LEN];
bool recvFlag = false ;
bool recvMSB = true ;
int  recvLen = 0 ;
bool sendCmdFlag = false ;
int dispCont = 0 ;
int retryCont = 0 ;
unsigned long passTime;

unsigned cmdData[CMD_DATA_LEN];
char cmdStr[CMD_STR_LEN] ;

static char bin2Hex[] = {"0123456789ABCDEF"} ;
static uint8_t i2cCmd1[] = {7,0x78,0x88,0x01,0x01,0x38,0x71,0x00} ;
static uint8_t i2cCmd2[] = {9,0x78,0x88,0x02,0x01,0x38,0xAC,0x02,0x33,0x00} ;
static uint8_t i2cCmd3[] = {7,0x78,0x88,0x03,0x02,0x38,0x00,0x07} ;

void hexCharaSet(uint8_t data,char *buffPos) {
  buffPos[0] = bin2Hex[data >> 4] ;
  buffPos[1] = bin2Hex[data & 0x0F] ;
  buffPos[2] = 0x00 ;
}

uint8_t hex2Bin(char c) {
  uint8_t hex ;
  if ('0' <= c && c <= '9') {
    hex = c - '0' ;
  } else
  if ('A' <= c && c <= 'F') {
    hex = c - 'A' + 10 ;
  } else
  if ('a' <= c && c <= 'f') {
    hex = c - 'a' + 10 ;
  }
  return hex ;
}

void sendCommandBuild(int cmdNo) {
  memset(cmdData,0,CMD_DATA_LEN) ;
  int cmdLen ;
  uint8_t *i2cData ;
  switch(cmdNo) {
    case 1: cmdLen = i2cCmd1[0] ; i2cData = &i2cCmd1[1] ; break ;
    case 2: cmdLen = i2cCmd2[0] ; i2cData = &i2cCmd2[1] ; break ;
    case 3: cmdLen = i2cCmd3[0] ; i2cData = &i2cCmd3[1] ; break ;
  }
  uint8_t checkSum = 0 ;
  cmdStr[0] = ':' ;
  for (int i=0;i<cmdLen;i++) {
    checkSum += i2cData[i] ;
    hexCharaSet(i2cData[i],&cmdStr[1+i*2]) ;
  }
  checkSum = ~checkSum + 1 ;
  hexCharaSet(checkSum,&cmdStr[1+cmdLen*2]) ;
  cmdStr[1+(cmdLen+1)*2] = 0x0d ;
  cmdStr[1+(cmdLen+1)*2+1] = 0x0a ;
  cmdStr[1+(cmdLen+1)*2+2] = 0x00 ;
}

void progressDisp(int no) {
  int xP = 0;
  int yP = TEXT_HEIGHT*3+1;
  char dispStr[32] ;

  M5.Lcd.setTextColor(TFT_MAGENTA, TFT_BLACK);
  sprintf(dispStr,"[%4d]%04d  ",dispCont,retryCont) ;
  xP = M5.Lcd.drawString(dispStr, xP , yP , 4);
  switch(no) {
    case 1: M5.Lcd.drawString("+-- ", xP , yP , 4); break ;
    case 2: M5.Lcd.drawString("-+- ", xP , yP , 4); break ;
    case 3: M5.Lcd.drawString("--+ ", xP , yP , 4); break ;
    case -1: M5.Lcd.drawString("F-- ", xP , yP , 4); break ;
    case -2: M5.Lcd.drawString("-F- ", xP , yP , 4); break ;
    case -3: M5.Lcd.drawString("--F ", xP , yP , 4); break ;
    default: M5.Lcd.drawString("--- ", xP , yP , 4); break ;
  }
  M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
}

void readDataDecode(uint8_t *sensorData) {
  int xP = 0;
  int yP = TEXT_HEIGHT*4+1;
  char dispStr[32] ;
  bool flagError = false ;

  M5.Lcd.fillRect(0, yP , 320, yP+TEXT_HEIGHT , TFT_BLACK);  // なくても平気なので、チラツキ防止のために外す
  M5.Lcd.setTextColor(TFT_MAGENTA, TFT_BLACK);

  // ----- BUSY CHECK --
  if ((sensorData[0] & 0x80) != 0) {
    M5.Lcd.drawString("DEVICE BUSY ", 0 , yP , 4);
    flagError = true ;
  } else {
    // ----- CRC8 -----CRC[7:0]= 1 + x^4 + x^5 + x^8  : B1 0011 0001
    uint8_t crc = 0xFF;
    for (int i = 0; i < 6 ; i++) {
      uint8_t b = sensorData[i];
      crc ^= b;
      for (int x = 0; x < 8; x++) {
        if (crc & 0x80) {
          crc <<= 1;
          crc ^= 0x31;
        } else {
          crc <<= 1;
        }
      }
    }
    if (sensorData[6] != crc) {
      M5.Lcd.drawString("CRC ERROR ", 0 , yP , 4);
      flagError = true ;
    }
  }

  M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
  if (flagError) { return ; }
  sendCmdFlag = false ;
  dispCont ++ ;

  // --- 温湿度算出
  uint32_t t1,t2,t3 ;
  t1 = sensorData[1] ; t2 = sensorData[2] ; t3 = sensorData[3] & 0xF0 ;
  uint32_t h = t1 << 12 | t2 << 4 | t3 >> 4 ; 

  t1 = sensorData[3] & 0x0F; t2 = sensorData[4] ; t3 = sensorData[5] ;
  uint32_t t = t1 << 16 | t2 << 8 | t3 ;

  int   humidity = (h * 100 ) >> 20 ;
  float temperature = (( t * 200) >> 18) / 4. - 50;

//  sprintf(dispStr,"H:%d T:%f  ",humidity,temperature) ;
  sprintf(dispStr,"%2.1f   %2d ",temperature,humidity) ;

  yP += TEXT_HEIGHT ;
  M5.Lcd.setTextFont(7);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setCursor(12,yP);
  M5.Lcd.print(dispStr);

  M5.Lcd.setTextFont(4);
  M5.Lcd.setTextSize(1);
  M5.Lcd.setCursor(130,yP+26);
  M5.Lcd.print("C");
  M5.Lcd.setCursor(220,yP+26);
  M5.Lcd.print("%");
  M5.Lcd.drawCircle(127, yP+26 , 4, WHITE);
  M5.Lcd.drawCircle(127, yP+26 , 3, WHITE);
}

void statDisp(int dataLen) {
  int xP = 0;
  int yP = TEXT_HEIGHT+1;
  char dispStr[12] ;
  memset(dispStr,0,12) ;
//  M5.Lcd.fillRect(0, yP , 320, TEXT_HEIGHT*5 , TFT_BLACK);  // なくても平気なので、チラツキ防止のために外す
  M5.Lcd.setTextColor(TFT_GREENYELLOW, TFT_BLACK);

  // -----
  // 送信デバイスID
  yP = TEXT_HEIGHT + 1 ;
  xP = M5.Lcd.drawString("Device:", 0 , yP , 4);
  hexCharaSet(readData[0],dispStr) ;
  xP += M5.Lcd.drawString(dispStr, xP, yP, 4);
  xP += M5.Lcd.drawString("-", xP , yP , 4);
  // 送信元個体識別番号
  for (int i=5;i<=8;i++) {
    hexCharaSet(readData[i],dispStr) ;
    xP += M5.Lcd.drawString(dispStr, xP, yP, 4);
  }
  // 電波強度
  yP += TEXT_HEIGHT ;
  xP = M5.Lcd.drawString("LQI:", 0 , yP , 4);
  sprintf(dispStr,"%03d",readData[4]) ;
  xP += M5.Lcd.drawString(dispStr, xP, yP, 4);
  // 電圧
  xP += M5.Lcd.drawString(" / Vcc:", xP , yP , 4);
  float vcc = (readData[13] << 8 | readData[14]) / 1000. ;
  sprintf(dispStr,"%1.2f ",vcc) ;
  xP += M5.Lcd.drawString(dispStr, xP, yP, 4);
  // -----
  M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
}

void setup() {
  M5.begin();
  M5.Power.begin();  // Init Power module.
  M5.Speaker.setVolume(3);
  M5.Lcd.clear(BLACK);
  M5.Lcd.setTextFont(4);
  M5.Lcd.setTextSize(1);

  Serial1.begin(115200, SERIAL_8N1, 16, 17);  // シリアル通信1初期化(RX, TX)

  M5.Lcd.fillScreen(TFT_BLACK);
  M5.Lcd.setTextColor(TFT_WHITE, TFT_BLUE);
  M5.Lcd.fillRect(0, 0, 320, TEXT_HEIGHT, TFT_BLUE);
  M5.Lcd.drawCentreString("TWELITE I2C", 320 / 2, 4, 4);
  M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);

  // -----
  passTime = millis();
  sendCmdFlag = true ;
}

void loop() {
  M5.update();  //本体のボタン状態更新
  // データ送信
  if (M5.BtnA.wasPressed() || // 手動操作
       millis() - passTime > 1000 * 60 || // 一分間隔で動作
      (sendCmdFlag && millis() - passTime > 1000 * 4) ) { // 4秒間隔でチェック
    sendCmdFlag = true ;
    retryCont ++ ;
    passTime = millis();
    progressDisp(0) ;
    sendCommandBuild(1);
    Serial1.print(cmdStr) ;
  }

  // データ受信
  if (Serial1.available() > 0) {
    char c = Serial1.read();
    if (c == ':') {
      memset(readData,0,READ_DATA_LEN) ;
      recvFlag = true ;
      recvLen = 0 ;
    } else
    if (c == 0x0d) {
      // STATUS RECIVE
      recvFlag = false ;
      if (readData[1] == 0x89 && readData[2] == 0x01) {
        if (readData[3] == 0) {
          progressDisp(-1) ;
        } else {
          progressDisp(1) ;
          delay(33) ; //　ココのディレイは仕様上は 100ms必要だけど、他の処理に時間が掛かるので調整している。 長いと無視されるし、短いとDEVICE　BUSYになる。
          sendCommandBuild(2);
          Serial1.print(cmdStr) ;
        }
      } else
      if (readData[1] == 0x89 && readData[2] == 0x02) {
        if (readData[3] == 0) {
          progressDisp(-2) ;
        } else {
          progressDisp(2) ;
          delay(27) ; //　ココのディレイは仕様上は 80ms必要だけど、他の処理に時間が掛かるので調整している。長いと無視されるし、短いとDEVICE　BUSYになる。
          sendCommandBuild(3);
          Serial1.print(cmdStr) ;
        }
      } else
      if (readData[1] == 0x89 && readData[2] == 0x03) {
        if (readData[3] == 0) {
          progressDisp(-3) ;
        } else {
          retryCont = 0 ;
          progressDisp(3) ;
          readDataDecode(&readData[6]) ;
        }
      } else
      if (!sendCmdFlag && readData[1] == 0x81 && readData[9] == 0x00) {
        statDisp(recvLen) ;
      }
    } else {
      if (c >= ' ') {
        if (recvMSB) {
          readData[recvLen] = hex2Bin(c) << 4 ;
          recvMSB = false ;
        } else {
          readData[recvLen] |= hex2Bin(c) ;
          recvMSB = true ;
          recvLen++ ;
        }
      }
    }
  }
}
