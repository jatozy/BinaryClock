
#include <RTClib.h>

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

RTC_DS3231 rtc;

char daysOfTheWeek[7][12] =
    {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

int timer = 0;
bool printTime = 0;

unsigned int sample = 0;
unsigned int second = 0;
unsigned int synchronisationPoint = 0;
bool synchronized = false;
unsigned int ignoreSamples = 0;
unsigned int zeros = 0;
unsigned int ones = 0;
unsigned int minute = 0;
bool ignoreThisMinute = false;
unsigned int sampleInSecond = 0;
unsigned int bits[59];
size_t bit = 0;
unsigned int DcfMinute = 0;
unsigned int DcfHour = 0;
unsigned int DcfDay = 0;
unsigned int DcfMonth = 0;
unsigned int DcfYear = 0;

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

    pinMode(SECONDS_GND, OUTPUT);
    pinMode(MINUTES_GND, OUTPUT);
    pinMode(HOURS_GND, OUTPUT);
    pinMode(PIN_1, OUTPUT);
    pinMode(PIN_2, OUTPUT);
    pinMode(PIN_4, OUTPUT);
    pinMode(PIN_8, OUTPUT);
    pinMode(PIN_16, OUTPUT);
    pinMode(PIN_32, OUTPUT);
    pinMode(DCF77_SIGNAL, INPUT_PULLUP);

    digitalWrite(SECONDS_GND, LOW);
    digitalWrite(MINUTES_GND, HIGH);
    digitalWrite(HOURS_GND, HIGH);

    for (size_t i = 0; i < 59; i++)
    {
        bits[i] = 0;
    }
}

void loop()
{
    if (timer >= 10)
    {
        printTime = true;
        timer = 0;
        sample++;
    }

    if (printTime)
    {
        printTime = false;
        const auto dcf77Bit = digitalRead(DCF77_SIGNAL);

        if (!synchronized)
        {
            Serial.print(dcf77Bit, DEC);

            if (dcf77Bit == 0)
            {
                Serial.print(' ');
                Serial.print(synchronisationPoint, DEC);
                Serial.print('\n');
                synchronisationPoint = 0;
            }
            else
            {
                synchronisationPoint++;

                if (synchronisationPoint >= 100)
                {
                    second = 0;
                    minute++;
                    Serial.print('\n');
                    Serial.print('\n');
                    Serial.print("Minute: ");
                    Serial.print(minute, DEC);
                    Serial.print('\n');
                    sample = 1;
                    synchronized = true;
                    ignoreSamples = 40;
                    ignoreThisMinute = false;
                }
            }
        }
        else
        {
            if (ignoreSamples > 0)
            {
                ignoreSamples--;
                sample = 0;
                bit = 0;
            }
            else
            {
                if (dcf77Bit == 0)
                {
                    zeros++;
                    ones = 0;

                    sampleInSecond = sample % 100;
                    if ((sampleInSecond <= 40 || 80 <= sampleInSecond))
                    {
                        zeros--;
                        ignoreThisMinute = true;
                    }
                }
                else
                {
                    ones++;
                }

                if (ones >= 100)
                {
                    second = 0;
                    minute++;
                    Serial.print('\n');
                    Serial.print('\n');
                    Serial.print("Bits: ");
                    for (const auto& i : bits)
                    {
                        Serial.print(i, DEC);
                    }
                    Serial.print('\n');
                    DcfMinute = bits[21] + (bits[22] << 1) + (bits[23] << 2) + (bits[24] << 3);
                    DcfMinute = DcfMinute + ((bits[25] + (bits[26] << 1) + (bits[27] << 2)) * 10);
                    DcfHour = bits[29] + (bits[30] << 1) + (bits[31] << 2) + (bits[32] << 3);
                    DcfHour = DcfHour + ((bits[33] + (bits[34] << 1)) * 10);
                    DcfDay = bits[36] + (bits[37] << 1) + (bits[38] << 2) + (bits[39] << 3);
                    DcfDay = DcfDay + ((bits[40] + (bits[41] << 1)) * 10);
                    DcfMonth = bits[45] + (bits[46] << 1) + (bits[47] << 2) + (bits[48] << 3);
                    DcfMonth = DcfMonth + (bits[49] * 10);
                    DcfYear = bits[50] + (bits[51] << 1) + (bits[52] << 2) + (bits[53] << 3);
                    DcfYear =
                        DcfYear +
                        ((bits[54] + (bits[55] << 1) + (bits[56] << 2) + (bits[57] << 3)) * 10);
                    if (DcfHour < 10)
                    {
                        Serial.print('0');
                    }
                    Serial.print(DcfHour, DEC);
                    Serial.print(':');
                    if (DcfMinute < 10)
                    {
                        Serial.print('0');
                    }
                    Serial.print(DcfMinute, DEC);
                    Serial.print(' ');
                    if (DcfYear < 10)
                    {
                        Serial.print('0');
                    }
                    Serial.print(DcfYear, DEC);
                    Serial.print('-');
                    if (DcfMonth < 10)
                    {
                        Serial.print('0');
                    }
                    Serial.print(DcfMonth, DEC);
                    Serial.print('-');
                    if (DcfDay < 10)
                    {
                        Serial.print('0');
                    }
                    Serial.print(DcfDay, DEC);
                    Serial.print('\n');
                    Serial.print("Minute: ");
                    Serial.print(minute, DEC);
                    Serial.print('\n');
                    sample = 1;
                    synchronized = true;
                    ignoreSamples = 40;
                    ones = 0;
                    zeros = 0;
                    ignoreThisMinute = false;
                }
                else
                {
                    Serial.print(dcf77Bit, DEC);

                    if (sample % 10 == 0)
                    {
                        Serial.print(' ');
                    }
                    if (sample % 100 == 0)
                    {
                        bits[bit] = (zeros >= 15) ? 1 : 0;
                        bit++;

                        if (second < 100)
                            Serial.print('0');
                        if (second < 10)
                            Serial.print('0');
                        Serial.print(second, DEC);
                        second++;
                        Serial.print(" zeros: ");
                        if (zeros < 10)
                        {
                            Serial.print('0');
                        }
                        Serial.print(zeros, DEC);
                        Serial.print(" IgnoreMinute: ");
                        Serial.print(ignoreThisMinute);
                        Serial.print('\n');
                        zeros = 0;
                    }
                }
            }
        }
    }
}

ISR(TIMER0_COMPA_vect)
{
    timer++;
}