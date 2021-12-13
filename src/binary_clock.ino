#include <RTClib.h>

namespace binary_clock
{
constexpr auto HOURS_GND = 4;
constexpr auto MINUTES_GND = 7;
constexpr auto SECONDS_GND = 8;
constexpr auto PIN_32 = 3;
constexpr auto PIN_16 = 5;
constexpr auto PIN_8 = 6;
constexpr auto PIN_4 = 11;
constexpr auto PIN_2 = 10;
constexpr auto PIN_1 = 9;
constexpr auto DCF77_SIGNAL = 12;

union TimePointValue
{
    uint8_t value;
    struct
    {
        int bit7 : 1;
        int bit6 : 1;
        int bit5 : 1;
        int bit4 : 1;
        int bit3 : 1;
        int bit2 : 1;
        int bit1 : 1;
        int bit0 : 1;
    };
};

struct TimePoint
{
    TimePointValue hour;
    TimePointValue minute;
    TimePointValue second;

    TimePoint()
    {
        hour.value = 0;
        minute.value = 0;
        second.value = 0;
    }
};

using TimePointReader = TimePoint (*)();
using PinWriter = void (*)(uint8_t pin, uint8_t value);

class Clock
{
public:
    Clock(TimePointReader timePointReader, PinWriter writePin);

    /**
     * @brief Call this function every 10 milliseconds.
     */
    void execute();

private:
    TimePointReader m_readTimePoint = nullptr;
    PinWriter m_writePin = nullptr;
};

Clock::Clock(TimePointReader readTimePoint, PinWriter writePin)
    : m_readTimePoint(readTimePoint), m_writePin(writePin)
{
}

void Clock::execute()
{
    if (!m_readTimePoint)
    {
        return;
    }

    const auto timePoint = m_readTimePoint();
    Serial.print(timePoint.hour.value, DEC);
    Serial.print(':');
    Serial.print(timePoint.minute.value, DEC);
    Serial.print(':');
    Serial.print(timePoint.second.value, DEC);
    Serial.print('\n');
}
} // namespace binary_clock

binary_clock::TimePoint readCurrentTimePoint();

RTC_DS3231 rtc;
unsigned int timer = 0;
binary_clock::Clock clock(&readCurrentTimePoint, &digitalWrite);

void setup()
{
    Serial.begin(9600);

    if (!rtc.begin())
    {
        Serial.println("Couldn't find RTC");
        Serial.flush();
        abort();
    }

    Serial.println("Setting the time...");
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));

    TCCR0A = (1 << WGM01); // Set the CTC mode
    OCR0A = 0xF9;          // Value for ORC0A for 1ms

    TIMSK0 |= (1 << OCIE0A); // Set the interrupt request
    sei();                   // Enable interrupt

    TCCR0B |= (1 << CS01); // Set the prescale 1/64 clock
    TCCR0B |= (1 << CS00);

    pinMode(binary_clock::SECONDS_GND, OUTPUT);
    pinMode(binary_clock::MINUTES_GND, OUTPUT);
    pinMode(binary_clock::HOURS_GND, OUTPUT);
    pinMode(binary_clock::PIN_1, OUTPUT);
    pinMode(binary_clock::PIN_2, OUTPUT);
    pinMode(binary_clock::PIN_4, OUTPUT);
    pinMode(binary_clock::PIN_8, OUTPUT);
    pinMode(binary_clock::PIN_16, OUTPUT);
    pinMode(binary_clock::PIN_32, OUTPUT);
    pinMode(binary_clock::DCF77_SIGNAL, INPUT_PULLUP);

    digitalWrite(binary_clock::SECONDS_GND, HIGH);
    digitalWrite(binary_clock::MINUTES_GND, HIGH);
    digitalWrite(binary_clock::HOURS_GND, HIGH);
}

void loop()
{
    if (timer >= 1000)
    {
        timer = 0;
        clock.execute();
    }
}

ISR(TIMER0_COMPA_vect)
{
    timer++;
}

binary_clock::TimePoint readCurrentTimePoint()
{
    const auto now = rtc.now();

    binary_clock::TimePoint result;
    result.hour.value = now.hour();
    result.minute.value = now.minute();
    result.second.value = now.second();

    return result;
}
