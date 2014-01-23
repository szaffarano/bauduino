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
modbus_state_t modbus_state;

void read_holding_callback(frame_t frame);
void write_holding_callback(frame_t frame);

void blink(int count, int time);

void setup() {
	Serial.begin(9600, SERIAL_8N2);

	pinMode(SD_SS, OUTPUT);
	pinMode(LIGHT, OUTPUT);

	blink(7, 90);

	modbus_state = modbus_configure(&Serial, 9600, 0x3, 0x0, 0, 0, HR_SIZE, 0);

	for (int i = 0; i < HR_SIZE; i++) {
		modbus_state.hr[i] = 0;
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
	digitalWrite(LIGHT, modbus_state.hr[LED]);

	// output
	modbus_state.hr[PHOTORESISTOR] = analogRead(LIGHT);

	DateTime n = clock->now();

	modbus_state.hr[DAY] = n.day();
	modbus_state.hr[MONTH] = n.month();
	modbus_state.hr[YEAR] = n.year();
	modbus_state.hr[HOUR] = n.hour();
	modbus_state.hr[MINUTE] = n.minute();
	modbus_state.hr[SECOND] = n.second();

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

void read_holding_callback(frame_t frame) {
	String str = clock->prettyPrint();
	str += ": read holding";
	datalog->log(str);
	modbus_state.hr[COUNTER1] += 1;
}

void write_holding_callback(frame_t frame) {
	String str = clock->prettyPrint();
	str += ": write holding";
	datalog->log(str);
	modbus_state.hr[COUNTER2] += 1;
}

