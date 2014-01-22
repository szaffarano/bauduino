#include <SimpleModbusSlave.h>
#include <string.h>

#define JOIN(HIGH, LOW) 	((HIGH << 8) | LOW)
#define IS_BROADCAST(frame)	frame.address == 0x0

static config_t config;
static modbus_state_t state;

static unsigned int errorCount;

// function definitions
static void exceptionResponse(frame_t frame, unsigned char exception);
static unsigned int calculateCRC(unsigned char* data, unsigned char bufferSize);
static void sendPacket(unsigned char* data, unsigned char bufferSize);
static frame_t get_modbus_message();
static void function0x03(frame_t frame);
static void function0x10(frame_t frame);

void modbus_configure(Stream *port, long baud, unsigned char slaveId,
		unsigned char txEnablePin, unsigned int holdingRegsSize,
		unsigned int* regs) {

	config.port = port;
	config.slaveId = slaveId;
	config.holdingRegsSize = holdingRegsSize;
	config.txEnablePin = txEnablePin;

	state.regs = regs;

	pinMode(config.txEnablePin, OUTPUT);
	digitalWrite(config.txEnablePin, LOW);

	if (baud > 19200) {
		config.T1_5 = 750;
		config.T3_5 = 1750;
	} else {
		config.T1_5 = 15000000 / baud; // 1T * 1.5 = T1.5
		config.T3_5 = 35000000 / baud; // 1T * 3.5 = T3.5
	}

	errorCount = 0; // initialize errorCount
}

frame_t get_modbus_message() {
	frame_t frame;

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

	frame.address = buffer[0];
	frame.function = buffer[1];
	frame.crc = JOIN(buffer[buffer_size - 2], buffer[buffer_size - 1]);
	memcpy(frame.data, buffer, MAX_BUFFER_SIZE);
	frame.length = buffer_size;
	frame.startingAddress = JOIN(frame.data[2], frame.data[3]); // combine the starting address bytes
	frame.noOfRegisters = JOIN(frame.data[4], frame.data[5]); // combine the number of register bytes

	return frame;
}

unsigned int modbus_update() {
	frame_t frame = get_modbus_message();

	// The minimum request packet is 8 bytes for function 3 & 16
	if (frame.length > 7 && frame.length < MAX_BUFFER_SIZE) {
		// if the recieved ID matches the slaveID or broadcasting id (0), continue
		if (frame.address == config.slaveId || IS_BROADCAST(frame)) {
			// if the calculated crc matches the recieved crc continue
			if (calculateCRC(frame.data, frame.length - 2) == frame.crc) {
				// broadcasting is not supported for function 3
				if (!IS_BROADCAST(frame) && (frame.function == 0x3)) {
					// modbus function 0x03
					function0x03(frame);
				} else if (frame.function == 0x10) {
					// function 0x10 = 16
					function0x10(frame);
				} else {
					// illegal function
					exceptionResponse(frame, 0x1); // exception 0x1 ILLEGAL FUNCTION
				}
			} else {
				// checksum failed
				errorCount++;
			}
		} // incorrect id
	} else if (frame.length > 0 && frame.length < 8) {
		errorCount++; // corrupted packet
	}
	return errorCount;
}

void exceptionResponse(frame_t frame, unsigned char exception) {
	// each call to exceptionResponse() will increment the errorCount
	errorCount++;

	unsigned char response[5];

	// don't respond if its a broadcast message
	if (!IS_BROADCAST(frame)) {
		response[0] = config.slaveId;
		response[1] = (frame.function | 0x80); // set MSB bit high, informs the master of an exception
		response[2] = exception;
		unsigned int crc16 = calculateCRC(response, 3); // ID, function|0x80, exception code
		response[3] = crc16 >> 8;
		response[4] = crc16 & 0xFF;
		// exception response is always 5 bytes
		// ID, function + 0x80, exception code, 2 bytes crc
		sendPacket(response, 5);
	}
}

unsigned int calculateCRC(unsigned char* data, unsigned char size) {
	unsigned int temp, temp2, flag;
	temp = 0xFFFF;
	for (unsigned char i = 0; i < size; i++) {
		temp = temp ^ data[i];
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

void sendPacket(unsigned char* data, unsigned char bufferSize) {
	digitalWrite(config.txEnablePin, HIGH);

	for (unsigned char i = 0; i < bufferSize; i++) {
		config.port->write(data[i]);
	}
	config.port->flush();

	// allow a frame delay to indicate end of transmission
	delayMicroseconds(config.T3_5);

	digitalWrite(config.txEnablePin, LOW);
}

void function0x03(frame_t frame) {

	unsigned int maxData = frame.startingAddress + frame.noOfRegisters;
	unsigned char index;
	unsigned char address;
	unsigned int crc16;

	// check exception 2 ILLEGAL DATA ADDRESS
	if (frame.startingAddress < config.holdingRegsSize) {
		// check exception 3 ILLEGAL DATA VALUE
		if (maxData <= config.holdingRegsSize) {
			unsigned char noOfBytes = frame.noOfRegisters * 2;

			// ID, function, noOfBytes, (dataLo + dataHi)*number of registers,
			//  crcLo, crcHi
			unsigned char responseFrameSize = 5 + noOfBytes;
			unsigned char response[responseFrameSize];
			response[0] = config.slaveId;
			response[1] = frame.function;
			response[2] = noOfBytes;
			address = 3; // PDU starts at the 4th byte
			unsigned int temp;

			for (index = frame.startingAddress; index < maxData; index++) {
				temp = state.regs[index];
				response[address] = temp >> 8; // split the register into 2 bytes
				address++;
				response[address] = temp & 0xFF;
				address++;
			}

			crc16 = calculateCRC(response, responseFrameSize - 2);
			response[responseFrameSize - 2] = crc16 >> 8; // split crc into 2 bytes
			response[responseFrameSize - 1] = crc16 & 0xFF;
			sendPacket(response, responseFrameSize);
		} else
			exceptionResponse(frame, 3); // exception 3 ILLEGAL DATA VALUE
	} else {
		exceptionResponse(frame, 2); // exception 2 ILLEGAL DATA ADDRESS
	}
}

void function0x10(frame_t frame) {
	unsigned int maxData = frame.startingAddress + frame.noOfRegisters;
	unsigned char index;
	unsigned char address;
	unsigned int crc16;

	// Check if the recieved number of bytes matches the calculated bytes
	// minus the request bytes.
	// id + function + (2 * address bytes) + (2 * no of register bytes) +
	// byte count + (2 * CRC bytes) = 9 bytes
	if (frame.data[6] == (frame.length - 9)) {
		// check exception 2 ILLEGAL DATA ADDRESS
		if (frame.startingAddress < config.holdingRegsSize) {
			// check exception 3 ILLEGAL DATA VALUE
			if (maxData <= config.holdingRegsSize) {
				// start at the 8th byte in the frame
				address = 7;

				for (index = frame.startingAddress; index < maxData; index++) {
					state.regs[index] = JOIN(frame.data[address],
							frame.data[address + 1]);
					address += 2;
				}

				// a function 16 response is an echo of the first 6 bytes from
				// the request + 2 crc bytes
				unsigned char response[7];
				response[0] = frame.data[0];
				response[1] = frame.data[1];
				response[2] = frame.data[2];
				response[3] = frame.data[3];
				response[4] = frame.data[4];
				response[5] = frame.data[5];

				// only the first 6 bytes are used for CRC calculation
				crc16 = calculateCRC(frame.data, 6);
				response[6] = crc16 >> 8; // split crc into 2 bytes
				response[7] = crc16 & 0xFF;

				// don't respond if it's a broadcast message
				if (!IS_BROADCAST(frame)) {
					sendPacket(response, 8);
				}
			} else {
				exceptionResponse(frame, 3); // exception 3 ILLEGAL DATA VALUE
			}
		} else {
			exceptionResponse(frame, 2); // exception 2 ILLEGAL DATA ADDRESS
		}
	} else {
		errorCount++; // corrupted packet
	}
}
