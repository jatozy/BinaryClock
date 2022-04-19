#include <WiFi.h>
#include <arduino-timer.h>
#include <time.h>

#include "wifi_credentials.hpp"

#ifndef WIFI_SSID
#define WIFI_SSID "ssid"
#define WIFI_PASSWORD "password"
#endif

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

using PinWriter = void (*)(uint8_t pin, uint8_t value);

class Clock
{
public:
    static constexpr auto NTP_SERVER = "1.de.pool.ntp.org";
    // choose your time zone from this list
    // https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
    static constexpr auto TIME_ZONE = "CET-1CEST,M3.5.0,M10.5.0/3";

    static constexpr auto LED_UPDATE_INTERVAL_MS = 1000;
    static constexpr auto TIMEPOINT_UPDATE_INTERVAL_MS = 10000;

    static constexpr auto HOURS_GND = 18;
    static constexpr auto MINUTES_GND = 19;
    static constexpr auto SECONDS_GND = 21;
    static constexpr auto PIN_32 = 23;
    static constexpr auto PIN_16 = 34;
    static constexpr auto PIN_8 = 32;
    static constexpr auto PIN_4 = 33;
    static constexpr auto PIN_2 = 26;
    static constexpr auto PIN_1 = 14;

public:
    Clock(PinWriter writePin);

    /**
     * @brief Call this function every 5 milliseconds.
     */
    void controllLeds();

    void updateDisplayedTime(const TimePoint& displayedTime);

private:
    enum class SelectedTimeWriter
    {
        Second,
        Minute,
        Hour
    };

private:
    void printIdleStateOnLeds();
    void printTimeOnLeds();
    void printSecond(const TimePoint& timePoint) const;
    void printMinute(const TimePoint& timePoint) const;
    void printHour(const TimePoint& timePoint) const;
    void printTimePoint(const TimePointValue& value, int groundPin) const;

    void readAndInterpretDcf77(int sample);
    void prepareDcf77VariablesForNextSecond();

private:
    PinWriter m_writePin = nullptr;

    TimePoint m_displayedTime;

    SelectedTimeWriter m_nextTimeWriter = SelectedTimeWriter::Second;
    bool m_realTimeClockCanBeUsed = true;
    TimePointValue m_meanValueOfReceivedZeros = {0};
};
} // namespace binary_clock

binary_clock::TimePoint readCurrentTimePoint();
bool updateLeds(void*);
bool updateDisplayedTime(void*);

bool ledTimer = false;
bool displayedTimeTimer = false;
auto timer = timer_create_default();
binary_clock::Clock binaryClock(&digitalWrite);

void setup()
{
    Serial.begin(115200);
    Serial.println("\nNTP TZ DST - bare minimum");

    configTime(
        0, 0, binary_clock::Clock::NTP_SERVER);      // 0, 0 because we will use TZ in the next line
    setenv("TZ", binary_clock::Clock::TIME_ZONE, 1); // Set environment variable with your time zone
    tzset();

    // start network
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    while (WiFi.status() != WL_CONNECTED)
    {
        delay(200);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected");

    pinMode(binary_clock::Clock::SECONDS_GND, OUTPUT);
    pinMode(binary_clock::Clock::MINUTES_GND, OUTPUT);
    pinMode(binary_clock::Clock::HOURS_GND, OUTPUT);
    pinMode(binary_clock::Clock::PIN_1, OUTPUT);
    pinMode(binary_clock::Clock::PIN_2, OUTPUT);
    pinMode(binary_clock::Clock::PIN_4, OUTPUT);
    pinMode(binary_clock::Clock::PIN_8, OUTPUT);
    pinMode(binary_clock::Clock::PIN_16, OUTPUT);
    pinMode(binary_clock::Clock::PIN_32, OUTPUT);

    digitalWrite(binary_clock::Clock::SECONDS_GND, HIGH);
    digitalWrite(binary_clock::Clock::MINUTES_GND, HIGH);
    digitalWrite(binary_clock::Clock::HOURS_GND, HIGH);

    timer.every(binary_clock::Clock::LED_UPDATE_INTERVAL_MS, updateLeds);
    timer.every(binary_clock::Clock::TIMEPOINT_UPDATE_INTERVAL_MS, updateDisplayedTime);
}

void loop()
{
    timer.tick();

    if (ledTimer)
    {
        binaryClock.controllLeds();
        ledTimer = false;
    }

    if (displayedTimeTimer)
    {
        binaryClock.updateDisplayedTime(readCurrentTimePoint());
        displayedTimeTimer = false;
    }
}

bool updateLeds(void*)
{
    ledTimer = true;
    return true;
}

bool updateDisplayedTime(void*)
{
    displayedTimeTimer = true;
    return true;
}

binary_clock::TimePoint readCurrentTimePoint()
{
    static time_t now;   // this is the epoch
    static tm localTime; // the structure tm holds time information in a more convenient way
    time(&now);          // read the current time
    localtime_r(&now, &localTime); // update the structure tm with the current time

    Serial.print("year:");
    Serial.print(localTime.tm_year + 1900); // years since 1900
    Serial.print("\tmonth:");
    Serial.print(localTime.tm_mon + 1); // January = 0 (!)
    Serial.print("\tday:");
    Serial.print(localTime.tm_mday); // day of month
    Serial.print("\thour:");
    Serial.print(localTime.tm_hour); // hours since midnight 0-23
    Serial.print("\tmin:");
    Serial.print(localTime.tm_min); // minutes after the hour 0-59
    Serial.print("\tsec:");
    Serial.print(localTime.tm_sec); // seconds after the minute 0-61*
    Serial.print("\twday");
    Serial.print(localTime.tm_wday); // days since Sunday 0-6
    if (localTime.tm_isdst == 1)     // Daylight Saving Time flag
        Serial.print("\tDST");
    else
        Serial.print("\tstandard");
    Serial.println();

    binary_clock::TimePoint result;
    result.hour.value = localTime.tm_hour;
    result.minute.value = localTime.tm_min;
    result.second.value = localTime.tm_sec;

    return result;
}

namespace binary_clock
{
Clock::Clock(PinWriter writePin) : m_writePin(writePin)
{
}

void Clock::controllLeds()
{
    if (!m_writePin)
    {
        return;
    }

    printTimeOnLeds();
}

void Clock::updateDisplayedTime(const TimePoint& displayedTime)
{
    m_displayedTime = displayedTime;
}

void Clock::printTimeOnLeds()
{
    if (m_nextTimeWriter == SelectedTimeWriter::Second)
    {
        printSecond(m_displayedTime);
        m_nextTimeWriter = SelectedTimeWriter::Minute;
    }
    else if (m_nextTimeWriter == SelectedTimeWriter::Minute)
    {
        printMinute(m_displayedTime);
        m_nextTimeWriter = SelectedTimeWriter::Hour;
    }
    else if (m_nextTimeWriter == SelectedTimeWriter::Hour)
    {
        printHour(m_displayedTime);
        m_nextTimeWriter = SelectedTimeWriter::Second;
    }
}

void Clock::printSecond(const TimePoint& timePoint) const
{
    Serial.print("Print Second: ");
    Serial.print(timePoint.second.value);
    Serial.print('\n');
    printTimePoint(timePoint.second, SECONDS_GND);
}

void Clock::printMinute(const TimePoint& timePoint) const
{
    Serial.print("Print Minute: ");
    Serial.print(timePoint.minute.value);
    Serial.print('\n');
    printTimePoint(timePoint.minute, MINUTES_GND);
}

void Clock::printHour(const TimePoint& timePoint) const
{
    Serial.print("Print Hour: ");
    Serial.print(timePoint.hour.value);
    Serial.print('\n');
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
