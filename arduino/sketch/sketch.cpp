#include <SoftwareSerial.h>
#include <SimpleModbusSlave.h>
#include <Util.h>
#include <Bounce.h>

#define USE_SOFTWARE_SERIAL	false

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
unsigned int holdingRegs[HOLDING_REGS_SIZE];

void setup() {
	if (USE_SOFTWARE_SERIAL) {
		SoftwareSerial* ss = new SoftwareSerial(BT_RX, BT_TX);
		ss->begin(9600);
		conn = new Connection(ss);
	} else {
		Serial.begin(9600, SERIAL_8N2);
		conn = new Connection(&Serial);
	}
	clock = new RTC();
	but = new Bounce();
	but->attach(PUSH);
	but->interval(5);
	button = new Button(PUSH);
	datalog = new Log(LOGFILE, SD_SS);

	modbus_configure(conn->getConnection(), 9600, 0x3, 0, HOLDING_REGS_SIZE,
			holdingRegs);
}

void dump_log(File f, void* payload) {
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

void log(File f, void* payload) {
	if (f) {
		f.println(*((String*) payload));
	}
}

void write_log(File f, void* payload) {
	Stream* bt = conn->getConnection();
	long start_time = millis();
	boolean end_loop = false;

	if (f) {
		String s = *((String*) payload);
		f.println(s);
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
	String s = String("Iniciando aplicacion...");
	datalog->open(log, &s, FILE_WRITE);
}

void menu() {
	Stream* bt = conn->getConnection();
	String s = String("escritura");

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
			datalog->open(write_log, &s, FILE_WRITE);
			break;
		case 'r':
			datalog->open(dump_log, NULL);
			break;
		default:
			bt->print("Comando desconocido: ");
			bt->println(r);

		};
	}

	if (button->isPressed()) {
		bt->println(clock->prettyPrint());
	}
}

void loop() {
	int error_count = modbus_update();

	//String s = String("Cantidad de errores: ") + error_count;
	//datalog->open(log, &s, FILE_WRITE);

	DateTime n = clock->now();

	holdingRegs[PHOTORESISTOR] = analogRead(LIGHT);
	holdingRegs[DAY] = n.day();
	holdingRegs[MONTH] = n.month();
	holdingRegs[YEAR] = n.year();
	holdingRegs[HOUR] = n.hour();
	holdingRegs[MINUTE] = n.minute();
	holdingRegs[SECOND] = n.second();

	delay(100);
}

