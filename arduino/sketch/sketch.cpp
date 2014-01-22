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
	COUNTER,
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

void log(const char* str);
void log_callback(File f, void* payload);

void read_holding_callback(frame_t frame);
void write_holding_callback(frame_t frame);

void blink(int count, int time);

void setup() {

	Serial.begin(9600, SERIAL_8N2);

	modbus_state = modbus_configure(&Serial, 9600, 0x3, 0x0, 0, 0, HR_SIZE, 0);

	for (int i = 0; i < HR_SIZE; i++) {
		modbus_state.hr[i] = 0;
	}

	clock = new RTC();

	button = new Button(PUSH);

	pinMode(LIGHT, OUTPUT);

	blink(15, 90);

	datalog = new Log(LOGFILE, SD_SS);

	add_modbus_callback(0x03, read_holding_callback);
	add_modbus_callback(0x10, write_holding_callback);
}

void loop() {
	// read modbus requests
	modbus_update();

	DateTime n = clock->now();

	// input
	digitalWrite(LIGHT, modbus_state.hr[LED]);

	// output
	modbus_state.hr[PHOTORESISTOR] = analogRead(LIGHT);
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
    delay(time/2);
  }
}

void read_holding_callback(frame_t frame) {
	//log("read holding callback");
	modbus_state.hr[COUNTER] += 1;
}

void write_holding_callback(frame_t frame) {
	//log("write holding callback");
	modbus_state.hr[COUNTER] += 10;
}

void log_callback(File f, void* payload) {
	if (f) {
		String str = *((String*)payload);
		f.println(str);
	}
}

void log(const char* str) {
	String s = String(*str);
	datalog->open(log_callback, &s);
}
