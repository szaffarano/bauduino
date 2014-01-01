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

File Log::openForRead() {
	return SD.open(this->name.c_str());
}

File Log::openForWrite() {
	return SD.open(this->name.c_str(), FILE_WRITE);
}
