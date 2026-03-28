#include <Arduino_GFX_Library.h>
#include <Adafruit_LSM6DS33.h>
#include <Adafruit_LSM6DS3TRC.h> 
#include <SPIFFS.h>

#include <AnimatedGIF.h>

#define SCL 1
#define SDA 7
#define DC  10
#define CS  8
#define RES 3
#define BLK 6

#define IMU_SCL 5
#define IMU_SDA 4

#define CHRG  0
#define STDBY 2

#define BLK_ON  analogWrite(BLK, 30)
#define BLK_OFF analogWrite(BLK, 0)


Arduino_DataBus     *bus = new Arduino_ESP32SPI (DC, CS, SCL, SDA, GFX_NOT_DEFINED );
Arduino_GFX         *gfx = new Arduino_ST7789   (bus, RES, 2, true, 172, 320,0,0,34,0);
Adafruit_LSM6DS3TRC imu;
File                gifFile;
AnimatedGIF         gif;
unsigned long long  lasttime = millis();

void *GIFOpenFile(const char *fname, int32_t *pSize){
  gifFile = SPIFFS.open(fname);
  if (gifFile)
  {
      *pSize = gifFile.size();
      return (void *)&gifFile;
  }
  return NULL;
}
void GIFCloseFile(void *pHandle){
  File *f = static_cast<File *>(pHandle);
  if (f != NULL)
      f->close();
}
int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen){
  int32_t iBytesRead;
  iBytesRead = iLen;
  File *f = static_cast<File *>(pFile->fHandle);
  if ((pFile->iSize - pFile->iPos) < iLen)   iBytesRead = pFile->iSize - pFile->iPos - 1;
  if (iBytesRead <= 0)  return 0;
  iBytesRead = (int32_t)f->read(pBuf, iBytesRead);
  pFile->iPos = f->position();
  return iBytesRead;
}
int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition){
  File *f = static_cast<File *>(pFile->fHandle);
  f->seek(iPosition);
  pFile->iPos = (int32_t)f->position();
  return pFile->iPos;
}
void GIFDraw(GIFDRAW *pDraw){
  uint8_t *s;
  uint16_t *d, *usPalette, usTemp[172];
  int x, y, iWidth;

  iWidth = pDraw->iWidth;
  if (iWidth + pDraw->iX > 172){
    iWidth = 172 - pDraw->iX;
  }
  usPalette = pDraw->pPalette;
  y = pDraw->iY + pDraw->y; 
  if (y >= 320 || pDraw->iX >= 172 || iWidth < 1){
    return;
  }
  s = pDraw->pPixels;
  if (pDraw->ucDisposalMethod == 2){
    for (x = 0; x < iWidth; x++){
      if (s[x] == pDraw->ucTransparent){
        s[x] = pDraw->ucBackground;
      }
    }
    pDraw->ucHasTransparency = 0;
  }
  if (pDraw->ucHasTransparency) {
    uint8_t *pEnd, c, ucTransparent = pDraw->ucTransparent;
    int x, iCount;
    pEnd = s + iWidth;
    x = 0;
    iCount = 0; 
    while (x < iWidth){
      c = ucTransparent - 1;
      d = usTemp;
      while (c != ucTransparent && s < pEnd){
        c = *s++;
        if (c == ucTransparent) {s--; }
        else {
          *d++ = usPalette[c];
          iCount++;
        }
      }           
      if (iCount) {
        gfx->draw16bitBeRGBBitmap(pDraw->iX + x, y, usTemp, iCount, 1);
        x += iCount;
        iCount = 0;
      }
      c = ucTransparent;
      while (c == ucTransparent && s < pEnd){
        c = *s++;
        if (c == ucTransparent){iCount++;}
        else{s--;}
      }
      if (iCount){
        x += iCount;
        iCount = 0;
      }
    }
  }
  else{
    s = pDraw->pPixels;
    for (x = 0; x < iWidth; x++){
      usTemp[x] = usPalette[*s++];
    }
    gfx->draw16bitBeRGBBitmap(pDraw->iX, y, usTemp, iWidth, 1);
  }
}
void DecodeInit(){
  gif.begin(BIG_ENDIAN_PIXELS);
}
void OpenGIF(const char *path){
  if ( SPIFFS.exists(path) ){
    if (gif.open(path, GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw));
    else{
      // Serial.printf("打开GIF失败 : %d\n", gif.getLastError());
      gif.close();
    }
  }
}
void setup() {
  // Serial.begin(115200);
  gpio_config_t cfg = {
      .pin_bit_mask = 1ULL<<IMU_SCL | 1ULL<<IMU_SDA,
      .mode         = GPIO_MODE_INPUT_OUTPUT,
      .pull_up_en   = GPIO_PULLUP_ENABLE,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .intr_type    = GPIO_INTR_DISABLE
  };
  gpio_config(&cfg);
  pinMode(BLK, OUTPUT);
  pinMode(CHRG, INPUT_PULLUP);
  pinMode(STDBY, INPUT_PULLUP);

  gfx->begin(80000000);
  gfx->fillScreen(0x000000);

  Wire.begin(IMU_SDA, IMU_SCL, 10000);
  while(imu.begin_I2C());
  imu.reset();
  delay(100);
  imu.setGyroRange(LSM6DS_GYRO_RANGE_2000_DPS);
  imu.setAccelRange(LSM6DS_ACCEL_RANGE_16_G);
  imu.setAccelDataRate(LSM6DS_RATE_12_5_HZ);
  imu.setGyroDataRate(LSM6DS_RATE_12_5_HZ);
  imu.configInt1(false, false, false, false, true);
  imu.enableWakeup(true, 0 ,1);

  SPIFFS.begin();
  DecodeInit();
  unsigned int random = esp_random()%5;
  switch(random){
    case 0:
      OpenGIF("/twece.gif");
      break;
    case 1:
      OpenGIF("/anger.gif");
      break;
    case 2:
      OpenGIF("/disdain.gif");
      break;
    case 3:
      OpenGIF("/excited.gif");
      break;
    case 4:
      OpenGIF("/once.gif");
      break;
  }
  BLK_ON;
  lasttime = millis();
}
void loop() {
  if(gif.playFrame(true, NULL)){
    //NULL
  }
  else if(digitalRead(0) == 0){
    gif.close();
    OpenGIF("/charge.gif");
  }
  else{
    gif.close();
    unsigned int random = esp_random()%5;
    switch(random){
      case 0:
        OpenGIF("/twece.gif");
        break;
      case 1:
        OpenGIF("/anger.gif");
        break;
      case 2:
        OpenGIF("/disdain.gif");
        break;
      case 3:
        OpenGIF("/excited.gif");
        break;
      case 4:
        OpenGIF("/once.gif");
        break;
    }
  }
  if(imu.shake()){lasttime = millis();}
  else if(millis() - lasttime > 10000){
    for(int i=0; i<=30; i++) {analogWrite(BLK, 30-i);delay(5);}//BLK_OFF;
    while(1){
      if(imu.shake()){
        BLK_ON;
        lasttime = millis();
        break;
      }
    }
  }
}
















