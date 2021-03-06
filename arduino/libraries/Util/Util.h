/*
 * Util.h
 *
 *  Created on: 30/12/2013
 *      Author: sebas
 */

#ifndef UTIL_H_
#define UTIL_H_

#include <Arduino.h>
#include <RTClib.h>
#include <Wire.h>
#include <SD.h>

extern HardwareSerial Serial;

/**
 * Basado en la lógica de http://arduino.cc/en/Tutorial/Debounce
 * Pensado para usarse con el pulsador configurado con una resistencia
 * pull up (cuando el botón está presionado el estado es LOW).
 *
 * Se puede utilizar la resistencia pull up interna de arduino.
 */
class Button {
private:
	byte pin;
	byte buttonState;
	byte lastButtonState;
	long lastDebounceTime;
	byte debounceDelay;
public:
	Button(byte pin, byte debounceDelay = 50);
	boolean isPressed();
};

class RTC {
private:
	RTC_DS1307* rtc;
public:
	RTC();
	String prettyPrint();
	DateTime now();
};

class Log {
private:
	const char* name;
	boolean running;
public:
	Log(const char* name, byte pin);
	void log(const char* str);
	void log(String str);
	void open(void (*callback)(File,void*), void* payload, uint8_t mode = FILE_READ);
	void remove();
};

class FileHelper {
private:
	File file;
public:
	FileHelper(File file);
	~FileHelper();
};

#endif /* UTIL_H_ */
