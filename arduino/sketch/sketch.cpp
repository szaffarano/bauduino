#include <SoftwareSerial.h>
#include <SimpleModbusSlave.h>
#include <Arduino.h>
#include <Util.h>

#define	RTC_SDA A4
#define	RTC_SCL A5

#define PHOTO	0
#define	LIGHT	3
#define	PUSH	6
#define	BT_RX	8
#define	BT_TX	9
#define SD_SS	10

#define LOGFILE	"data.log"

enum {
	LED,
	PHOTORESISTOR,
	PUSHBUTTON,
	TRIAC,
	DAY,
	MONTH,
	YEAR,
	HOUR,
	MINUTE,
	SECOND,
	HOLDING_REGS_SIZE
};

unsigned int hr[HOLDING_REGS_SIZE];

RTC* clock;
Button* button;
Log* datalog;

void log(File f, void* payload);

void setup() {
	Serial.begin(9600, SERIAL_8N2);

	modbus_configure(&Serial, 9600, 0x3, 0x0, HOLDING_REGS_SIZE, hr);

	clock = new RTC();

	button = new Button(PUSH);

	datalog = new Log(LOGFILE, SD_SS);

	pinMode(LIGHT, OUTPUT);
	digitalWrite(LIGHT, LOW);
}

void loop() {
	// read modbus requests
	modbus_update();

	DateTime n = clock->now();

	// input
	digitalWrite(LIGHT, hr[LED]);

	// output
	hr[PHOTORESISTOR] = analogRead(LIGHT);
	hr[DAY] = n.day();
	hr[MONTH] = n.month();
	hr[YEAR] = n.year();
	hr[HOUR] = n.hour();
	hr[MINUTE] = n.minute();
	hr[SECOND] = n.second();

	delay(100);
}

void log(File f, void* payload) {
	if (f) {
		f.println(*((String*) payload));
	}
}
