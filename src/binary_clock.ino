#include <RTClib.h>

namespace binary_clock
{
union TimePointValue
{
    uint8_t value;
    struct
    {
        int bit0 : 1;
        int bit1 : 1;
        int bit2 : 1;
        int bit3 : 1;
        int bit4 : 1;
        int bit5 : 1;
        int bit6 : 1;
        int bit7 : 1;
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
using PinReader = int (*)(uint8_t pin);

class Clock
{
public:
    static constexpr auto HOURS_GND = 4;
    static constexpr auto MINUTES_GND = 7;
    static constexpr auto SECONDS_GND = 8;
    static constexpr auto PIN_32 = 3;
    static constexpr auto PIN_16 = 5;
    static constexpr auto PIN_8 = 6;
    static constexpr auto PIN_4 = 11;
    static constexpr auto PIN_2 = 10;
    static constexpr auto PIN_1 = 9;
    static constexpr auto DCF77_SIGNAL = 12;

public:
    Clock(TimePointReader timePointReader, PinWriter writePin, PinReader readPin);

    /**
     * @brief Call this function every 5 milliseconds.
     */
    void controllLeds();

    /**
     * @brief Call this function every 10 milliseconds.
     */
    void processDcf77Signal();

private:
    enum class SelectedTimeWriter
    {
        Second,
        Minute,
        Hour
    };

    static constexpr auto DFC77_SAMPLES_IN_SECOND = 100;
    static constexpr auto DFC77_ZEROS_IN_LINE_FOR_SYNCHRONIZATION = 15;
    static constexpr auto DFC77_IGNORED_SAMPLES_AFTER_SYNCHRONIZATION = 80;
    static constexpr auto DFC77_NECESSARY_SAMPLES = 100;
    static constexpr auto DFC77_NUMBER_RECEIVED_BITS = 59;

private:
    void printIdleStateOnLeds();
    void printTimeOnLeds();
    void printSecond(const TimePoint& timePoint) const;
    void printMinute(const TimePoint& timePoint) const;
    void printHour(const TimePoint& timePoint) const;
    void printTimePoint(const TimePointValue& value, int groundPin) const;

    void synchronizeDcf77(int sample);
    void readAndInterpretDcf77(int sample);
    void interpretDcf77Second();
    void interpretAndUseDcf77Minute();

private:
    TimePointReader m_readTimePoint = nullptr;
    PinWriter m_writePin = nullptr;
    PinReader m_readPin = nullptr;

    SelectedTimeWriter m_nextTimeWriter = SelectedTimeWriter::Second;
    bool m_realTimeClockCanBeUsed = false;
    uint16_t m_idleStateCounter = 0;

    bool m_dcf77IsSynchronized = false;
    uint8_t m_dcf77IgnoreSamples = 0;
    uint8_t m_dcf77SampleCounter = 0;
    uint8_t m_dcf77BitsCounter = 0;
    uint8_t m_dcf77NumberZerosPerSamplesSecond = 0;
    uint8_t m_dcf77ReceivedBits[DFC77_NUMBER_RECEIVED_BITS] = {0};
};
} // namespace binary_clock

binary_clock::TimePoint readCurrentTimePoint();

RTC_DS3231 rtc;
unsigned int ledTimer = 0;
unsigned int dcf77Timer = 0;
binary_clock::Clock clock(&readCurrentTimePoint, &digitalWrite, digitalRead);

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

    pinMode(binary_clock::Clock::SECONDS_GND, OUTPUT);
    pinMode(binary_clock::Clock::MINUTES_GND, OUTPUT);
    pinMode(binary_clock::Clock::HOURS_GND, OUTPUT);
    pinMode(binary_clock::Clock::PIN_1, OUTPUT);
    pinMode(binary_clock::Clock::PIN_2, OUTPUT);
    pinMode(binary_clock::Clock::PIN_4, OUTPUT);
    pinMode(binary_clock::Clock::PIN_8, OUTPUT);
    pinMode(binary_clock::Clock::PIN_16, OUTPUT);
    pinMode(binary_clock::Clock::PIN_32, OUTPUT);
    pinMode(binary_clock::Clock::DCF77_SIGNAL, INPUT_PULLUP);

    digitalWrite(binary_clock::Clock::SECONDS_GND, HIGH);
    digitalWrite(binary_clock::Clock::MINUTES_GND, HIGH);
    digitalWrite(binary_clock::Clock::HOURS_GND, HIGH);
}

void loop()
{
    if (ledTimer >= 5)
    {
        ledTimer = 0;
        clock.controllLeds();
    }

    if (dcf77Timer >= 10)
    {
        dcf77Timer = 0;
        clock.processDcf77Signal();
    }
}

ISR(TIMER0_COMPA_vect)
{
    ledTimer++;
    dcf77Timer++;
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

namespace binary_clock
{

Clock::Clock(TimePointReader readTimePoint, PinWriter writePin, PinReader readPin)
    : m_readTimePoint(readTimePoint), m_writePin(writePin), m_readPin(readPin)
{
}

void Clock::processDcf77Signal()
{
    if (!m_readPin)
    {
        return;
    }

    const auto dcf77Sample = m_readPin(DCF77_SIGNAL);

    if (!m_dcf77IsSynchronized)
    {
        synchronizeDcf77(dcf77Sample);
    }
    else
    {
        readAndInterpretDcf77(dcf77Sample);
    }
}

void Clock::readAndInterpretDcf77(int sample)
{
    if (m_dcf77IgnoreSamples > 0)
    {
        m_dcf77IgnoreSamples--;
        return;
    }

    if (m_dcf77SampleCounter < DFC77_NECESSARY_SAMPLES)
    {
        Serial.print(sample, DEC);
        m_dcf77NumberZerosPerSamplesSecond += (sample == 0) ? 1 : 0;
    }

    m_dcf77SampleCounter++;

    if (m_dcf77SampleCounter >= DFC77_SAMPLES_IN_SECOND)
    {
        if (m_dcf77NumberZerosPerSamplesSecond == 0)
        {
            interpretAndUseDcf77Minute();
        }
        else
        {
            interpretDcf77Second();
        }

        Serial.print('\n');
        m_dcf77NumberZerosPerSamplesSecond = 0;
        m_dcf77SampleCounter = 0;
    }
}

void Clock::interpretDcf77Second()
{
    if (m_dcf77BitsCounter >= DFC77_NUMBER_RECEIVED_BITS)
    {
        m_dcf77BitsCounter = 0;
    }

    if (m_dcf77NumberZerosPerSamplesSecond > 15)
    {
        m_dcf77ReceivedBits[m_dcf77BitsCounter] = 1;
    }
    else
    {
        m_dcf77ReceivedBits[m_dcf77BitsCounter] = 0;
    }
}

void Clock::interpretAndUseDcf77Minute()
{
    int minute = m_dcf77ReceivedBits[21] + (m_dcf77ReceivedBits[22] << 1) +
                 (m_dcf77ReceivedBits[23] << 2) + (m_dcf77ReceivedBits[24] << 3);
    minute = minute + ((m_dcf77ReceivedBits[25] + (m_dcf77ReceivedBits[26] << 1) +
                        (m_dcf77ReceivedBits[27] << 2)) *
                       10);

    int hour = m_dcf77ReceivedBits[29] + (m_dcf77ReceivedBits[30] << 1) +
               (m_dcf77ReceivedBits[31] << 2) + (m_dcf77ReceivedBits[32] << 3);
    hour = hour + ((m_dcf77ReceivedBits[33] + (m_dcf77ReceivedBits[34] << 1)) * 10);

    Serial.print('\t');
    if (hour < 10)
    {
        Serial.print('0');
    }
    Serial.print(hour, DEC);
    Serial.print(':');
    if (minute < 10)
    {
        Serial.print('0');
    }
    Serial.print(minute, DEC);
    Serial.print('\n');

    m_dcf77BitsCounter = 0;
}

void Clock::synchronizeDcf77(int sample)
{
    if (sample == 0)
    {
        m_dcf77SampleCounter++;
    }
    else
    {
        m_dcf77SampleCounter = 0;
    }

    if (m_dcf77SampleCounter == DFC77_ZEROS_IN_LINE_FOR_SYNCHRONIZATION)
    {
        m_dcf77IsSynchronized = true;
        m_dcf77IgnoreSamples = DFC77_IGNORED_SAMPLES_AFTER_SYNCHRONIZATION;
        m_dcf77SampleCounter = 0;
    }
}

void Clock::controllLeds()
{
    if (!m_readTimePoint || !m_writePin)
    {
        return;
    }

    if (m_realTimeClockCanBeUsed)
    {
        printTimeOnLeds();
    }
    else
    {
        printIdleStateOnLeds();
    }
}

void Clock::printIdleStateOnLeds()
{
    m_writePin(HOURS_GND, 1);
    m_writePin(MINUTES_GND, 1);
    m_writePin(SECONDS_GND, 1);

    if (m_idleStateCounter == 0)
    {
        m_writePin(PIN_1, 0);
        m_writePin(PIN_2, 0);
        m_writePin(PIN_4, 1);
        m_writePin(PIN_8, 1);
        m_writePin(PIN_16, 0);
        m_writePin(PIN_32, 0);
    }
    else if (m_idleStateCounter == 25)
    {
        m_writePin(PIN_1, 0);
        m_writePin(PIN_2, 1);
        m_writePin(PIN_4, 1);
        m_writePin(PIN_8, 1);
        m_writePin(PIN_16, 1);
        m_writePin(PIN_32, 0);
    }
    else if (m_idleStateCounter == 50)
    {
        m_writePin(PIN_1, 0);
        m_writePin(PIN_2, 1);
        m_writePin(PIN_4, 0);
        m_writePin(PIN_8, 0);
        m_writePin(PIN_16, 1);
        m_writePin(PIN_32, 0);
    }
    else if (m_idleStateCounter == 75)
    {
        m_writePin(PIN_1, 1);
        m_writePin(PIN_2, 1);
        m_writePin(PIN_4, 0);
        m_writePin(PIN_8, 0);
        m_writePin(PIN_16, 1);
        m_writePin(PIN_32, 1);
    }
    else if (m_idleStateCounter == 100)
    {
        m_writePin(PIN_1, 1);
        m_writePin(PIN_2, 0);
        m_writePin(PIN_4, 0);
        m_writePin(PIN_8, 0);
        m_writePin(PIN_16, 0);
        m_writePin(PIN_32, 1);
    }
    else if (m_idleStateCounter == 125)
    {
        m_writePin(PIN_1, 0);
        m_writePin(PIN_2, 0);
        m_writePin(PIN_4, 0);
        m_writePin(PIN_8, 0);
        m_writePin(PIN_16, 0);
        m_writePin(PIN_32, 0);
    }
    else if (m_idleStateCounter == 150)
    {
        m_writePin(PIN_1, 1);
        m_writePin(PIN_2, 0);
        m_writePin(PIN_4, 0);
        m_writePin(PIN_8, 0);
        m_writePin(PIN_16, 0);
        m_writePin(PIN_32, 1);
    }
    else if (m_idleStateCounter == 175)
    {
        m_writePin(PIN_1, 1);
        m_writePin(PIN_2, 1);
        m_writePin(PIN_4, 0);
        m_writePin(PIN_8, 0);
        m_writePin(PIN_16, 1);
        m_writePin(PIN_32, 1);
    }
    else if (m_idleStateCounter == 200)
    {
        m_writePin(PIN_1, 0);
        m_writePin(PIN_2, 1);
        m_writePin(PIN_4, 0);
        m_writePin(PIN_8, 0);
        m_writePin(PIN_16, 1);
        m_writePin(PIN_32, 0);
    }
    else if (m_idleStateCounter == 225)
    {
        m_writePin(PIN_1, 0);
        m_writePin(PIN_2, 1);
        m_writePin(PIN_4, 1);
        m_writePin(PIN_8, 1);
        m_writePin(PIN_16, 1);
        m_writePin(PIN_32, 0);
    }
    else if (m_idleStateCounter == 250)
    {
        m_writePin(PIN_1, 0);
        m_writePin(PIN_2, 0);
        m_writePin(PIN_4, 1);
        m_writePin(PIN_8, 1);
        m_writePin(PIN_16, 0);
        m_writePin(PIN_32, 0);
    }
    else if (m_idleStateCounter == 275)
    {
        m_writePin(PIN_1, 0);
        m_writePin(PIN_2, 0);
        m_writePin(PIN_4, 0);
        m_writePin(PIN_8, 0);
        m_writePin(PIN_16, 0);
        m_writePin(PIN_32, 0);
    }
    else if (m_idleStateCounter == 325)
    {
        m_idleStateCounter = -1;
    }

    m_writePin(MINUTES_GND, 0);
    m_idleStateCounter++;
}

void Clock::printTimeOnLeds()
{
    const auto timePoint = m_readTimePoint();

    if (m_nextTimeWriter == SelectedTimeWriter::Second)
    {
        printSecond(timePoint);
        m_nextTimeWriter = SelectedTimeWriter::Minute;
    }
    else if (m_nextTimeWriter == SelectedTimeWriter::Minute)
    {
        printMinute(timePoint);
        m_nextTimeWriter = SelectedTimeWriter::Hour;
    }
    else if (m_nextTimeWriter == SelectedTimeWriter::Hour)
    {
        printHour(timePoint);
        m_nextTimeWriter = SelectedTimeWriter::Second;
    }
}

void Clock::printSecond(const TimePoint& timePoint) const
{
    printTimePoint(timePoint.second, SECONDS_GND);
}

void Clock::printMinute(const TimePoint& timePoint) const
{
    printTimePoint(timePoint.minute, MINUTES_GND);
}

void Clock::printHour(const TimePoint& timePoint) const
{
    printTimePoint(timePoint.hour, HOURS_GND);
}

void Clock::printTimePoint(const TimePointValue& value, int groundPin) const
{
    m_writePin(HOURS_GND, 1);
    m_writePin(MINUTES_GND, 1);
    m_writePin(SECONDS_GND, 1);

    m_writePin(PIN_1, value.bit0);
    m_writePin(PIN_2, value.bit1);
    m_writePin(PIN_4, value.bit2);
    m_writePin(PIN_8, value.bit3);
    m_writePin(PIN_16, value.bit4);
    m_writePin(PIN_32, value.bit5);
    m_writePin(groundPin, 0);
}
} // namespace binary_clock
