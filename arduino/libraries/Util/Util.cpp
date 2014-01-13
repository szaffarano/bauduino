/*
 * Util.cpp
 *
 *  Created on: 30/12/2013
 *      Author: sebas
 */

#include <Util.h>

Button::Button(byte pin, byte debounceDelay) {
	this->pin = pin;
	this->debounceDelay = debounceDelay;

	this->buttonState = HIGH;
	this->lastButtonState = HIGH;
	this->lastDebounceTime = 0;

	pinMode(pin, INPUT_PULLUP);
}

boolean Button::isPressed() {
	int reading = digitalRead(this->pin);

	if (reading != this->lastButtonState) {
		this->lastDebounceTime = millis();
	}

	if ((millis() - this->lastDebounceTime) > this->debounceDelay) {
		if (reading != this->buttonState) {
			this->buttonState = reading;
		}
	}

	lastButtonState = reading;
	return buttonState == LOW;
}

Connection::Connection(Stream* serial) {
	this->serial = serial;
}

Stream* Connection::getConnection() {
	return this->serial;
}

RTC::RTC() {
	this->rtc = new RTC_DS1307();
	Wire.begin();
	this->rtc->begin();
    if (!rtc->isrunning()) {
            rtc->adjust(DateTime(__DATE__, __TIME__));
    }

}

DateTime RTC::now() {
	return this->rtc->now();
}

String RTC::prettyPrint() {
	String str;
	DateTime now = this->rtc->now();

	if (now.day() < 10)
		str += "0";
	str += now.day();

	str += ".";

	if (now.month() < 10)
		str += "0";
	str += now.month();

	str += ".  ";

	if (now.hour() < 10)
		str += "0";
	str += now.hour();

	str += ":";

	if (now.minute() < 10)
		str += "0";
	str += now.minute();

	str += ":";

	if (now.second() < 10)
		str += "0";
	str += now.second();

	return str;
}

Log::Log(String name, byte pin) {
	this->name = name;
	pinMode(pin, OUTPUT);

	this->running = SD.begin(pin);

}

void Log::open(void (*callback)(File, void*), void* payload, uint8_t mode) {
	File f = SD.open(this->name.c_str(), mode);
	FileHelper helper(f); // me aseguro que cierran el archivo en el destructor.
	callback(f, payload);
}

void Log::remove() {
	SD.remove((char*) this->name.c_str());
}

FileHelper::FileHelper(File file) {
	this->file = file;
}

FileHelper::~FileHelper() {
	this->file.close();
}
