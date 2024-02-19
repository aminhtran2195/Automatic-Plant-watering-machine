#include "Keypad.h"
#include "LiquidCrystal_I2C.h"
#include "Mylib.h"
#include "amt1001_ino.h"
#include "DS1307.h"
#include "EEPROM.h"

#define trigger 4

#define MODE1     0X01
#define MODE2     0X02
#define MODE3     0X03
#define MODE4     0X04

#define MODE_TM  0X01
#define MODE_HM  0X02

#define BACK    0X00
#define NEXT    0X01
#define CANCEL  0x02
#define SAVED   0X03
#define ENTERED 0x05

#define TIMEOUT 15000 // timeout là 15s

#define HR_CHG    0      // define các giá trị phục vụ cho việc xóa màn hình
#define OTHER_CHG 1 

const byte rows = 4; //số hàng
const byte columns = 3; //số cột

char keys[rows][columns] = 
{
  {'1', '2', '3'},
  {'4', '5', '6'},
  {'7', '8', '9'},
  {'*', '0', '#'},
};

uint8_t RHpin =  A2;
uint8_t TMPpin = A3;

byte rowPins[rows] = {5, 6, 7, 8};  // các pin nối với keypad  PD 5->7, PB0
byte columnPins[columns] = {9, 10, 11}; // PB1 -> 3

Keypad keypad = Keypad(makeKeymap(keys), rowPins, columnPins, rows, columns);

uint8_t hr_atv[4] = {0}, mins_atv[4] = {0}; // thời gian hẹn giờ tưới, gồm 4 khoảng thời gian
uint8_t state = 0;
char key = 0;
// độ ẩm dưới và độ ẩm trên - dành cho mode 2/4
uint8_t hm1 = 35, hm2 = 65;

//uint8_t tmp_hm1,tmp_hm2; 

uint8_t mode = MODE1;                // mac dinh la mode 1, tưới khi người dùng nhấn phím
uint8_t in_mode = MODE_TM;           // Mặc định là tưới theo tg
uint8_t mins_opr = 0, secs_opr = 10; // thời gian tưới, mặc định là 10 giây
uint8_t thr_temp = 31;               // nhiệt độ ngưỡng
              

// 2 biến dành riêng cho việc hiển thị luân phiên nhiệt độ và độ ẩm
unsigned long tmtp = 0;
unsigned int cnt = 0;  

bool on_atv = false;
uint8_t present_secs = 50;
uint8_t cent_1 = 20;
int count_down;

/**  CÁC GIÁ TRỊ LƯU TRONG EEPROM **/
/*
BIẾN              ĐỊA CHỈ       ĐỊNH NGHĨA
hr_atv[0]         0             Giờ hẹn 1
hr_atv[1]         1             Giờ hẹn 2
hr_atv[2]         2             Giờ hẹn 3
hr_atv[3]         3             Giờ hẹn 4
mins_atv[0]       4             Phút hẹn 1
mins_atv[1]       5             Phút hẹn 2
mins_atv[2]       6             Phút hẹn 3
mins_atv[3]       7             Phút hẹn 4
hm1               8             Độ ẩm ngưỡng dưới
hm2               9             Độ ẩm ngưỡng trên
thr_temp          10            Nhiệt độ ngưỡng
mins_opr          11            Thời gian tưới (phút)
secs_opr          12            Thời gian tưới (giây)
mode              13            chế độ tưới
in_mode           14            cách thức tưới
cent_1            15            Thế kỉ trừ đi 1
*/

// đối tượng rtc và lcd 
DS1307 rtc(0x68);
LiquidCrystal_I2C lcd(0x27, 16, 2);

void KeyState_Check();

void GetInCsMode();
void MainIntf_Display();
void Operation();

void ParameterInit(){
  for(uint8_t i=0;i<4;i++){
    hr_atv[i] = EEPROM.read(i);
    mins_atv[i] = EEPROM.read(i+4);
  }
  hm1 = EEPROM.read(8);
  hm2 = EEPROM.read(9);
  thr_temp = EEPROM.read(10);
  mins_opr = EEPROM.read(11);
  secs_opr = EEPROM.read(12);
  mode = EEPROM.read(13);
  in_mode = EEPROM.read(14);
  cent_1 = EEPROM.read(15);
}

void setup() {
  Wire.begin();
  Serial.begin(9600); 
  pinMode(TMPpin,INPUT); // nhiet do
  pinMode(RHpin,INPUT); // do am
  pinMode(trigger,OUTPUT);
  digitalWrite(trigger,LOW);
  lcd.init();
  lcd.backlight();
  ParameterInit();
  count_down = 60*mins_opr + secs_opr;
}

void loop() {  
  // Hàm kiểm tra người dùng nhấn đè '*' để vào tùy chỉnh chế độ
  if(!on_atv) KeyState_Check();
  // Hàm hiển thị thời gian lấy từ rtc lên LCD, hiển thị mode, nhiệt độ và độ ẩm
  MainIntf_Display();
  // hàm kiểm tra mode, vận hành máy bơm
  Operation();
}

uint8_t ChooseMode();
uint8_t Humidity1_Change();
uint8_t Humidity1_Display(uint8_t pr);
uint8_t SetOprTime();
uint8_t Humidity2_Change();
uint8_t DailyTime_Setting();

/************** Hàm chờ người dùng nhập phím, có timeout ****************/
// trả về là giá trị phím người dùng đã nhập 
// hoặc = NO_KEY ('\0') khi TIMEOUT
char Waitforkey(unsigned long tm_out){
  unsigned long start = millis();
  char temp;
  do{
      temp = keypad.getKey();
      if(temp != NO_KEY) return temp;
      delay(40);
  } while(millis() - start < tm_out);
  return NO_KEY;
}

/************  Hàm xử lý giá trị người dùng nhập từ bàn phím  ************/
/* Trả về : BACK - Khi người dùng nhập giá trị không hợp lệ
            NEXT - Khi người dùng nhấn '#' khi chưa nhập gì
            CANCEL - Khi người dùng để timeout
            ENTERED - Khi người dùng nhập giá trị hợp lệ */
uint8_t HandleInputValue(CursorPos cursor, uint8_t* pri_val,uint8_t min_val, uint8_t max_val, uint8_t stat){
  uint8_t val[3] = {0};
  //  counter: biến đếm số lần người dùng nhấn từ keypad, 
  //  i: biến index của mảng
  uint8_t counter = 0,i = 0;
  uint8_t temp_val = *pri_val;
  do{
    key = Waitforkey(TIMEOUT);
    
      // Xử lý lúc nhấn xóa
      if (key == '*') {
        lcd.setCursor(cursor.col,cursor.row);
        if (stat == HR_CHG) {
          lcd.print("     ");
          *pri_val = 0;
        }
        else lcd.print("  ");
        lcd.setCursor(cursor.col,cursor.row);
        counter = 0;
        i = 0;
        val[0] = val[1] = val[2] = 0;
      }
      else if( key == '#')  break;
      else if(key == NO_KEY) return CANCEL; // timeout

      // Xử lý khi nhấn các phím số
      else {
        if (counter < 2 ){
          val[i] = (byte)key - 48;
          lcd.setCursor(cursor.col+i,0);
          lcd.print(key);
          counter++;
          i++;
        }
        if(i == 2) i = 0;
      }
  } while(1);
  // NEU KET QUA KIEM TRA LA TRUE
  if(counter > 0 && Joining(val,&counter,min_val,max_val,&temp_val)){
    *pri_val = temp_val;
    //Serial.println("Dia chi cua pri_val" + String(int(pri_val)));
    if(pri_val > mins_atv && int(pri_val - mins_atv) < 4) EEPROM.write(int(pri_val - mins_atv) + 4,*pri_val);
    else if(pri_val > hr_atv && int(pri_val - hr_atv) < 4) EEPROM.write(int(pri_val-hr_atv),*pri_val);
    else if(pri_val == &hm1) EEPROM.write(8,hm1);
    else if(pri_val == &hm2) EEPROM.write(9,hm2);
    else if(pri_val == &thr_temp) EEPROM.write(10,thr_temp);
    else if(pri_val == &mins_opr) EEPROM.write(11,mins_opr);
    else if(pri_val == &secs_opr) EEPROM.write(12,secs_opr);
    return ENTERED; // Nhập thành công
  }
  else if (counter == 0) return NEXT; // trường hợp không nhập j, đã nhấn '#'
  lcd.setCursor(0,1);
  lcd.print("    Illvalid    ");
  delay(700);
  lcd.setCursor(cursor.col,cursor.row);
  lcd.print("  ");
  return BACK; // Nhập lại
}

/************  Hàm hiển thị ngày/tháng/năm tại vị trí cur  ********************/
void DMYDisplay(CursorPos cur){
  lcd.setCursor(cur.col,cur.row);
  lcd.print(FixValue(rtc.getDate()) + "/" + FixValue(rtc.getMonth()) + "/" + String(cent_1) + FixValue(rtc.getYear()));
}

/************  Hàm hiển thị giờ:phút:giây tại vị trí cur  ********************/
void HMSDisplay(CursorPos cur){
  lcd.setCursor(cur.col,cur.row);
  lcd.print(FixValue(rtc.getHour24()) + ":" + FixValue(rtc.getMinute()) + ":" + FixValue(rtc.getSecond()));
}

/***********  Hàm hiển thị giao diện chính  ********************/
void MainIntf_Display(){
  // Hiển thị ngày giờ
  CursorPos cur(0,0);
  DMYDisplay(cur);
  cur.Setpos(0,1);
  HMSDisplay(cur);
  // Hiển thị chế độ đang dùng trên giao diện chính
  lcd.setCursor(11,0);
  lcd.print("M"+String(mode));
  if(in_mode == MODE_TM)
      lcd.print("-Tm");
  else
      lcd.print("-Rh");
  // Đoạn code sau thực hiện 2 công việc: 
  //       Khoảng 1s: cập nhật 1 lần giá trị nhiệt độ hoặc độ ẩm lên lcd
  //       Khoảng 5s: Nhiệt độ và độ ẩm hiển thị luân phiên
  if(millis() - tmtp > 200){
    cnt++;
    tmtp = millis();
  } 
  lcd.setCursor(11,1);
  
  if (cnt % 5 == 0){
    if(cnt < 25){
    lcd.print((String)amt1001_gettemperature(analogRead(TMPpin))+ (String)(char)223 +"C");
    }
    else{
    // uint16_t step = analogRead(A3);
    uint16_t humidity = ReadHumidity(RHpin);
    if(humidity < 10) 
      lcd.print("0" + (String)humidity+ "% ");
    else 
      lcd.print((String)humidity+ "% ");
    }
  }
  if(cnt> 50) cnt = 0;
}

// hàm PHÁT HIỆN trạng thái nhấn và giá trị phím nhấn 
// gồm các trạng thái nhấn đơn,nhấn giữ.
// khi nhấn giữ , giá trị của key được + thêm 33
void KeyState_Check(){
  char temp = keypad.getKey();
  if ((int)keypad.getState() ==  PRESSED) {
    state = 0;
    if (temp != NO_KEY) {
      key = temp;
    }
  }
  if ((int)keypad.getState() ==  HOLD)   state = 33;
  if ((int)keypad.getState() ==  RELEASED) {
    key += state; 
    state = 0;
    if(key == 75) {
      key = 0;
      GetInCsMode();
    }
    // vung dieu khien mode 1
    else if ((key == '#'+33) && mode == MODE1){
      on_atv = true;
      digitalWrite(trigger,HIGH);
      count_down = 60*mins_opr + secs_opr;
      Serial.println(count_down);
      key = 0;
    }
  }
}

String MakeTimeString(uint8_t value1, uint8_t value2 ){
  return FixValue(value1) + ":" +FixValue(value2);
}

//******* Hàm sử dụng cho giao diện hiển thị thời gian hẹn giờ
void PrintTimeLCD(uint8_t n){
  String tmp_str = (String)(n+1) + "-";
  tmp_str += MakeTimeString(hr_atv[n],mins_atv[n]);
  lcd.print(tmp_str);
}

//******* Hàm giao diện điều chỉnh hình thức tưới
uint8_t Parameter_Setting_tt(){
  lcd.clear();
  String temp = "1_ ";
  temp+= MakeTimeString(mins_opr,secs_opr) + "m|s [*]C";
  lcd.print(temp);
  lcd.setCursor(0,1);
  temp = "2_ ";
  temp += (String)hm2+"%";
  lcd.print(temp);
  lcd.setCursor(12,1);
  lcd.print("[#]>");
  if(in_mode == MODE_TM)  lcd.setCursor(1,0);
  else lcd.setCursor(1,1);
  lcd.print(">");
  do{
    key = Waitforkey(TIMEOUT);
    if(key == '1'){
      in_mode = MODE_TM;
      lcd.setCursor(1,1);
      lcd.print(" ");
      lcd.setCursor(1,0);
      lcd.print(">");
    }
    else if (key == '2'){
      in_mode = MODE_HM;
      lcd.setCursor(1,0);
      lcd.print(" ");
      lcd.setCursor(1,1);
      lcd.print(">");
    }
    else if(key == '#') break;
    else if (key == '*') {
      uint8_t res;
      // sửa 
      if(in_mode == MODE_TM){
        // nhập phút:giây
        res = SetOprTime();
        if(res == SAVED) break;
        else return CANCEL;
      }
      else {
        // nhập độ ẩm
        res = Humidity2_Change();
        if(res == SAVED) break;
        else return CANCEL;
      }
    }
    else if (key == NO_KEY) return CANCEL;
  } while(1);
  return SAVED;
}

uint8_t Parameter_Setting(){
  do{
    lcd.clear();
    if (in_mode == MODE_TM) 
    lcd.print("Time: "+ MakeTimeString(mins_opr,secs_opr));
    else 
    lcd.print("RH2: "+(String)hm2+"%");
    lcd.setCursor(0,1);
    lcd.print("<[*] [0]Ent [#]S");
    key = Waitforkey(TIMEOUT);
    if (key == '*') return BACK;
    else if (key == '#') break;
    else if (key == '0') {
      uint8_t res = Parameter_Setting_tt();
      if(res == CANCEL) return CANCEL;
    }
    else if(key == NO_KEY) return CANCEL;
  }while(1);
  return SAVED;
}

//******* Hiển thị và điều chỉnh độ ẩm ngưỡng trên
uint8_t Humidity2_Change(){
  uint8_t res = NEXT;
  lcd.clear();
  lcd.print("    RH2:");
  lcd.setCursor(12,0);
  lcd.print("%");
  do{
    lcd.setCursor(0,1);
    lcd.print("Del[*]   [#]Next");
    CursorPos cur(10,0);
    res = HandleInputValue(cur,&hm2,hm1,99,OTHER_CHG);
    if (res == CANCEL) return CANCEL;
    else if(res == BACK ) {
      lcd.setCursor(0,1);
      lcd.print("REQUIRED RH2>"+ String(hm1));
      delay(1500);
    }  
  } while(res == BACK);
  // luu vao bien
  // Serial.println(hm1);
  return SAVED;
}

uint8_t Humidity2_Display(){
 do{
    lcd.clear();
    lcd.print("    RH2:");
    lcd.setCursor(0,1);
    lcd.print("<[*] [0]Ent [#]S");
    lcd.setCursor(10,0);
    lcd.print((String)hm2 + "%");
    key = Waitforkey(TIMEOUT);
    if(key == '#') break;
    else if(key == '*') return BACK;
    else if (key == '0'){
      uint8_t res = Humidity2_Change();
      if(res == CANCEL) return CANCEL;
    }
    else if (key == NO_KEY) return CANCEL;
  }while(1);
  return SAVED;
}

//******* Hàm mode thủ công
uint8_t DisplayMode1()
{
  uint8_t res = NEXT;
  do{
    lcd.clear();
    lcd.print("M1 - MANUAL");
    lcd.setCursor(0, 1);
    lcd.print("Back[*]  [#]Next");
    key = Waitforkey(TIMEOUT);
    if (key == NO_KEY)
      return CANCEL;
    else if (key == '*')
      return BACK;
    else if (key == '#'){
      res = Parameter_Setting();
      if(res == CANCEL) return CANCEL;
    }
  } while (res == BACK);
  return SAVED;
}

//******* Các hàm mode tự động
uint8_t DisplayMode2(){
  uint8_t res = NEXT;
  do{
    lcd.clear();
    lcd.print("M2 - AUTO");
    lcd.setCursor(0,1);
    lcd.print("Back[*]  [#]Next");
    key = Waitforkey(TIMEOUT);
    if(key == NO_KEY) return CANCEL;
    else if (key == '*') return BACK;
    else if (key == '#') {
      res = Humidity1_Display(0);
      if(res == CANCEL) return CANCEL;
    }
  }while(res == BACK);
  return SAVED;
}

uint8_t Humidity1_Change(){
  uint8_t res = NEXT;
  CursorPos cur(10,0);
  lcd.clear();
  lcd.print("    RH1:");
  lcd.setCursor(12,0);
  lcd.print("%");
  do{
    lcd.setCursor(0,1);
    lcd.print("Del[*]   [#]Next");
    res = HandleInputValue(cur,&hm1,0,hm2,OTHER_CHG);
    if(res == CANCEL) return CANCEL;
  } while(res == BACK);
  return BACK;
}

uint8_t Humidity1_Display(uint8_t pr){
  uint8_t res = NEXT;
  do{
    lcd.clear();
    lcd.print("    RH1:");
    lcd.setCursor(0,1);
    lcd.print("<[*] [0]Ent [#]>");
    lcd.setCursor(10,0);
    lcd.print((String)hm1 + "%");
    key = Waitforkey(TIMEOUT);
    if(key == '#') {
      if(pr == 0) res = Parameter_Setting();
      else res = DailyTime_Setting();
      if(res == CANCEL) return CANCEL;
    }
    else if(key == '*') return BACK;
    else if (key == '0'){
      res = Humidity1_Change();
      if(res == CANCEL) return CANCEL;
    }
    else if (key == NO_KEY) return CANCEL;
  }while(res == BACK);
  return NEXT;
}

// Các hàm mode hẹn giờ
uint8_t SetTime(uint8_t n){
  if(n != 5){
    lcd.clear();
    lcd.print("TIME:");
    if(hr_atv[n] != 0){
      lcd.setCursor(6,0);
      String tmp_str = MakeTimeString(hr_atv[n],mins_atv[n]);
      lcd.print(tmp_str);
    }
    uint8_t res = NEXT;
    CursorPos cur(6,0);
    // nhập giờ
    do{
      lcd.setCursor(0,1);
      lcd.print("Dl[*]       [#]S");
      res = HandleInputValue(cur,hr_atv+n,0,23,HR_CHG);
      if(res == CANCEL) return CANCEL;
    } while(res == BACK);
    
    if(!hr_atv[n]) {
      mins_atv[n] = 0;
      return BACK;
    }
    EEPROM.write(n,hr_atv[n]);
    lcd.setCursor(8,0);
    lcd.print(":");
    cur.col = 9;cur.row = 0;
    // nhập phút
    do{
      lcd.setCursor(0,1);
      lcd.print("Dl[*]       [#]S");
      res = HandleInputValue(cur,mins_atv+n,0,59,HR_CHG);
      if(res == CANCEL) return CANCEL;
    } while(res == BACK);
  }
  else return CANCEL;
  // nhập xong, quay lại giao diện hiển thị thời gian hẹn giờ => return BACK
  return BACK;
}

uint8_t DailyTime_Setting(){
  uint8_t res = NEXT;
  do{
    lcd.clear();
    lcd.setCursor(0,0);
    if (hr_atv[0] == 0) lcd.print("1|NONE");
    else PrintTimeLCD(0);
    lcd.setCursor(9,0);
    if (hr_atv[1] == 0) lcd.print("2|NONE");
    else PrintTimeLCD(1);
    lcd.setCursor(0,1);
    if (hr_atv[2] == 0) lcd.print("3|NONE");
    else PrintTimeLCD(2);
    lcd.setCursor(9,1);
    if (hr_atv[3] == 0) lcd.print("4|NONE");
    else PrintTimeLCD(3);

    key = Waitforkey(25000);
    switch (key)
    {
    case NO_KEY:
      return CANCEL;
    case '#':
      res = Parameter_Setting();
      if(res == CANCEL) return CANCEL;
      break;
    case '*': 
      return BACK;
    case '1':
    case '2':
    case '3':
    case '4':
      res = SetTime((byte)key-49);
      if(res == CANCEL) return CANCEL;
      break;
    default:
      break;
    }
  }while(res == BACK);
  return SAVED;
}

uint8_t DisplayMode3(){
  uint8_t res = NEXT;
  do{
    lcd.clear();
    lcd.print("M3 - TIMER");
    lcd.setCursor(0,1);
    lcd.print("Back[*]  [#]Next");
    key = Waitforkey(TIMEOUT);
    if(key == NO_KEY) return CANCEL;
    else if (key == '*') return BACK;
    else if (key == '#') {
      res = DailyTime_Setting();
      if(res == CANCEL) return CANCEL;
    };
  }while(res == BACK);
  return SAVED;
}

uint8_t SetOprTime(){
  lcd.clear();
  lcd.print("Time: ");
  lcd.setCursor(0,1);
  lcd.print("Dl[*]       [#]S");
  uint8_t res = NEXT;
  CursorPos cur(6,0);
  // Nhập phút
  do{
    res = HandleInputValue(cur,&mins_opr,0,59,OTHER_CHG);
    if(res == CANCEL) return CANCEL;
    if(res == NEXT) return SAVED;
  } while(res == BACK);
  lcd.setCursor(8,0);
  lcd.print(":");
    // nhập giây
  cur.Setpos(9,0);
  do{
    res = HandleInputValue(cur,&secs_opr,0,59,OTHER_CHG);
    if(res == CANCEL) return CANCEL;
  } while(res == BACK);
  return SAVED;
}

// Mode tổng hợp 
uint8_t DisplayMode4(){
  uint8_t res = NEXT;
  do{
    lcd.clear();
    lcd.print("M4 - MIXTURE");
    lcd.setCursor(0,1);
    lcd.print("Back[*]  [#]Next");
    key = Waitforkey(TIMEOUT);
    if(key == NO_KEY) return CANCEL;
    else if (key == '*') return BACK;
    else if (key == '#') {
      res = Humidity1_Display(1);
      if(res == CANCEL) return CANCEL;
    };
  }while(res == BACK);
  return SAVED;
}
// điều chỉnh nhiệt độ ngưỡng 
uint8_t ThrTempurature_Change(){
  uint8_t res = NEXT;
  lcd.clear();
  lcd.print("   Temp:");
  lcd.setCursor(12,0);
  lcd.print((String)(char)223 + "C");
  CursorPos cur(10,0);
  do{
    lcd.setCursor(0,1);
    lcd.print("Del[*]   [#]Save");
    res = HandleInputValue(cur,&thr_temp,0,60,OTHER_CHG);
    if(res == CANCEL) return CANCEL;
  } while( res == BACK);

  // luu vao bien
  // Serial.println(hm1);
  return BACK;
}

uint8_t ThrTempurate_Display(){
  do{
    lcd.clear();
    lcd.print("   Temp:");
    lcd.setCursor(0,1);
    lcd.print("<[*] [0]Ent [#]S");
    lcd.setCursor(10,0);
    lcd.print(FixValue(thr_temp) + (String)(char)223 + "C");
    key = Waitforkey(TIMEOUT);
    if(key == '#') break;
    else if(key == '*') return BACK;
    else if (key == '0'){
      uint8_t res = ThrTempurature_Change();
      if(res == CANCEL) return CANCEL;
    }
    else if (key == NO_KEY) return CANCEL;
  }while(1);
  return SAVED;
}

// Giao diện điều chỉnh ngày tháng năm và giờ cho RTC 
uint8_t RTC_SetDMY(){
  uint8_t res = NEXT;
  uint16_t year = rtc.getYear();
  uint8_t month = rtc.getMonth(),date = rtc.getDate();
  uint8_t val[5] = {0};
  //  counter: biến đếm số lần người dùng nhấn từ keypad, 
  //  i: biến index của mảng
  uint8_t counter = 0,i = 0;
  // -- NHẬP NĂM --- 
  lcd.clear();
  lcd.print("YEAR: ");
  do{
    lcd.setCursor(0,1);
    lcd.print("[*]Del   Next[#]");
    do{
    key = Waitforkey(TIMEOUT);
      // Xử lý lúc nhấn xóa
      if (key == '*') {
        lcd.setCursor(8,0);
        lcd.print("    ");
        lcd.setCursor(8,0);
        counter = 0;
        i = 0;
        val[0] = val[1] = val[2] = val[3] = 0;
      }
      else if( key == '#')  break;
      else if(key == NO_KEY) return CANCEL; // timeout

      // Xử lý khi nhấn các phím số
      else {
        if (counter < 4 ){
          val[i] = (byte)key - 48;
          lcd.setCursor(8+i,0);
          lcd.print(key);
          counter++;
          i++;
        }
        if(i == 4) i = 0;
      }
    } while(1);
  // NEU KET QUA KIEM TRA LA TRUE
    if(counter > 0 && Joining(val,&counter, 2021, 9999, &year)){
      res = ENTERED; // Nhập thành công
      break;
    }
    else if (counter == 0) {
      res = NEXT; // trường hợp không nhập j, đã nhấn '#'
      break;
    }
    lcd.setCursor(0,1);
    lcd.print("    Illvalid    ");
    delay(700);
    lcd.setCursor(8,0);
    lcd.print("    ");
    i = 0;
    counter = 0;
    res = BACK; // Nhập lại
  }while(res == BACK);
  if(res == ENTERED) {
    rtc.SetYear(year % 100);
    cent_1 = year/100;
    EEPROM.write(15,cent_1);
  }
  // -- NHẬP THÁNG -- 
  lcd.clear();
  lcd.print("MONTH: ");
  CursorPos cur(8,0);
  do{
    lcd.setCursor(0,1);
    lcd.print("[*]Del   Next[#]");
    res = HandleInputValue(cur,&month,1,12,OTHER_CHG);
  }while(res == BACK);
  if(res == ENTERED){
    rtc.SetMonth(month);
  }
  else if(res == CANCEL) return CANCEL;
  // -- NHẬP NGÀY -- 
  lcd.clear();
  lcd.print("DATE: ");
  do{
    lcd.setCursor(0,1);
    lcd.print("[*]Del   Next[#]");
    res = HandleInputValue(cur,&date,1,DaysOfMonth(rtc.getMonth(),rtc.getYear()),OTHER_CHG);
  }while(res == BACK);
  if(res == ENTERED){
    rtc.SetDate(date);
  }
  else if(res == CANCEL) return CANCEL;
  return NEXT;
}

uint8_t RTC_SetHMS(){
  uint8_t res = NEXT,
          hour = rtc.getHour24(),
          minute = rtc.getMinute(),
          second = rtc.getSecond();
  // -- NHẬP GIỜ --
  lcd.clear();
  lcd.print("HOUR: ");
  CursorPos cur(8,0);
  do{
    lcd.setCursor(0,1);
    lcd.print("[*]Del   Next[#]");
    res = HandleInputValue(cur,&hour,0,23,OTHER_CHG);
  } while(res == BACK);
  if(res == ENTERED){
    rtc.SetHour24(hour);
  }
  else if(res == CANCEL) return CANCEL;
  // -- NHẬP PHÚT --
  lcd.clear();
  lcd.print("MINUTE: ");
  do{
    lcd.setCursor(0,1);
    lcd.print("[*]Del   Next[#]");
    res = HandleInputValue(cur,&minute,0,59,OTHER_CHG);
  }while(res == BACK);
  if(res == ENTERED){
    rtc.SetMinute(minute);
  }
  else if(res == CANCEL) return CANCEL;

  // -- NHẬP GIÂY -- 
  lcd.clear();
  lcd.print("SECOND: ");
  do{
    lcd.setCursor(0,1);
    lcd.print("[*]Del   Next[#]");
    res = HandleInputValue(cur,&second,0,59,OTHER_CHG);
  }while(res == BACK);
  if(res == ENTERED){
    rtc.SetSecond(second);
  }
  else if(res == CANCEL) return CANCEL;
  return NEXT;
}

uint8_t RTC_TimeDisplay(){
  uint8_t res = NEXT;
  do{
    lcd.clear();
    lcd.print("Set: 1_H:M:S");
    lcd.setCursor(0,1);
    lcd.print("<[*] 2_D/M/Y #]S");
    key = Waitforkey(TIMEOUT);
    switch (key)
    {
    case '1':
      res = RTC_SetHMS();
      if(res == CANCEL) return CANCEL;
      break;
    case '2':
      res = RTC_SetDMY();
      if(res == CANCEL) return CANCEL;
      break;
    case '*': return BACK;
    case '#':
      return SAVED;
      break;
    case NO_KEY: return CANCEL;
    default:
      break;
    }
  } while(1);
  return SAVED;
}

uint8_t RTC_SettingDisplay(){
  uint8_t res = NEXT;
  do {
    lcd.clear();
    lcd.print("RTC Setting");
    lcd.setCursor(0, 1);
    lcd.print("Back[*]  [#]Next");
    key = Waitforkey(TIMEOUT);
    if (key == NO_KEY)
      return CANCEL;
    else if (key == '*')
      return BACK;
    else if (key == '#'){
      res = RTC_TimeDisplay();
      if(res == SAVED) break;
    }
  } while (res == BACK);
  return SAVED;
}

// thông báo đã lựa chọn mode
void NotifyMode(){
  lcd.clear();
  switch (mode)
  {
  case 1:
    lcd.print("Che do thu cong");
    break;
  case 2:
    lcd.print("Che do tu dong");
    break;
  case 3:
    lcd.print("Che do hen gio");
    break;
  case 4:
    lcd.print("Che do tu dong");
    break;
  default:
    break;
  }
  lcd.setCursor(0,1);
  if (in_mode == MODE_TM){
    lcd.print("TG tuoi:");
    String tmp_str = MakeTimeString(mins_opr,secs_opr);
    lcd.print(tmp_str + "m");
    count_down = mins_opr*60+secs_opr;
  }
  else {
    lcd.print("Do am ngung: "+ (String)hm2 + "%");
  }
  delay(2000);
}

// Giao diện lựa chọn mode 
uint8_t SelectMode(){
  uint8_t res = BACK; // ket qua viec nhan 1-> 4 
  char str_arr[] = "1-Manual  2-Auto  3-Timer  4-Mix  5-Set_Temp  6-Set_RTC   ";
  char temp;
  uint8_t inc = 0;
  unsigned long start = millis(),jst_print = 0;
  do{
    if(millis() - jst_print > 500){
      lcd.setCursor(0,1);
      lcd.print(SplitString(str_arr,inc));
      inc ++;
      jst_print = millis();
    }
    temp = keypad.getKey();
    if(temp == '1' || temp == '2' || 
      temp == '3' || temp == '4' || 
      temp == '5' || temp == '6' || temp == '*') break;
    if(millis() - start > 30000) {
      temp = NO_KEY;
      break;
    }
    if(inc > 50) inc = 0;
  } while(1);
  key = temp;
  if(key == NO_KEY || key == '*') return CANCEL;
  else {
    Serial.println(key);
    switch (key)  {
    case '1':
      /* MODE 1*/ // MANUAL
      res = DisplayMode1(); 
      if(res == SAVED) {
        count_down = mins_opr*60+secs_opr;
        mode = MODE1;
        EEPROM.write(13,mode);
        EEPROM.write(14,in_mode);
        //NotifyMode();
      }
      break;
    case '2':
      /* MODE 2*/ // AUTO 
      res = DisplayMode2();
      if(res == SAVED) {
        mode = MODE2;
        EEPROM.write(13,mode);
        EEPROM.write(14,in_mode);
         //NotifyMode();
      }
      break;

    case '3':
      /* MODE 3*/ // TIMER
      res = DisplayMode3();
      if(res == SAVED) {
        mode = MODE3;
        EEPROM.write(13,mode);
        EEPROM.write(14,in_mode);
       // NotifyMode();
      }
      break;
    case '4':
      res = DisplayMode4();
      if(res == SAVED){
        mode = MODE4;
        EEPROM.write(13,mode);
        EEPROM.write(14,in_mode);
         //NotifyMode();
      }
      break;
    case '5': // Chỉnh nhiệt độ ngưỡng ( không phải là 1 chế độ hoạt động)
      /* code MODE 4*/
      res = ThrTempurate_Display();
      break;
    case '6': // Chỉnh thời gian cho RTC (không phải là 1 chế độ hoạt động)
      res = RTC_SettingDisplay();
      break;
    default:
      return BACK;
      break;
    }
  }
  return res;
}

// Giao diện đầu tiên chọn mode
void GetInCsMode(){
  uint8_t res = BACK;
  do{
    lcd.clear();
    lcd.print("Select mode");
    res = SelectMode();
    if(res == CANCEL || res == SAVED) {
      lcd.clear();
      break;
    }
    
  } while(res == BACK);
}

// kiem tra dieu kien dừng tưới 
void Stop_opr(){
  if(in_mode == MODE_TM){
    uint8_t a = rtc.getSecond();
   // thời điểm ban đầu trc khi đếm ngược, kích đèn sáng
    // khi có sự thay đổi về giây tại mỗi lần xét
    if(a !=  present_secs){
      present_secs = a;
      // đếm ngược
      count_down --;
      //Serial.println(count_down);
    }
    if(count_down < 0){
      digitalWrite(trigger,LOW);
      // trả lại giá trị count down ban đầu
      // tắt cờ kích máy bơm
      on_atv = false;
      count_down = mins_opr*60 + secs_opr;
      present_secs = 50;
    } 
  }
  else{
    if(ReadHumidity(RHpin) > hm2) {
      digitalWrite(trigger,LOW);
      on_atv = false;
    }
  }
  if(keypad.getKey() == '#') {
    on_atv = false;
    digitalWrite(trigger,LOW);
    count_down = 60*mins_opr + secs_opr;
  }
}

bool Chk_time(){
  if(rtc.getSecond() < 1 && amt1001_gettemperature(analogRead(TMPpin)) < thr_temp ) {
    for(uint8_t i = 0; i< 4 ; i++){
      if(hr_atv[i] == 0) continue;
      if(rtc.getHour24() == hr_atv[i] && rtc.getMinute() == mins_atv[i]){
          return true;
      }
    }
  }
  return false;
}

bool Chk_moist(){
  if(ReadHumidity(RHpin) < hm1 && amt1001_gettemperature(analogRead(TMPpin)) < thr_temp) 
    return true;
  return false;
}

// kiểm tra điều kiện bắt đầu tưới 
void Start_opr(){
  switch (mode){
    case MODE3:
      if(Chk_time() == true && ReadHumidity(RHpin) < hm2) {
        on_atv = true;
        digitalWrite(trigger,HIGH);
      }
      break;
    case MODE2:
      if(Chk_moist() == true) {
        on_atv = true;
        digitalWrite(trigger,HIGH);
      }
      break;
    case MODE4:
      if((Chk_time() == true &&  ReadHumidity(RHpin) < hm2) || Chk_moist() == true) {
        on_atv = true;
        digitalWrite(trigger,HIGH);
      }
      break;
  }
}

// hàm điều khiển máy bơm
void Operation(){
  if(!on_atv){
    Start_opr();
  }
  else{
    Stop_opr();
  }
}