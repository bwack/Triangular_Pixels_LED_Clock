#ifndef NTP_H
#define NTP_H

/*
 **
 **  NTP
 **
 */

static const uint8_t monthDays[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30,
		31 };
#define LEAP_YEAR(Y) ( ((1970+Y)>0) && !((1970+Y)%4) && ( ((1970+Y)%100) || !((1970+Y)%400) ) )

//WiFiClient client;

struct strDateTime {
	byte hour;
	byte minute;
	byte second;
	int year;
	byte month;
	byte day;
	byte wday;
};

strDateTime DateAndTime; // Global DateAndTime structure, will be refreshed every Second
const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];

void getNTPtime() {
	unsigned long _unixTime = 0;

	if (WiFi.status() == WL_CONNECTED) {
		UDPNTPClient.begin(2390);  // Port for NTP receive
		IPAddress timeServerIP;
		WiFi.hostByName(config.ntpServerName.c_str(), timeServerIP);

		//Serial.println("sending NTP packet...");
		memset(packetBuffer, 0, NTP_PACKET_SIZE);
		packetBuffer[0] = 0b11100011;   // LI, Version, Mode
		packetBuffer[1] = 0;     // Stratum, or type of clock
		packetBuffer[2] = 6;     // Polling Interval
		packetBuffer[3] = 0xEC;  // Peer Clock Precision
		packetBuffer[12] = 49;
		packetBuffer[13] = 0x4E;
		packetBuffer[14] = 49;
		packetBuffer[15] = 52;
		UDPNTPClient.beginPacket(timeServerIP, 123);
		UDPNTPClient.write(packetBuffer, NTP_PACKET_SIZE);
		UDPNTPClient.endPacket();

		delay(100);

		int cb = UDPNTPClient.parsePacket();
		if (cb == 0) {
			Serial.println("No NTP packet yet");
			_unixTime = myrtc.now().unixtime();
			Serial.print("unix time fetced from rtc was ");
			Serial.println(_unixTime);
		} else {
			Serial.print("NTP packet received, length=");
			Serial.println(cb);
			config.Update_Time_Via_NTP_Every = 30;
			UDPNTPClient.read(packetBuffer, NTP_PACKET_SIZE); // read the packet into the buffer
			unsigned long highWord = word(packetBuffer[40], packetBuffer[41]);
			unsigned long lowWord = word(packetBuffer[42], packetBuffer[43]);
			unsigned long secsSince1900 = highWord << 16 | lowWord;
			const unsigned long seventyYears = 2208988800UL;
			_unixTime = secsSince1900 - seventyYears;
			myDS.setEpoch(_unixTime);
			Serial.print("unix time was ");
			Serial.println(_unixTime);
			ntp_response_ok = true;
		}
	} else {
		Serial.println("Internet not yet connected");
		_unixTime = myrtc.now().unixtime();
		delay(500);
	}
	yield();
	if (_unixTime > 0)
		UnixTimestamp = _unixTime; // store universally available time stamp
}

strDateTime ConvertUnixTimeStamp(unsigned long _tempTimeStamp) {
	strDateTime _tempDateTime;
	uint8_t year;
	uint8_t month, monthLength;
	uint32_t time;
	unsigned long days;

	time = (uint32_t) _tempTimeStamp;
	_tempDateTime.second = time % 60;
	time /= 60; // now it is minutes
	_tempDateTime.minute = time % 60;
	time /= 60; // now it is hours
	_tempDateTime.hour = time % 24;
	time /= 24; // now it is days
	_tempDateTime.wday = ((time + 4) % 7) + 1;  // Sunday is day 1

	year = 0;
	days = 0;
	while ((unsigned) (days += (LEAP_YEAR(year) ? 366 : 365)) <= time) {
		year++;
	}
	_tempDateTime.year = year; // year is offset from 1970

	days -= LEAP_YEAR(year) ? 366 : 365;
	time -= days; // now it is days in this year, starting at 0

	days = 0;
	month = 0;
	monthLength = 0;
	for (month = 0; month < 12; month++) {
		if (month == 1) { // february
			if (LEAP_YEAR(year)) {
				monthLength = 29;
			} else {
				monthLength = 28;
			}
		} else {
			monthLength = monthDays[month];
		}

		if (time >= monthLength) {
			time -= monthLength;
		} else {
			break;
		}
	}
	_tempDateTime.month = month + 1;  // jan is month 1
	_tempDateTime.day = time + 1;     // day of month
	_tempDateTime.year += 1970;

	return _tempDateTime;
}

//
// Summertime calculates the daylight saving time for middle Europe. Input: Unixtime in UTC
//
boolean summerTime(unsigned long _timeStamp) {
	strDateTime _tempDateTime = ConvertUnixTimeStamp(_timeStamp);
	// printTime("Innerhalb ", _tempDateTime);

	if (_tempDateTime.month < 3 || _tempDateTime.month > 10)
		return false; // keine Sommerzeit in Jan, Feb, Nov, Dez
	if (_tempDateTime.month > 3 && _tempDateTime.month < 10)
		return true; // Sommerzeit in Apr, Mai, Jun, Jul, Aug, Sep
	if (_tempDateTime.month == 3
			&& (_tempDateTime.hour + 24 * _tempDateTime.day)
					>= (3 + 24 * (31 - (5 * _tempDateTime.year / 4 + 4) % 7))
			|| _tempDateTime.month == 10
					&& (_tempDateTime.hour + 24 * _tempDateTime.day)
					< (3 + 24 * (31 - (5 * _tempDateTime.year/ 4 + 1) % 7)))
		return true;
	else
		return false;
}

unsigned long adjustTimeZone(unsigned long _timeStamp, int _timeZone,
		bool _isDayLightSavingSaving) {
	strDateTime _tempDateTime;
	_timeStamp += _timeZone * 360; // adjust timezone
	// printTime("Innerhalb adjustTimeZone ", ConvertUnixTimeStamp(_timeStamp));
	if (_isDayLightSavingSaving && summerTime(_timeStamp))
		_timeStamp += 3600; // Sommerzeit beachten
	return _timeStamp;
}

bool isLeapYear(int yr) {
	if (yr % 4 == 0 && yr % 100 != 0 || yr % 400 == 0)
		return true;
	else
		return false;
}

byte daysInMonth(int yr, int m) {
	byte days[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
	if (m == 2 && isLeapYear(yr))
		return 29;
	else
		return days[m - 1];
}

long ConvertDate(int year, byte month, byte day, byte hour, byte minute,
		byte second) {
	long epoch = 0;
	for (int yr = 1970; yr < year; yr++)
		if (isLeapYear(yr))
			epoch += 366 * 86400L;
		else
			epoch += 365 * 86400L;
	for (int m = 1; m < month; m++)
		epoch += daysInMonth(year, m) * 86400L;
	epoch += (day - 1) * 86400L;
	epoch += hour * 3600L;
	epoch += minute * 60;
	epoch += second;
	Serial.print("POSIX time:");
	Serial.println(epoch);
	return epoch;
}

int DayOfTheWeek(int year, int month, int day) {
	if (month <= 2) {
		--year;
		month += 12;
	}
	return ((((day + (((month + 1) * 26) / 10) + year + (year / 4)
			+ (6 * (year / 100)) + (year / 400)) % 7) + 6) % 7);
}

void ISRsecondTick() {
	strDateTime _tempDateTime;
	//AdminTimeOutCounter++;
	cNTP_Update++;
	UnixTimestamp++;
	absoluteActualTime = adjustTimeZone(UnixTimestamp, config.timeZone,
			config.isDayLightSaving);
	DateAndTime = ConvertUnixTimeStamp(absoluteActualTime); //  convert to DateAndTime format
	if (millis() - customWatchdog > 30000) {
		Serial.println("CustomWatchdog bites. Bye");
		ESP.reset();
	}
}

#endif
