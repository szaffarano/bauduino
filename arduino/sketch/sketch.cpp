#include <SoftwareSerial.h>
#include <ModbusSlave.h>
#include <Util.h>
#include <Bounce.h>

#define SD_SS				10
#define	BT_RX				8
#define	BT_TX				9

#define	RTC_SDA 			A4
#define	RTC_SCL 			A5

#define	PUSH				6

#define	LIGHT				0

enum {
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

#define LOGFILE	"data.log"

Connection* conn;
RTC* clock;
Button* button;
Log* datalog;
Bounce* but;
ModbusSlave* modbus;

unsigned int regs[HOLDING_REGS_SIZE];

void setup() {
	// RTC setup
	clock = new RTC();

	// debounce setup
	but = new Bounce();
	but->attach(PUSH);
	but->interval(5);
	button = new Button(PUSH);

	// datalog setup
	datalog = new Log(LOGFILE, SD_SS);

	// modbus setup
	Serial.begin(9600, SERIAL_8N2);
	modbus = new ModbusSlave(&Serial, new ModbusContext(regs, regs, regs, regs),
			0x3, 0x0, 9600);
}

void log(File f, void* payload) {
	if (f) {
		f.println(*((String*) payload));
	}
}

void loop() {
	modbus->update();

	DateTime n = clock->now();

	regs[PHOTORESISTOR] = analogRead(LIGHT);
	regs[DAY] = n.day();
	regs[MONTH] = n.month();
	regs[YEAR] = n.year();
	regs[HOUR] = n.hour();
	regs[MINUTE] = n.minute();
	regs[SECOND] = n.second();

	delay(100);
}

