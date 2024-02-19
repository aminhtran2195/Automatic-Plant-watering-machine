#include <Mylib.h>


CursorPos::CursorPos(uint8_t c, uint8_t r){
    this->col = c;
    this->row = r;
}

void CursorPos::Setpos(uint8_t c, uint8_t r){
    this->col = c;
    this->row = r;
}

bool ScanValue(uint16_t val, uint16_t minx, uint16_t maxx){
    if(val >= minx && val <= maxx) return true;
    return false;
}

uint8_t Joining(uint8_t val[] ,uint8_t* counter, uint8_t dft){
    if (*counter == 1) {
        return val[0];
    }
    else{
        return 10*val[0] + val[1];
    }
    return dft;
}


/* return:  True khi ket hop gia tri va kiem tra hop le thanh cong,
            False khi kiem tra gia tri khong hop le */
bool Joining(uint8_t val[],uint8_t* counter, uint8_t min_val, uint8_t max_val, uint8_t* temp_val){
    //Serial.println("counter = " + String(*counter));
    if(*counter == 1) *temp_val = val[0];
    else *temp_val = 10*val[0]+val[1];
    if(ScanValue(*temp_val,min_val,max_val)) return true;
    counter = 0;
    return false;
}
bool Joining(uint8_t val[],uint8_t* counter, uint16_t min_val, uint16_t max_val, uint16_t* temp_val){
    //Serial.println("counter = " + String(*counter));
    if(*counter == 1) *temp_val = val[0];
    else if (*counter == 2 ) *temp_val = 10*val[0]+val[1];
    else if (*counter == 3) *temp_val = 100*val[0] + 10*val[1] + val[2];
    else *temp_val = 1000*val[0] + 100*val[1] + 10*val[2] + val[3];
    if(ScanValue(*temp_val,min_val,max_val)) return true;
    counter = 0;
    return false;
}

uint8_t DaysOfMonth(uint8_t month,uint8_t year){
    switch (month)
    {
    case 1:
    case 3:
    case 5:
    case 7:
    case 8: 
    case 10:
    case 12:
        return 31;
        break;
    case 2:
        if(year % 4 == 0) return 29;
        else return 28;
        break;
    default:
        break;
    }
    return 30;
}
String SplitString(char * arr,uint8_t inc){
    String tmp_str;
    for(uint8_t i = 0; i < 16; i++){
        if(i+inc > 57 ) tmp_str += arr[i+inc-58];
        else tmp_str += arr[i+inc];
    }
    return tmp_str;
}

uint8_t ReadHumidity(uint8_t pin){
    return map(analogRead(pin),200,1023,99,0);
}

String FixValue(uint8_t val){
    if(val < 10) return "0" + String(val);
    return String(val);
}