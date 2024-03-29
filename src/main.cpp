//Controller for M5Fly
#include <Arduino.h>
#include <M5AtomS3.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <MPU6886.h>
#include <MadgwickAHRS.h>
#include <atoms3joy.h>

#define CHANNEL 1

#define ANGLECONTROL 0
#define RATECONTROL 1
#define ANGLECONTROL_W_LOG 2
#define RATECONTROL_W_LOG 3
#define RESO10BIT (4096)

esp_now_peer_info_t peerInfo;

float Throttle;
float Phi, Theta, Psi;
uint16_t Phi_bias =2048;
uint16_t Theta_bias = 2048;
uint16_t Psi_bias =2048;
uint16_t Throttle_bias = 2048;
short xstick=0;
short ystick=0;
uint8_t Mode=ANGLECONTROL;
volatile uint8_t Loop_flag=0;


unsigned long stime,etime,dtime;
byte axp_cnt=0;

char data[140];
uint8_t senddata[19];
uint8_t disp_counter=0;

//StampFly MAC ADDRESS
//1 F4:12:FA:66:80:54 (Yellow)
const uint8_t addr[6] = {0xF4, 0x12, 0xFA, 0x66, 0x80, 0x54};

void rc_init(void);
void data_send(void);
void show_battery_info();
void voltage_print(void);

void rc_init(void)
{
  // ESP-NOW初期化
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  if (esp_now_init() == ESP_OK) {
    USBSerial.println("ESPNow Init Success");
  } else {
    USBSerial.println("ESPNow Init Failed");
    ESP.restart();
  }

  //ペアリング
  memcpy(peerInfo.peer_addr, addr, 6);
  peerInfo.channel = CHANNEL;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) 
  {
        USBSerial.println("Failed to add peer");
        return;
  }
  esp_wifi_set_channel(CHANNEL, WIFI_SECOND_CHAN_NONE);
}

//周期カウンタ割り込み関数
hw_timer_t * timer = NULL;
void IRAM_ATTR onTimer() 
{
  Loop_flag = 1;
}

void setup() {
  M5.begin();
  Wire1.begin(38, 39, 100*1000);
  rc_init();
  
  M5.Lcd.setRotation( 2 );
  M5.Lcd.setTextFont(2);
  M5.Lcd.setCursor(4, 2);
  M5.Lcd.println("ATOMS3Joy");
  //Display init
  //M5.Lcd.fillScreen(RED);       // 画面全体の塗りつぶし
  //M5.Lcd.setCursor(9, 10);      // カーソル位置の指定
  //M5.Lcd.setTextFont(1);        // フォントの指定
  //M5.Lcd.setTextSize(2);        // フォントサイズを指定（倍数）
  //M5.Lcd.setTextColor(WHITE, RED);
  //M5.Lcd.println("AtomFly2.0");           
  //for (uint8_t i=0;i<50;i++)show_battery_info();

  byte error, address;
  int nDevices;

////////////////////////////////////////////////////////
  USBSerial.println("Scanning... Wire1");

  nDevices = 0;
  for (address = 1; address < 127; address++ )
  {
    Wire1.beginTransmission(address);
    error = Wire1.endTransmission();

    if (error == 0)
    {
      USBSerial.print("I2C device found at address 0x");
      if (address < 16)
        USBSerial.print("0");
      USBSerial.print(address, HEX);
      USBSerial.println("  !");

      nDevices++;
    }
    else if (error == 4)
    {
      USBSerial.print("Unknown error at address 0x");
      if (address < 16)
        USBSerial.print("0");
      USBSerial.println(address, HEX);
    }
  }
  if (nDevices == 0)
    USBSerial.println("No I2C devices found\n");
  else
    USBSerial.println("done\n");

  //割り込み設定
  timer = timerBegin(1, 80, true);
  timerAttachInterrupt(timer, &onTimer, true);
  timerAlarmWrite(timer, 10000, true);
  timerAlarmEnable(timer);
  delay(100);
}

uint8_t check_mode_change(void)
{
  uint8_t state;
  static uint8_t flag =0;
  state = 0;
  if (flag==0)
  {
    if (getModeButton() == 1)
    {
      flag = 1;
    }
  }
  else
  {
    if (getModeButton() == 0)
    {
      flag = 0;
      state = 1;
    }
  }
  //USBSerial.printf("%d %d\n\r", state, flag);
  return state;
}


void loop() {
  while(Loop_flag==0);
  Loop_flag = 0;
  etime = stime;
  stime = micros();
  dtime = stime - etime;

  M5.update();
  joy_update();
  
  if (check_mode_change() == 1)
  {
    if (Mode==ANGLECONTROL)Mode=RATECONTROL;
    else Mode = ANGLECONTROL;
  }

  uint16_t _throttle = getThrottle();
  uint16_t _phi = getAileron();
  uint16_t _theta = getElevator();
  uint16_t _psi = getRudder();

  if(getArmButton()==1)
  {
    //Throttle_bias = _throttle;
    Phi_bias = _phi;
    Theta_bias = _theta;
    Psi_bias = _psi;
  }

  Throttle = (float)(_throttle - Throttle_bias)/(float)(RESO10BIT*0.5);
  Phi = (float)(_phi - Phi_bias)/(float)(RESO10BIT*0.5);
  Theta = -(float)(_theta - Theta_bias)/(float)(RESO10BIT*0.5);
  Psi = (float)(_psi - Psi_bias)/(float)(RESO10BIT*0.5);

  uint8_t* d_int;
  
  d_int = (uint8_t*)&Psi;
  senddata[0]=d_int[0];
  senddata[1]=d_int[1];
  senddata[2]=d_int[2];
  senddata[3]=d_int[3];

  d_int = (uint8_t*)&Throttle;
  senddata[4]=d_int[0];
  senddata[5]=d_int[1];
  senddata[6]=d_int[2];
  senddata[7]=d_int[3];

  d_int = (uint8_t*)&Phi;
  senddata[8]=d_int[0];
  senddata[9]=d_int[1];
  senddata[10]=d_int[2];
  senddata[11]=d_int[3];

  d_int = (uint8_t*)&Theta;
  senddata[12]=d_int[0];
  senddata[13]=d_int[1];
  senddata[14]=d_int[2];
  senddata[15]=d_int[3];

  senddata[16]=getArmButton();
  senddata[17]=getFlipButton();
  senddata[18]=Mode;
  
  //送信
  esp_err_t result = esp_now_send(peerInfo.peer_addr, senddata, sizeof(senddata));

  //Display information
  //float vbat =0.0;// M5.Axp.GetBatVoltage();
  //int8_t bat_charge_p = int8_t((vbat - 3.0) / 1.2 * 100);
  
  M5.Lcd.setCursor(4, 2+disp_counter*17);
  switch (disp_counter)
  {
    case 0:
      M5.Lcd.printf("ATOMS3Joy");
      break;
    case 1:
      M5.Lcd.printf("Volt: %5.1fV", Battery_voltage);
      //M5.Lcd.printf("X:%4d",xstick);
      break;
    case 2:
      M5.Lcd.printf("FPS: %5.1f",1000000.0/dtime);
      //M5.Lcd.printf("Y:%4d",ystick);
      break;
    case 3:
      if( Mode == ANGLECONTROL )      M5.Lcd.printf("-STABILIZE-");
      else if ( Mode == RATECONTROL ) M5.Lcd.printf("-ACRO-     ");
      //M5.Lcd.printf("Phi:%5.1f",Phi*180/3.14159);
      break;
    case 4:
      //M5.Lcd.printf("Tht:%5.1f",Theta*180/3.14159);
      break;
    case 5:
      //M5.Lcd.printf("Psi:%5.1f",Psi*180/3.14159);
      break;
    case 6:
      //M5.Lcd.printf("FPS:%5.1f",1000000.0/dtime);
      break;
    case 7:
      //M5.Lcd.printf("Vlt:%3.1fV", Battery_voltage);
      break;
    case 8:
      //M5.Lcd.printf("Chg:%3d%%", bat_charge_p);
      break;
    case 9:
      //disp_counter++;
      //M5.Lcd.setCursor(2, 5+disp_counter*17);
      //if( Mode == ANGLECONTROL ) M5.Lcd.printf("-STABILIZE-");
      //else if ( Mode == RATECONTROL ) M5.Lcd.printf("-ACRO-     ");
      //else if ( Mode == ANGLECONTROL_W_LOG) {} //M5.Lcd.printf("-STABILIZE.L-");
      //else if ( Mode == RATECONTROL_W_LOG ) {} //M5.Lcd.printf("-ACRO.L-     ");
      break;

  }
  disp_counter++;
  if(disp_counter==11)disp_counter=0;

  //Reset
  if( /*M5.Axp.GetBtnPress() == 2*/ 0 ){
    // 電源ボタンクリック
    //M5.Lcd.println("AtomFly2.0"); 
    esp_restart();
  } 

}

void show_battery_info(){
  #if 0
  // バッテリー電圧表示
  double vbat = 0.0;
  int8_t bat_charge_p = 0;

  vbat = M5.Axp.GetBatVoltage();
  M5.Lcd.setCursor(5, 100);
  //M5.Lcd.setTextSize(1);
  M5.Lcd.printf("Volt:\n %8.2fV", vbat);

  // バッテリー残量表示
  bat_charge_p = int8_t((vbat - 3.0) / 1.2 * 100);
  M5.Lcd.setCursor(5, 140);
  M5.Lcd.printf("Charge:\n %8d%%", bat_charge_p);
#endif
}

void voltage_print(void)
{

  M5.Lcd.setCursor(0, 17, 2);
  M5.Lcd.printf("%3.1fV", Battery_voltage);
}
