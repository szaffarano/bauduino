#include <SoftwareSerial.h>
#include <SimpleModbusSlave.h>
#include <Arduino.h>
#include <Util.h>

#define PHOTO		A0

#define	RTC_SDA 	A4
#define	RTC_SCL 	A5

#define	BT_RX		0
#define	BT_TX		1

#define	DHT22		2

#define	LIGHT		3

#define	PUSH		6

#define	TRIAC		7

#define SD_SS		10
#define	SD_MOSI		11
#define	SD_MISO		12
#define	SD_SCK		13


#define	CLIENT_ID	0x3

enum {
	LED,
	PHOTORESISTOR,
	COUNTER1,
	COUNTER2,
	DAY,
	MONTH,
	YEAR,
	HOUR,
	MINUTE,
	SECOND,
	HR_SIZE
};

RTC* clock;
Button* button;
Log* datalog;
modbus_state state;

void read_holding_callback(ADU adu);
void write_holding_callback(ADU adu);

void blink(int count, int time);

void setup() {
	Serial.begin(9600, SERIAL_8N2);

	pinMode(SD_SS, OUTPUT);
	pinMode(LIGHT, OUTPUT);

	blink(7, 90);

	state = modbus_configure(&Serial, 9600, 0x3, 0x0, 0, 0, HR_SIZE, 0);

	for (int i = 0; i < HR_SIZE; i++) {
		state.hr[i] = 0;
	}

	clock = new RTC();

	button = new Button(PUSH);

	datalog = new Log("data.log", SD_SS);

	datalog->log("Iniciando aplicacion...");

	add_modbus_callback(READ_HOLDING_REGISTERS, read_holding_callback);
	add_modbus_callback(WRITE_MULTIPLE_REGISTERS, write_holding_callback);
}

void loop() {
	// read modbus requests
	modbus_update();

	// input
	digitalWrite(LIGHT, state.hr[LED]);

	// output
	state.hr[PHOTORESISTOR] = analogRead(LIGHT);

	DateTime n = clock->now();

	state.hr[DAY] = n.day();
	state.hr[MONTH] = n.month();
	state.hr[YEAR] = n.year();
	state.hr[HOUR] = n.hour();
	state.hr[MINUTE] = n.minute();
	state.hr[SECOND] = n.second();

	delay(100);
}

void blink(int count, int time) {
	for (int i = 0; i < count; i++) {
		digitalWrite(LIGHT, HIGH);
		delay(time);
		digitalWrite(LIGHT, LOW);
		delay(time / 2);
	}
}

void read_holding_callback(ADU adu) {
	String str = clock->prettyPrint();
	str += ": read holding";
	datalog->log(str.c_str());
	state.hr[COUNTER1] += 1;
}

void write_holding_callback(ADU adu) {
	String str = clock->prettyPrint();
	str += ": write holding";
	datalog->log(str.c_str());
	state.hr[COUNTER2] += 1;
}

