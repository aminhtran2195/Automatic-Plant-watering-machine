#include <Arduino.h>

struct CursorPos
{
    uint8_t col,row;
    CursorPos(uint8_t c, uint8_t r);
    void Setpos(uint8_t c, uint8_t r);
};

// Hàm kiểm tra điều kiện cho biến val
bool ScanValue(uint16_t val, uint16_t minx, uint16_t maxx);

// Hàm tạo số thập phân, sau khi người dùng nhập từng số
uint8_t Joining(uint8_t val[] ,uint8_t* counter, uint8_t dft);
bool Joining(uint8_t val[],uint8_t*counter,uint8_t min_val, uint8_t max_val,uint8_t* temp_val);
bool Joining(uint8_t val[],uint8_t*counter,uint16_t min_val, uint16_t max_val,uint16_t* temp_val);
// tách chuỗi, dành riêng cho hiển thị chạy chuỗi trên lcd
String SplitString(char * arr,uint8_t inc);
uint8_t DaysOfMonth(uint8_t month,uint8_t year);
// đọc giá trị độ ẩm đất
uint8_t ReadHumidity(uint8_t pin);

// Sửa lại số hiển thị ở dạng chuỗi
// Ví dụ: thay vì hiển thị số "3" thì hiển thị "03" 
String FixValue(uint8_t val);