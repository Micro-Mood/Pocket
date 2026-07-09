/**
 * ================================================================================
 *  Pocket v1.0 — ESP32 挂件
 * ================================================================================
 *  硬件:
 *    - MCU:    ESP32 (S3/C3 兼容)
 *    - 屏幕:   ST7789 172×320 IPS TFT (SPI)
 *    - IMU:    LSM6DS3TRC 6 轴陀螺仪 (I2C)
 *    - 存储:   SPIFFS 闪存文件系统
 *
 *  功能:
 *    - 开机随机播放 5 个预设 GIF 之一
 *    - GPIO0 引脚识别切换到充电 GIF
 *    - GIF 播完后自动随机切换到下一个
 *    - 晃动唤醒 / 10 秒无操作自动熄屏省电
 *    - 串口 115200bps 输出全链路调试日志
 * ================================================================================
 */

#include <Arduino_GFX_Library.h>
#include <Adafruit_LSM6DS33.h>      // LSM6DS3TRC 依赖此头文件
#include <Adafruit_LSM6DS3TRC.h>
#include <SPIFFS.h>
#include <AnimatedGIF.h>

// ============================================================================
//  引脚定义 — ST7789 SPI 接口
// ============================================================================
#define TFT_SCL  1                  // SPI 时钟
#define TFT_SDA  7                  // SPI 数据 (MOSI)
#define TFT_DC   10                 // 数据/命令选择
#define TFT_CS   8                  // 片选
#define TFT_RST  3                  // 复位
#define TFT_BLK  6                  // 背光 PWM

// ============================================================================
//  引脚定义 — LSM6DS3TRC I2C 接口
// ============================================================================
#define IMU_SCL  5
#define IMU_SDA  4

// ============================================================================
//  引脚定义 — 充电 & 按键
// ============================================================================
#define PIN_CHRG   0                // 充电状态检测 (低有效, INPUT_PULLUP)
#define PIN_STDBY  2                // 待机状态检测 (低有效, INPUT_PULLUP)
#define PIN_BTN    0                // 多功能按钮 (复用 GPIO0)

// ============================================================================
//  屏幕参数
// ============================================================================
#define TFT_WIDTH   172
#define TFT_HEIGHT  320
#define TFT_ROTATION 2              // ST7789 旋转方向

// ============================================================================
//  背光控制
// ============================================================================
#define BLK_MAX     30              // 最大 PWM 占空比 (0~255, 30 足够亮)
#define BLK_FADE_MS 5               // 渐变每步延时 (ms)
#define BLK_TIMEOUT 10000           // 无操作熄屏超时 (ms)

// ============================================================================
//  IMU 配置
// ============================================================================
#define IMU_RETRY_MAX 50            // I2C 初始化最大重试次数
#define IMU_RETRY_MS  10            // 每次重试等待 (ms)
#define IMU_I2C_FREQ  100000        // I2C 时钟频率 (Hz, LSM6DS3TRC 支持 400k)

// ============================================================================
//  GIF 文件列表 (存放于 SPIFFS 根目录)
// ============================================================================
static const char *GIF_LIST[] = {
  "/twece.gif",
  "/anger.gif",
  "/disdain.gif",
  "/excited.gif",
  "/once.gif"
};
#define GIF_COUNT (sizeof(GIF_LIST) / sizeof(GIF_LIST[0]))
#define GIF_CHARGE "/charge.gif"    // 特殊 GIF

// ============================================================================
//  全局对象
// ============================================================================
Arduino_DataBus     *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCL, TFT_SDA, GFX_NOT_DEFINED);
Arduino_GFX         *gfx = new Arduino_ST7789(bus, TFT_RST, TFT_ROTATION, true,
                                              TFT_WIDTH, TFT_HEIGHT, 0, 0, 34, 0);
Adafruit_LSM6DS3TRC imu;
File                gifFile;
AnimatedGIF         gif;

// ============================================================================
//  运行时状态
// ============================================================================
static unsigned long g_lastActivityMs = 0;  // 上次活动时间戳 (用于熄屏计时)
static bool          g_screenOn      = true; // 屏幕当前亮灭状态

// ============================================================================
//  AnimatedGIF 库回调 — 打开 SPIFFS 文件
// ============================================================================
void *GIFOpenFile(const char *fname, int32_t *pSize) {
  gifFile = SPIFFS.open(fname);
  if (gifFile) {
    *pSize = gifFile.size();
    return (void *)&gifFile;
  }
  return NULL;
}

// ============================================================================
//  AnimatedGIF 库回调 — 关闭文件
// ============================================================================
void GIFCloseFile(void *pHandle) {
  File *f = static_cast<File *>(pHandle);
  if (f != NULL) f->close();
}

// ============================================================================
//  AnimatedGIF 库回调 — 读取文件数据
// ============================================================================
int32_t GIFReadFile(GIFFILE *pFile, uint8_t *pBuf, int32_t iLen) {
  File *f = static_cast<File *>(pFile->fHandle);
  int32_t remaining = pFile->iSize - pFile->iPos - 1;
  if (remaining < iLen) iLen = remaining;
  if (iLen <= 0) return 0;
  iLen = (int32_t)f->read(pBuf, iLen);
  pFile->iPos = f->position();
  return iLen;
}

// ============================================================================
//  AnimatedGIF 库回调 — 文件随机寻址
// ============================================================================
int32_t GIFSeekFile(GIFFILE *pFile, int32_t iPosition) {
  File *f = static_cast<File *>(pFile->fHandle);
  f->seek(iPosition);
  pFile->iPos = (int32_t)f->position();
  return pFile->iPos;
}

// ============================================================================
//  AnimatedGIF 库回调 — 逐帧绘制到屏幕
//  处理透明色 + disposal method，按行渲染 16-bit RGB565 位图
// ============================================================================
void GIFDraw(GIFDRAW *pDraw) {
  uint8_t  *pPixels   = pDraw->pPixels;       // 当前行像素索引
  uint16_t *pPalette  = pDraw->pPalette;       // 调色板 (RGB565)
  uint16_t  rowBuf[TFT_WIDTH];                 // 行缓冲区 (栈上, 172×2=344B)

  // --- 裁剪到屏幕宽度 ---
  int drawW = pDraw->iWidth;
  if (pDraw->iX + drawW > TFT_WIDTH) {
    drawW = TFT_WIDTH - pDraw->iX;
  }

  int drawY = pDraw->iY + pDraw->y;            // 绝对 Y 坐标
  if (drawY >= TFT_HEIGHT || pDraw->iX >= TFT_WIDTH || drawW < 1) {
    return;                                     // 超出屏幕范围, 跳过
  }

  // --- 处理 disposal method 2 (恢复背景色) ---
  if (pDraw->ucDisposalMethod == 2) {
    for (int x = 0; x < drawW; x++) {
      if (pPixels[x] == pDraw->ucTransparent) {
        pPixels[x] = pDraw->ucBackground;
      }
    }
    pDraw->ucHasTransparency = 0;               // 标记已无透明
  }

  // --- 带透明色的逐像素展开 ---
  if (pDraw->ucHasTransparency) {
    uint8_t *pEnd = pPixels + drawW;
    uint8_t  transp = pDraw->ucTransparent;
    int      curX = 0;

    while (curX < drawW) {
      // ▸ 扫描不透明像素段
      int runLen = 0;
      uint16_t *pOut = rowBuf;
      while (pPixels < pEnd) {
        uint8_t idx = *pPixels;
        if (idx == transp) break;               // 遇到透明, 结束此段
        *pOut++ = pPalette[idx];
        runLen++;
        pPixels++;
      }
      if (runLen) {
        gfx->draw16bitBeRGBBitmap(pDraw->iX + curX, drawY, rowBuf, runLen, 1);
        curX += runLen;
        runLen = 0;
      }

      // ▸ 跳过透明像素段 (不绘制)
      while (pPixels < pEnd && *pPixels == transp) {
        pPixels++;
        curX++;
      }
    }
  }
  // --- 无透明通道: 直接批量绘制整行 ---
  else {
    pPixels = pDraw->pPixels;
    for (int x = 0; x < drawW; x++) {
      rowBuf[x] = pPalette[*pPixels++];
    }
    gfx->draw16bitBeRGBBitmap(pDraw->iX, drawY, rowBuf, drawW, 1);
  }
}

// ============================================================================
//  初始化 AnimatedGIF 解码器 (大端像素格式匹配 ST7789)
// ============================================================================
void gifDecoderInit() {
  gif.begin(BIG_ENDIAN_PIXELS);
}

// ============================================================================
//  打开并播放指定 GIF 文件 (带错误日志)
// ============================================================================
void gifPlay(const char *path) {
  if (!SPIFFS.exists(path)) {
    Serial.printf(F("[GIF ] ERROR: file not found: %s\n"), path);
    return;
  }

  if (gif.open(path, GIFOpenFile, GIFCloseFile, GIFReadFile, GIFSeekFile, GIFDraw)) {
    Serial.printf(F("[GIF ] Opened: %s\n"), path);
  } else {
    Serial.printf(F("[GIF ] ERROR: decode failed, err=%d\n"), gif.getLastError());
    gif.close();
  }
}

// ============================================================================
//  随机选取一个普通 GIF 并播放
// ============================================================================
void gifPlayRandom() {
  unsigned int idx = esp_random() % GIF_COUNT;
  Serial.printf(F("[GIF ] Random: %u → %s\n"), idx, GIF_LIST[idx]);
  gifPlay(GIF_LIST[idx]);
}

// ============================================================================
//  渐变关闭背光 (BLK_MAX → 0)
// ============================================================================
void backlightFadeOut() {
  for (int i = 0; i <= BLK_MAX; i++) {
    analogWrite(TFT_BLK, BLK_MAX - i);
    delay(BLK_FADE_MS);
  }
  g_screenOn = false;
}

// ============================================================================
//  瞬间开启背光
// ============================================================================
void backlightOn() {
  analogWrite(TFT_BLK, BLK_MAX);
  g_screenOn = true;
}

// ============================================================================
//  重置活动计时器 (晃动 / 按键时调用)
// ============================================================================
void resetActivityTimer() {
  g_lastActivityMs = millis();
}

// ============================================================================
//  进入熄屏省电循环，直到 IMU 检测到晃动
// ============================================================================
void enterSleepLoop() {
  Serial.println(F("[BLK ] Timeout → dimming..."));
  backlightFadeOut();
  Serial.println(F("[BLK ] Screen off, waiting for shake..."));

  while (1) {
    if (imu.shake()) {
      backlightOn();
      resetActivityTimer();
      Serial.println(F("[BLK ] Wakeup! Backlight ON"));
      return;
    }
  }
}

// ============================================================================
//  Arduino setup() — 硬件初始化
// ============================================================================
void setup() {
  // --- 串口 ---
  Serial.begin(115200);
  delay(100);  // 等待 USB-CDC 枚举
  Serial.println(F("\n===== Pocket v1.0 Boot ====="));
  Serial.printf (F("[BOOT] %s, Flash: %dMB\n"),
                 ESP.getChipModel(), ESP.getFlashChipSize() / 1048576);

  // --- GPIO ---
  // IMU I2C 引脚复用为开漏 + 上拉
  gpio_config_t ioCfg = {
    .pin_bit_mask = (1ULL << IMU_SCL) | (1ULL << IMU_SDA),
    .mode         = GPIO_MODE_INPUT_OUTPUT,
    .pull_up_en   = GPIO_PULLUP_ENABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type    = GPIO_INTR_DISABLE
  };
  gpio_config(&ioCfg);

  pinMode(TFT_BLK, OUTPUT);
  pinMode(PIN_CHRG,  INPUT_PULLUP);
  pinMode(PIN_STDBY, INPUT_PULLUP);
  Serial.println(F("[GPIO] Pins configured"));

  // --- 显示屏 ---
  gfx->begin(80000000);                         // SPI 80MHz
  gfx->fillScreen(BLACK);
  Serial.println(F("[DISP] ST7789 172×320 init OK"));

  // --- IMU (六轴陀螺仪) ---
  Serial.print(F("[IMU ] I2C init... "));
  Wire.begin(IMU_SDA, IMU_SCL, IMU_I2C_FREQ);

  int retry = 0;
  while (!imu.begin_I2C() && ++retry <= IMU_RETRY_MAX) {
    delay(IMU_RETRY_MS);
  }
  if (retry > IMU_RETRY_MAX) {
    Serial.println(F("FAILED — check wiring!"));
  } else {
    Serial.printf(F("OK (retry=%d)\n"), retry);
  }

  imu.reset();
  delay(100);

  // 配置量程 & 采样率
  imu.setGyroRange(LSM6DS_GYRO_RANGE_2000_DPS);
  imu.setAccelRange(LSM6DS_ACCEL_RANGE_16_G);
  imu.setAccelDataRate(LSM6DS_RATE_12_5_HZ);
  imu.setGyroDataRate(LSM6DS_RATE_12_5_HZ);

  // 使能晃动唤醒中断 (阈值=0 表示极灵敏)
  imu.configInt1(false, false, false, false, true);
  imu.enableWakeup(true, 0, 1);
  Serial.println(F("[IMU ] LSM6DS3TRC configured (shake wakeup)"));

  // --- 文件系统 ---
  SPIFFS.begin();
  Serial.printf(F("[FS  ] SPIFFS: total=%dKB, used=%dKB\n"),
                SPIFFS.totalBytes() / 1024, SPIFFS.usedBytes() / 1024);

  // --- GIF 解码器 ---
  gifDecoderInit();
  gifPlayRandom();

  // --- 背光 & 计时 ---
  backlightOn();
  resetActivityTimer();

  Serial.println(F("===== Boot Complete =====\n"));
}

// ============================================================================
//  Arduino loop() — 主状态机
// ============================================================================
void loop() {
  // ── 状态 1: 正在播放 GIF, 喂帧 ──
  if (gif.playFrame(true, NULL)) {
    // 帧仍在播放中, 无额外操作
  }
  // ── 状态 2: GPIO0 按钮按下 → 充电 GIF ──
  else if (digitalRead(PIN_BTN) == LOW) {
    Serial.println(F("[BTN ] Pressed → charge.gif"));
    gif.close();
    gifPlay(GIF_CHARGE);
  }
  // ── 状态 3: GIF 播放完毕 → 随机下一个 ──
  else {
    Serial.println(F("[GIF ] Ended → random next"));
    gif.close();
    gifPlayRandom();
  }

  // ── 省电逻辑 ──
  if (imu.shake()) {
    resetActivityTimer();                       // 晃动 → 重置倒计时
  } else if (!g_screenOn) {
    // 屏幕已熄, 等待晃动唤醒 (不在计时)
    enterSleepLoop();
  } else if (millis() - g_lastActivityMs > BLK_TIMEOUT) {
    enterSleepLoop();
  }

  // ── 主动 yield 给 FreeRTOS (防止看门狗) ──
  delay(1);
}

















