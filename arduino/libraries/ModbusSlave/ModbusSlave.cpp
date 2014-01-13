/*
 * ModbusSlave.cpp
 *
 *  Created on: 12/01/2014
 *      Author: sebas
 */

/* ModbusSlave implementation */

#include <ModbusSlave.h>

ModbusSlave::ModbusSlave(Stream *port, ModbusContext* ctx, unsigned char id,
		unsigned char txEnablePin, long baud) {

	this->port = port;
	this->ctx = ctx;
	this->id = id;
	this->txEnablePin = txEnablePin;

	pinMode(this->txEnablePin, OUTPUT);
	digitalWrite(this->txEnablePin, LOW);

	if (baud > 19200) {
		this->interCharTimeout = 750;
		this->frameDalay = 1750;
	} else {
		this->interCharTimeout = 15000000 / baud; // 1T * 1.5 = T1.5
		this->frameDalay = 35000000 / baud; // 1T * 3.5 = T3.5
	}
}

void ModbusSlave::update() {

	Frame frame = getFrame();

	// The minimum request packet is 8 bytes for function 3 & 16
	if (frame.getLength() > 7 && frame.getLength() < MAX_BUFFER_SIZE) {
		// if the recieved ID matches the slaveID or broadcasting id (0), continue
		if (frame.getAddress() == this->id || isBroadcast(frame)) {
			// if the calculated crc matches the recieved crc continue
			if (crc(frame.getData(), frame.getLength() - 2) == frame.getCrc()) {
				// broadcasting is not supported for function 3
				if (!isBroadcast(frame) && (frame.getFunction() == 0x3)) {
					// modbus function 0x03
					function0x03(frame);
				} else if (frame.getFunction() == 0x10) {
					// function 0x10 = 16
					function0x10(frame);
				} else {
					// illegal function
					exceptionResponse(frame, 0x1); // exception 0x1 ILLEGAL FUNCTION
				}
			} else {
				// checksum failed
			}
		} // incorrect id
	} else if (frame.getLength() > 0 && frame.getLength() < 8) {
		// corrupted packet
	}

}

Frame ModbusSlave::getFrame() {
	unsigned char buffer[MAX_BUFFER_SIZE];
	unsigned char overflow = 0;
	unsigned char bufferSize = 0;

	if (port->available()) {
		while (port->available()) {
			if (overflow) {
				port->read();
			} else {
				if (bufferSize == MAX_BUFFER_SIZE) {
					overflow = 1;
				} else {
					buffer[bufferSize++] = port->read();
				}
			}
			delayMicroseconds(interCharTimeout);
		}
	}

	return Frame(buffer, bufferSize);
}

boolean ModbusSlave::isBroadcast(Frame frame) {
	return frame.getAddress() == 0x0;
}

unsigned int ModbusSlave::crc(unsigned char* buffer, unsigned char bufferSize) {
	unsigned int temp, temp2, flag;
	temp = 0xFFFF;
	for (unsigned char i = 0; i < bufferSize; i++) {
		temp = temp ^ buffer[i];
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

void ModbusSlave::exceptionResponse(Frame frame, unsigned char exception) {
	unsigned char response[5];

	// don't respond if its a broadcast message
	if (!isBroadcast(frame)) {
		response[0] = this->id;
		response[1] = (frame.getFunction() | 0x80); // set MSB bit high, informs the master of an exception
		response[2] = exception;
		unsigned int crc16 = crc(response, 3); // ID, function|0x80, exception code
		response[3] = crc16 >> 8;
		response[4] = crc16 & 0xFF;
		// exception response is always 5 bytes
		// ID, function + 0x80, exception code, 2 bytes crc
		sendPacket(response, 5);
	}

}

void ModbusSlave::sendPacket(unsigned char* data, unsigned char bufferSize) {

	digitalWrite(txEnablePin, HIGH);

	for (unsigned char i = 0; i < bufferSize; i++) {
		port->write(data[i]);
	}
	port->flush();

	// allow a frame delay to indicate end of transmission
	delayMicroseconds(frameDalay);

	digitalWrite(txEnablePin, LOW);
}

void ModbusSlave::function0x03(Frame frame) {

	unsigned int maxData = frame.getStartingAddress()
			+ frame.getNoOfRegisters();
	unsigned char index;
	unsigned char address;
	unsigned int crc16;

	// check exception 2 ILLEGAL DATA ADDRESS
	if (frame.getStartingAddress() < MAX_BUFFER_SIZE) {
		// check exception 3 ILLEGAL DATA VALUE
		if (maxData <= MAX_BUFFER_SIZE) {
			unsigned char noOfBytes = frame.getNoOfRegisters() * 2;

			// ID, function, noOfBytes, (dataLo + dataHi)*number of registers,
			//  crcLo, crcHi
			unsigned char responseFrameSize = 5 + noOfBytes;
			unsigned char response[responseFrameSize];
			response[0] = id;
			response[1] = frame.getFunction();
			response[2] = noOfBytes;
			address = 3; // PDU starts at the 4th byte
			unsigned int temp;

			for (index = frame.getStartingAddress(); index < maxData; index++) {
				temp = ctx->getHoldingRegisters()[index];
				response[address] = temp >> 8; // split the register into 2 bytes
				address++;
				response[address] = temp & 0xFF;
				address++;
			}

			crc16 = crc(response, responseFrameSize - 2);
			response[responseFrameSize - 2] = crc16 >> 8; // split crc into 2 bytes
			response[responseFrameSize - 1] = crc16 & 0xFF;
			sendPacket(response, responseFrameSize);
		} else
			exceptionResponse(frame, 3); // exception 3 ILLEGAL DATA VALUE
	} else {
		exceptionResponse(frame, 2); // exception 2 ILLEGAL DATA ADDRESS
	}
}

void ModbusSlave::function0x10(Frame frame) {
	unsigned int maxData = frame.getStartingAddress()
			+ frame.getNoOfRegisters();
	unsigned char index;
	unsigned char address;
	unsigned int crc16;

	// Check if the recieved number of bytes matches the calculated bytes
	// minus the request bytes.
	// id + function + (2 * address bytes) + (2 * no of register bytes) +
	// byte count + (2 * CRC bytes) = 9 bytes
	if (frame.getData()[6] == (frame.getLength() - 9)) {
		// check exception 2 ILLEGAL DATA ADDRESS
		if (frame.getStartingAddress() < MAX_BUFFER_SIZE) {
			// check exception 3 ILLEGAL DATA VALUE
			if (maxData <= MAX_BUFFER_SIZE) {
				// start at the 8th byte in the frame
				address = 7;

				for (index = frame.getStartingAddress(); index < maxData;
						index++) {
					ctx->getHoldingRegisters()[index] = JOIN(
							frame.getData()[address],
							frame.getData()[address + 1]);
					address += 2;
				}

				// a function 16 response is an echo of the first 6 bytes from
				// the request + 2 crc bytes
				unsigned char response[7];
				response[0] = frame.getData()[0];
				response[1] = frame.getData()[1];
				response[2] = frame.getData()[2];
				response[3] = frame.getData()[3];
				response[4] = frame.getData()[4];
				response[5] = frame.getData()[5];

				// only the first 6 bytes are used for CRC calculation
				crc16 = crc(frame.getData(), 6);
				response[6] = crc16 >> 8; // split crc into 2 bytes
				response[7] = crc16 & 0xFF;

				// don't respond if it's a broadcast message
				if (!isBroadcast(frame)) {
					sendPacket(response, 8);
				}
			} else {
				exceptionResponse(frame, 3); // exception 3 ILLEGAL DATA VALUE
			}
		} else {
			exceptionResponse(frame, 2); // exception 2 ILLEGAL DATA ADDRESS
		}
	} else {
		// corrupted packet
	}
}

/* Context implementation */
ModbusContext::ModbusContext(unsigned int* di, unsigned int* ir,
		unsigned int* hr, unsigned int* c) {
	this->discreteInputs = di;
	this->inputRegisters = ir;
	this->holdingRegisters = hr;
	this->coils = c;
}

unsigned int* ModbusContext::getDiscreteInputs() {
	return this->discreteInputs;
}

unsigned int* ModbusContext::getInputRegisters() {
	return this->inputRegisters;
}

unsigned int* ModbusContext::getHoldingRegisters() {
	return this->holdingRegisters;
}

unsigned int* ModbusContext::getCoils() {
	return this->coils;
}

/* Frame implementation */

Frame::Frame(unsigned char buffer[], unsigned int bufferSize) {
	this->length = bufferSize;

	if (length > 0) {
		memcpy(this->data, buffer, bufferSize);
		this->address = data[0];
		this->function = data[1];
		this->crc = JOIN(data[bufferSize - 2], data[bufferSize - 1]);
		this->startingAddress = JOIN(data[2], data[3]); // combine the starting address bytes
		this->noOfRegisters = JOIN(data[4], data[5]); // combine the number of register bytes
	}
}

Frame::~Frame() {
}

unsigned int Frame::getLength() {
	return this->length;
}
unsigned int Frame::getAddress() {
	return this->address;
}
unsigned char* Frame::getData() {
	return this->data;
}
unsigned int Frame::getCrc() {
	return this->crc;
}
unsigned char Frame::getFunction() {
	return this->function;
}

unsigned int Frame::getStartingAddress() {
	return this->startingAddress;
}

unsigned int Frame::getNoOfRegisters() {
	return this->noOfRegisters;
}
