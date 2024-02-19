#include <Wire.h>
class DS1307{
    private:
        uint8_t _addr;
    public:
        DS1307(uint8_t address);
        void SetTime(uint8_t date,uint8_t month ,uint8_t year,uint8_t hour ,uint8_t min,uint8_t sec,uint8_t DoW);
        uint8_t getSecond();
        uint8_t getMinute();
        uint8_t getHour24();
        uint8_t getDate();
        uint8_t getMonth();
        uint8_t getYear();
        uint8_t getDayOfWeek();
        void SetSecond(uint8_t sec);
        void SetMinute(uint8_t mins);
        void SetHour24(uint8_t hr);
        void SetDate(uint8_t date);
        void SetMonth(uint8_t mth);
        void SetYear(uint8_t yr);
};