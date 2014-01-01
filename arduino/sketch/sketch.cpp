#include <SoftwareSerial.h>
#include <Util.h>

#define BLUETOOTH_ENABLED	false

#define SD_SS				10
#define	BT_RX				8
#define	BT_TX				9

#define	RTC_SDA 			A4
#define	RTC_SCL 			A5

#define	PUSH				6

#define	LIGHT				A0

#define LOGFILE	"data.log"

Connection* conn;
RTC* clock;
Button* button;
Log* datalog;

void setup() {
	if (BLUETOOTH_ENABLED) {
		SoftwareSerial* ss = new SoftwareSerial(BT_RX, BT_TX);
		ss->begin(9600);
		conn = new Connection(ss);
	} else {
		Serial.begin(9600);
		conn = new Connection(&Serial);
	}
	clock = new RTC();
	button = new Button(PUSH);
	datalog = new Log(LOGFILE, SD_SS);
}

void loop() {
	Stream* bt = conn->getConnection();

	if (bt->available()) {
		char r = bt->read();
		if (r != 10 && r != 13) {
			if (r == 'r') {
				File f = datalog->openForRead();
				if (f) {
					while (f.available()) {
						bt->write(f.read());
					}
					f.close();
				}
			} else if (r == 't') {
				bt->println(clock->prettyPrint());
			} else if (r == 'w') {
				while (bt->available()) {
					char r = bt->read();
					if (r != '.') {
						File f = datalog->openForWrite();
						if (f) {
							bt->print(r);
							f.write(r);
							f.close();
						}
					}
				}
			} else {
				bt->print("Comando desconocido: ");
				bt->println(r);
			}
		}
	}

	if (button->isPressed()) {
		bt->println(clock->prettyPrint());
	}

	delay(100);
}

