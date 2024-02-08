// M5Stack BASIC で　TWELITEの状態表示、IO操作を行う
#include <M5Stack.h>

#define TEXT_HEIGHT 30
#define READ_DATA_LEN 25
#define CMD_DATA_LEN 14
#define CMD_STR_LEN 32

uint8_t readData[READ_DATA_LEN];
bool recvFlag = false ;
bool recvMSB = true ;
int  recvLen = 0 ;

// [0] - 宛先アドレス
// [1] - コマンド番号　(0x80 固定)
// [2] - 書式バージョン(0x01 固定)
// [3] - Digital ON/OFF
// [4] - Digital Switch No
// [ 5- 6] - PWM1
// [ 7- 8] - PWM2
// [ 9-10] - PWM3
// [11-12] - PWM4
// [13] - CheckSum
unsigned cmdData[CMD_DATA_LEN];
char cmdStr[CMD_STR_LEN] ;

int cmdPos = 1 ;
bool digitalSwitch[4];   // True:ON  False:OFF
uint16_t analogLevel[4]; // 0~1024

static char bin2Hex[] = {"0123456789ABCDEF"} ;

void hexCharaSet(uint8_t data,char *buffPos) {
  buffPos[0] = bin2Hex[data >> 4] ;
  buffPos[1] = bin2Hex[data & 0x0F] ;
}

// ASCII文字の16進数一桁を8Bit整数に変換
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

// 送信コマンドのバイナリー配列を生成
void sendCommandSet( ) {
  memset(cmdData,0,CMD_DATA_LEN) ;
  cmdData[0] = 0x78 ;
  cmdData[1] = 0x80 ;
  cmdData[2] = 0x01 ;
  cmdData[3] = digitalSwitch[3] << 3 | digitalSwitch[2] << 2 | digitalSwitch[1] << 1 | digitalSwitch[0] ;
  cmdData[4] = 0x0F ;
  cmdData[5] = analogLevel[0] >> 8 ;
  cmdData[6] = analogLevel[0] & 0xFF ;
  cmdData[7] = analogLevel[1] >> 8 ;
  cmdData[8] = analogLevel[1] & 0xFF ;
  cmdData[9] = analogLevel[2] >> 8 ;
  cmdData[10] = analogLevel[2] & 0xFF ;
  cmdData[11] = analogLevel[3] >> 8 ;
  cmdData[12] = analogLevel[3] & 0xFF ;
  cmdData[13] = 0 ;
}

// 送信コマンドのバイナリー配列をASCII文字のコマンド文字列に変換
void sendCommandBuild( ) {
  sendCommandSet( ) ;

  cmdStr[0] = ':' ;
  for (int i=0;i<13;i++) {
    cmdData[13] += cmdData[i] ;
    hexCharaSet(cmdData[i],&cmdStr[1+i*2]) ;
  }
  cmdData[13] = ~cmdData[13] + 1 ;
  hexCharaSet(cmdData[13],&cmdStr[1+13*2]) ;
  cmdStr[1+14*2  ] = 0x0d ;
  cmdStr[1+14*2+1] = 0x0a ;
  cmdStr[1+14*2+2] = 0x00 ;
}

// ASCII文字のコマンド文字列を表示
void cmdStrDump( ) {
  int xP = 0;
  int yP = TEXT_HEIGHT+1;
  M5.Lcd.fillRect(0, TEXT_HEIGHT+1 , 320, TEXT_HEIGHT*3 , TFT_BLACK);

  int dLen = 0 ;
  for (int i=0;cmdStr[i] >=' ';i++) {
    xP += M5.Lcd.drawChar(cmdStr[i], xP, yP, 4);
    if (dLen > 20) {
        xP = 0;
        yP += TEXT_HEIGHT ;
        dLen = 0 ;
    }
    dLen ++ ;
  }
}

// 読み込んだ文字列を表示
void readDataDump( ) {
  int xP = 0;
  int yP = TEXT_HEIGHT*5+1;
  M5.Lcd.fillRect(0, yP , 320, 320 , TFT_BLACK);

  int dLen = 0 ;
  M5.Lcd.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
  for (int i=0;readData[i] >=' ';i++) {
    xP += M5.Lcd.drawChar(readData[i], xP, yP, 4);
    if (dLen > 20) {
        xP = 0;
        yP += TEXT_HEIGHT ;
        dLen = 0 ;
    }
    dLen ++ ;
  }
  M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);
}

void sendCommandDisp( ) {
  int xP = 0;
  int yP = TEXT_HEIGHT*5+1;
  char dispStr[12] ;

  memset(dispStr,0,12) ;
  sendCommandSet( ) ;

//  M5.Lcd.fillRect(0, yP , 320, 320 , TFT_BLACK);  // なくても平気なので、チラツキ防止のために外す
  M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);

  // 送信先ID
  xP = M5.Lcd.drawString("SendTo:", 0 , yP , 4);
  hexCharaSet(cmdData[0],dispStr) ;
  setCursorColor(cmdPos == 0);
  xP += M5.Lcd.drawString(dispStr, xP, yP, 4);
  setCursorColor(false);
  // D端子
  yP += TEXT_HEIGHT ;
  xP = M5.Lcd.drawString("D: ", 2 , yP , 4);
  for (int i=0;i<4;i++) {
    setCursorColor(cmdPos == 1 + i);
    xP += M5.Lcd.drawChar((digitalSwitch[i] ? 'L' : 'H') , xP , yP , 4);
    setCursorColor(false); xP += M5.Lcd.drawChar(' ', xP , yP , 4);
  }
  // A端子
  yP += TEXT_HEIGHT ;
  xP = M5.Lcd.drawString("A: ", 2 , yP , 4);
  for (int i=0;i<4;i++) {
    setCursorColor(cmdPos == 5 + i);
    sprintf(dispStr,"%04d",analogLevel[i]) ;
    xP += M5.Lcd.drawString(dispStr, xP , yP , 4);
    setCursorColor(false); xP += M5.Lcd.drawString((i < 3 ? " / " : " ") , xP , yP , 4);
  }
}

void setCursorColor(bool invert) {
  if (invert) {
    M5.Lcd.setTextColor(TFT_BLACK ,TFT_YELLOW);
  } else {
    M5.Lcd.setTextColor(TFT_WHITE , TFT_BLACK);
  }
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
  xP = M5.Lcd.drawString("From:", 0 , yP , 4);
  hexCharaSet(readData[0],dispStr) ;
  xP += M5.Lcd.drawString(dispStr, xP, yP, 4);
  xP += M5.Lcd.drawString("-", xP , yP , 4);
  // 送信元個体識別番号
  for (int i=5;i<=8;i++) {
    hexCharaSet(readData[i],dispStr) ;
    xP += M5.Lcd.drawString(dispStr, xP, yP, 4);
  }
  // 宛先の論理デバイス番号
  xP += M5.Lcd.drawString(" / To:", xP, yP, 4);
  hexCharaSet(readData[9],dispStr) ;
  xP += M5.Lcd.drawString(dispStr, xP, yP, 4);
  xP += M5.Lcd.drawString(" ", xP, yP, 4);

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

  // D端子
  yP += TEXT_HEIGHT ;
  xP = M5.Lcd.drawString("D: ", 0 , yP , 4);
  memset(dispStr,' ',8) ;
  dispStr[8] = 0x00 ;
  dispStr[0] = (readData[16] & readData[17] & 0x01) != 0 ? 'L' : 'H' ;
  dispStr[2] = (readData[16] & readData[17] & 0x02) != 0 ? 'L' : 'H' ;
  dispStr[4] = (readData[16] & readData[17] & 0x04) != 0 ? 'L' : 'H' ;
  dispStr[6] = (readData[16] & readData[17] & 0x08) != 0 ? 'L' : 'H' ;
  xP += M5.Lcd.drawString(dispStr, xP, yP, 4);
  // A端子
  yP += TEXT_HEIGHT ;
  xP = M5.Lcd.drawString("A: ", 0 , yP , 4);
  int aData = (readData[18] * 4 + (readData[22] & 0b00000011)) * 4 ; 
  sprintf(dispStr,"%04d",aData) ;
  xP += M5.Lcd.drawString(dispStr, xP, yP, 4);
  aData = (readData[19] * 4 + ((readData[22] & 0b00001100) >> 2)) * 4 ; 
  sprintf(dispStr," / %04d",aData) ;
  xP += M5.Lcd.drawString(dispStr, xP, yP, 4);
  aData = (readData[20] * 4 + ((readData[22] & 0b00110000) >> 4)) * 4 ; 
  sprintf(dispStr," / %04d",aData) ;
  xP += M5.Lcd.drawString(dispStr, xP, yP, 4);
  aData = (readData[21] * 4 + ((readData[22] & 0b11000000) >> 6)) * 4 ; 
  sprintf(dispStr," / %04d ",aData) ;
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
  M5.Lcd.drawCentreString("TWELITE STATUS", 320 / 2, 4, 4);
  M5.Lcd.setTextColor(TFT_WHITE, TFT_BLACK);

  // --- COMMAND INIT ---
  for (int i=0;i<4;i++) {
    digitalSwitch[i] = false; // True:ON  False:OFF
    analogLevel[i] = 1024;    // 0~1024
  }
  sendCommandDisp( );
  sendCommandBuild( ) ;
  Serial1.print(cmdStr) ;
}

void loop() {
  M5.update();  //本体のボタン状態更新
  // データ送信
  if (M5.BtnA.wasPressed()) { // Next Command
    cmdPos = (cmdPos + 1) % 9 ;
    sendCommandDisp( );
  }
  if (M5.BtnB.wasPressed()) { // Up
    if (cmdPos == 0) {
      
    } else
    if (1 <= cmdPos && cmdPos <= 4) {
      digitalSwitch[cmdPos-1] = digitalSwitch[cmdPos-1] ? false : true ;
    } else {
      analogLevel[cmdPos-5] = (analogLevel[cmdPos-5] <= 64) ? 0 : analogLevel[cmdPos-5] - 64 ;
    }
    sendCommandDisp( );
    sendCommandBuild( );
    Serial1.print(cmdStr) ;
  }
  if (M5.BtnC.wasPressed()) { // Down
    if (cmdPos == 0) {
      
    } else
    if (1 <= cmdPos && cmdPos <= 4) {
      digitalSwitch[cmdPos-1] = digitalSwitch[cmdPos-1] ? false : true ;
    } else {
      analogLevel[cmdPos-5] = analogLevel[cmdPos-5] += 64 ;
      if (analogLevel[cmdPos-5] > 1024) analogLevel[cmdPos-5] = 1024 ;
    }
    sendCommandDisp( );
    sendCommandBuild( );
    Serial1.print(cmdStr) ;
  }

  // 受信データ処理
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
      statDisp(recvLen) ;
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
