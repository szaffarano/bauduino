#include <SimpleModbusSlave.h>
#include <string.h>

#define JOIN(HIGH, LOW) 	((HIGH << 8) | LOW)
#define IS_BROADCAST(adu)	adu.address == 0x0

static modbus_config config;

static void exceptionResponse(ADU adu, unsigned char exception);
static unsigned int calculateCRC(PDU pdu);
static void sendPacket(PDU pdu);
static ADU get_adu();

static modbus_function* search_function(unsigned char id);

static void readHR(ADU frame);
static void writeMR(ADU frame);

// id, handler, callback
modbus_function functions[] = {
		{READ_HOLDING_REGISTERS, readHR, NULL},
		{WRITE_MULTIPLE_REGISTERS, writeMR, NULL},
		{0x0, NULL, NULL}
};


modbus_state modbus_configure(Stream *port, long baud, unsigned char slaveId,
		unsigned char txEnablePin, unsigned int di_size, unsigned int ir_size,
		unsigned int hr_size, unsigned int co_size) {

	config.port = port;
	config.slaveId = slaveId;
	config.discreteInputsSize = di_size;
	config.inputRegistersSize = ir_size;
	config.holdingRegistersSize = hr_size;
	config.coilsSize = co_size;
	config.txEnablePin = txEnablePin;

	pinMode(config.txEnablePin, OUTPUT);
	digitalWrite(config.txEnablePin, LOW);

	if (baud > 19200) {
		config.T1_5 = 750;
		config.T3_5 = 1750;
	} else {
		config.T1_5 = 15000000 / baud; // 1T * 1.5 = T1.5
		config.T3_5 = 35000000 / baud; // 1T * 3.5 = T3.5
	}

	config.state.di = (unsigned int*) malloc(
			sizeof(unsigned int) * config.discreteInputsSize);
	config.state.ir = (unsigned int*) malloc(
			sizeof(unsigned int) * config.inputRegistersSize);
	config.state.hr = (unsigned int*) malloc(
			sizeof(unsigned int) * config.holdingRegistersSize);
	config.state.co = (unsigned int*) malloc(sizeof(unsigned int) * config.coilsSize);

	config.state.errors = 0;

	return config.state;
}

void add_modbus_callback(unsigned char function_id, function_handler handler) {
	modbus_function* found = search_function(function_id);
	if (found != NULL) {
		found->callback = handler;
	}
}

ADU get_adu() {
	ADU adu;

	unsigned char buffer[MAX_BUFFER_SIZE];
	unsigned char overflow = 0;
	unsigned char buffer_size = 0;

	if (config.port->available()) {
		while (config.port->available()) {
			if (overflow) {
				config.port->read();
			} else {
				if (buffer_size == MAX_BUFFER_SIZE) {
					overflow = 1;
				} else {
					buffer[buffer_size++] = config.port->read();
				}
			}
			delayMicroseconds(config.T1_5);
		}
	}

	adu.address = buffer[0];
	adu.pdu.function = buffer[1];
	adu.crc = JOIN(buffer[buffer_size - 2], buffer[buffer_size - 1]);
	memcpy(adu.pdu.data, buffer, MAX_BUFFER_SIZE);
	adu.pdu.length = buffer_size;

	return adu;
}

void modbus_update() {
	ADU adu = get_adu();

	// The minimum request packet is 8 bytes for function 3 & 16
	if (adu.pdu.length > 7 && adu.pdu.length < MAX_BUFFER_SIZE) {
		// if the recieved ID matches the slaveID or broadcasting id (0), continue
		if (adu.address == config.slaveId || IS_BROADCAST(adu)) {
			// if the calculated crc matches the recieved crc continue
			if (calculateCRC(adu.pdu) == adu.crc) {
				modbus_function* found = search_function(adu.pdu.function);
				if (found != NULL) {
					found->handler(adu);
					if (found->callback != NULL) {
						found->callback(adu);
					}
				} else {
					exceptionResponse(adu, ILLEGAL_FUNCTION);
				}
			} else {
				// checksum failed
				config.state.errors++;
			}
		} // incorrect id
	} else if (adu.pdu.length > 0 && adu.pdu.length < 8) {
		config.state.errors++; // corrupted packet
	}
}

void exceptionResponse(ADU adu, unsigned char exception) {
	// each call to exceptionResponse() will increment state.errors
	config.state.errors++;

	PDU pdu;
	pdu.length = 5;

	// don't respond if its a broadcast message
	if (!IS_BROADCAST(adu)) {
		pdu.data[0] = config.slaveId;
		pdu.data[1] = (adu.pdu.function | 0x80); // set MSB bit high, informs the master of an exception
		pdu.data[2] = exception;
		unsigned int crc16 = calculateCRC(pdu); // ID, function|0x80, exception code
		pdu.data[3] = crc16 >> 8;
		pdu.data[4] = crc16 & 0xFF;
		// exception response is always 5 bytes
		// ID, function + 0x80, exception code, 2 bytes crc
		sendPacket(pdu);
	}
}

unsigned int calculateCRC(PDU pdu) {
	unsigned int temp, temp2, flag;
	temp = 0xFFFF;
	for (unsigned char i = 0; i < (pdu.length-2); i++) {
		temp = temp ^ pdu.data[i];
		for (unsigned char j = 1; j <= 8; j++) {
			flag = temp & 0x0001;
			temp >>= 1;
			if (flag)
				temp ^= 0xA001;
		}
	}
	// Reverse byte order.
	temp2 = temp >> 8;
	temp = (temp << 8) | temp2;
	temp &= 0xFFFF;
	// the returned value is already swapped
	// crcLo byte is first & crcHi byte is last
	return temp;
}

void sendPacket(PDU pdu) {
	digitalWrite(config.txEnablePin, HIGH);

	for (unsigned char i = 0; i < pdu.length; i++) {
		config.port->write(pdu.data[i]);
	}
	config.port->flush();

	// allow a frame delay to indicate end of transmission
	delayMicroseconds(config.T3_5);

	digitalWrite(config.txEnablePin, LOW);
}

void readHR(ADU adu) {
	unsigned int startingAddress = JOIN(adu.pdu.data[2], adu.pdu.data[3]);
	unsigned int noOfRegisters = JOIN(adu.pdu.data[4], adu.pdu.data[5]);

	unsigned int maxData = startingAddress + noOfRegisters;
	unsigned char index;
	unsigned char address;
	unsigned int crc16;

	// broadcasting is not supported for function 3
	if (IS_BROADCAST(adu)) {
		exceptionResponse(adu, ILLEGAL_FUNCTION);
		return;
	}

	// check exception 2 ILLEGAL DATA ADDRESS
	if (startingAddress < config.holdingRegistersSize) {
		// check exception 3 ILLEGAL DATA VALUE
		if (maxData <= config.holdingRegistersSize) {
			unsigned char noOfBytes = noOfRegisters * 2;

			// ID, function, noOfBytes, (dataLo + dataHi)*number of registers,
			//  crcLo, crcHi
			PDU pdu;
			pdu.length = 5 + noOfBytes;

			pdu.data[0] = config.slaveId;
			pdu.data[1] = adu.pdu.function;
			pdu.data[2] = noOfBytes;
			address = 3; // PDU starts at the 4th byte
			unsigned int temp;

			for (index = startingAddress; index < maxData; index++) {
				temp = config.state.hr[index];
				pdu.data[address] = temp >> 8; // split the register into 2 bytes
				address++;
				pdu.data[address] = temp & 0xFF;
				address++;
			}

			crc16 = calculateCRC(pdu);
			pdu.data[pdu.length - 2] = crc16 >> 8; // split crc into 2 bytes
			pdu.data[pdu.length - 1] = crc16 & 0xFF;
			sendPacket(pdu);
		} else
			exceptionResponse(adu, ILLEGAL_DATA_VALUE); // exception 3 ILLEGAL DATA VALUE
	} else {
		exceptionResponse(adu, ILLEGAL_DATA_ADDRESS); // exception 2 ILLEGAL DATA ADDRESS
	}
}

void writeMR(ADU adu) {
	unsigned int startingAddress = JOIN(adu.pdu.data[2], adu.pdu.data[3]);
	unsigned int noOfRegisters = JOIN(adu.pdu.data[4], adu.pdu.data[5]);

	unsigned int maxData = startingAddress + noOfRegisters;
	unsigned char index;
	unsigned char address;
	unsigned int crc16;

	// Check if the recieved number of bytes matches the calculated bytes
	// minus the request bytes.
	// id + function + (2 * address bytes) + (2 * no of register bytes) +
	// byte count + (2 * CRC bytes) = 9 bytes
	if (adu.pdu.data[6] == (adu.pdu.length - 9)) {
		// check exception 2 ILLEGAL DATA ADDRESS
		if (startingAddress < config.holdingRegistersSize) {
			// check exception 3 ILLEGAL DATA VALUE
			if (maxData <= config.holdingRegistersSize) {
				// start at the 8th byte in the frame
				address = 7;

				for (index = startingAddress; index < maxData; index++) {
					config.state.hr[index] = JOIN(adu.pdu.data[address],
							adu.pdu.data[address + 1]);
					address += 2;
				}

				// a function 16 response is an echo of the first 6 bytes from
				// the request + 2 crc bytes
				PDU pdu;
				pdu.length = 8;

				pdu.data[0] = adu.pdu.data[0];
				pdu.data[1] = adu.pdu.data[1];
				pdu.data[2] = adu.pdu.data[2];
				pdu.data[3] = adu.pdu.data[3];
				pdu.data[4] = adu.pdu.data[4];
				pdu.data[5] = adu.pdu.data[5];

				// only the first 6 bytes are used for CRC calculation
				crc16 = calculateCRC(pdu);
				pdu.data[6] = crc16 >> 8; // split crc into 2 bytes
				pdu.data[7] = crc16 & 0xFF;

				// don't respond if it's a broadcast message
				if (!IS_BROADCAST(adu)) {
					sendPacket(pdu);
				}
			} else {
				exceptionResponse(adu, ILLEGAL_DATA_VALUE); // exception 3 ILLEGAL DATA VALUE
			}
		} else {
			exceptionResponse(adu, ILLEGAL_DATA_ADDRESS); // exception 2 ILLEGAL DATA ADDRESS
		}
	} else {
		config.state.errors++; // corrupted packet
	}
}

static modbus_function* search_function(unsigned char id) {
	modbus_function* found = NULL;
	for (int i = 0; functions[i].id != 0x0 && found == NULL; i++) {
		if (functions[i].id == id) {
			found = &functions[i];
		}
	}
	return found;
}
