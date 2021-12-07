#include <RTClib.h>

RTC_DS3231 rtc;
unsigned int timer = 0;

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
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

    TCCR0A = (1 << WGM01); // Set the CTC mode
    OCR0A = 0xF9;          // Value for ORC0A for 1ms

    TIMSK0 |= (1 << OCIE0A); // Set the interrupt request
    sei();                   // Enable interrupt

    TCCR0B |= (1 << CS01); // Set the prescale 1/64 clock
    TCCR0B |= (1 << CS00);
}

void loop()
{
    if (timer >= 1000)
    {
        timer = 0;
        const auto currentTime = rtc.now();
        const auto hour = currentTime.hour();
        const auto minute = currentTime.minute();
        const auto second = currentTime.second();

        if (hour < 10)
        {
            Serial.print('0');
        }
        Serial.print(hour, DEC);
        Serial.print(":");

        if (minute < 10)
        {
            Serial.print('0');
        }
        Serial.print(minute, DEC);
        Serial.print(":");

        if (second < 10)
        {
            Serial.print('0');
        }
        Serial.print(second, DEC);
        Serial.print('\t');
        Serial.print(timer, DEC);
        Serial.print('\n');
    }
}

ISR(TIMER0_COMPA_vect)
{
    timer++;
}