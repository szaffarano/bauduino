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
	LED, HOLDING_REGS_SIZE
};

ModbusSlave* modbus;
ModbusContext* ctx;

void setup() {
	// modbus setup
	Serial.begin(9600, SERIAL_8N2);
	ctx = new ModbusContext();
	ctx->setHoldingRegisters(new ModbusBlock(1));

	modbus = new ModbusSlave(&Serial, ctx, 0x3, 0x0, 9600);

	pinMode(13, OUTPUT);
	digitalWrite(13, HIGH);
}

void loop() {
	modbus->update();

	digitalWrite(13, ctx->getHoldingRegisters()->getData()[LED]);
	delay(100);
}

