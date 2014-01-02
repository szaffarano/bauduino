#include <SoftwareSerial.h>
#include <Util.h>

#define BLUETOOTH_ENABLED	true

#define SD_SS				10
#define	BT_RX				8
#define	BT_TX				9

#define	RTC_SDA 			A4
#define	RTC_SCL 			A5

#define	PUSH				6

#define	LIGHT				0

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

void dump_log(File f) {
	Stream* bt = conn->getConnection();
	bt->println("leyendo archivo...");
	if (f) {
		while (f.available()) {
			char r;
			f.read(&r, 1);
			bt->print(r);
		}
		bt->println();
	} else {
		bt->println("Archivo inexistente.");
	}
}

void write_log(File f) {
	Stream* bt = conn->getConnection();
	long start_time = millis();
	boolean end_loop = false;

	if (f) {
		bt->print("Escriba el contenido, finaliza con '.': ");
		while (!end_loop) {
			if (bt->available()) {
				start_time = millis();
				char r = bt->read();
				if (r != '.') {
					if (r == 10 || r == 13) {
						bt->print("\n");
						f.print("\n");
					} else {
						bt->print(r);
						f.print(r);
					}
				} else {
					bt->println(". finalizando escritura...");
					end_loop = true;
				}
			}
			// 5 segundos de tolerancia para escribir.
			end_loop = millis() - start_time > 5 * 1000;
		}
		bt->println("Escritura OK!");
	} else {
		bt->println("Error en la apertura del archivo.");
	}
}

void loop() {
	Stream* bt = conn->getConnection();

	if (bt->available()) {
		char r = bt->read();
		switch (r) {
		case 'd':
			datalog->remove();
			bt->println("Archivo borrado OK");
			break;
		case 't':
			bt->println(clock->prettyPrint());
			break;
		case 'l':
			bt->print("Intensidad lumínica: ");
			bt->println(analogRead(LIGHT));
			break;
		case 'w':
			datalog->open(write_log, FILE_WRITE);
			break;
		case 'r':
			datalog->open(dump_log);
			break;
		default:
			bt->print("Comando desconocido: ");
			bt->println(r);

		};
	}

	if (button->isPressed()) {
		bt->println(clock->prettyPrint());
	}

	delay(100);
}

