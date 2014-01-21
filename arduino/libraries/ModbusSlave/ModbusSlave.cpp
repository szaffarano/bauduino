/*
 * ModbusSlave.cpp
 *
 *  Created on: 12/01/2014
 *      Author: sebas
 */

/* ModbusSlave implementation */

#include <ModbusSlave.h>

/* ################### private helper methods ################### */
static unsigned int crc(unsigned char* buffer, unsigned char bufferSize);

enum {
	READ_HOLDING_REGISTERS, WRITE_MULTIPLE_REGISTERS, FUNCTIONS_SIZE
};

static FunctionRegistry* registry;

/* ################### ModbusSlave implementation ################### */
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
		this->frameDelay = 1750;
	} else {
		this->interCharTimeout = 15000000 / baud; // 1T * 1.5 = T1.5
		this->frameDelay = 35000000 / baud; // 1T * 3.5 = T3.5
	}

	registry = new FunctionRegistry();
	registry->add(new ReadHoldingRegisters());
	registry->add(new WriteMultipleRegisters());
}

ModbusRequest ModbusSlave::readRequest() {
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

	return ModbusRequest(buffer, bufferSize);
}

void ModbusSlave::update() {
	ModbusRequest req = readRequest();
	ModbusResponse* response = NULL;

	// The minimum request packet is 8 bytes for function 3 & 16
	if (req.getLength() > 7 && req.getLength() < MAX_BUFFER_SIZE) {
		// if the recieved ID matches the slaveID or broadcasting id (0), continue
		if (req.match(this->id) || req.isBroadcast()) {
			// if the calculated crc matches the recieved crc continue
			if (crc(req.getData(), req.getLength() - 2) == req.getCrc()) {

				ModbusFunction* func = registry->get(req.getFunction());
				//unsigned char function = req.getFunction();
				//for (int i = 0; i < FUNCTIONS_SIZE; i++) {
					//if (function == functions[i]->id()) {
						//func = functions[i];
						//break;
					//}
				//}

				if (func != NULL) {
					response = func->execute(req, *this);
				} else {
					response = new ExceptionResponse(0x1, &req);
				}
			} else {
				// checksum failed
			}
		} // incorrect id
	} else if (req.getLength() > 0 && req.getLength() < 8) {
		// corrupted packet
	}

	if (response != NULL) {
		response->write(this);
		delete response;
	}
}

ModbusContext* ModbusSlave::getContext() {
	return ctx;
}

unsigned char ModbusSlave::getId() {
	return id;
}

unsigned char ModbusSlave::getTxEnablePin() {
	return txEnablePin;
}

Stream* ModbusSlave::getPort() {
	return port;
}

unsigned int ModbusSlave::getFrameDelay() {
	return frameDelay;
}

/* ################### ModbusBlock implementation ################### */
ModbusBlock::ModbusBlock(unsigned int size, unsigned int initialValue) {
	this->size = size;
	for (int i = 0; i < size; i++) {
		this->data[i] = initialValue;
	}
}

unsigned int* ModbusBlock::getData() {
	return this->data;
}

unsigned int ModbusBlock::getSize() {
	return this->size;
}

/* ################### ModbusContext implementation ################### */
void ModbusContext::setHoldingRegisters(ModbusBlock* regs) {
	this->holdingRegisters = regs;
}

ModbusBlock* ModbusContext::getHoldingRegisters() {
	return this->holdingRegisters;
}

/* ################### ModbusRequest implementation ################### */
ModbusRequest::ModbusRequest(unsigned char buffer[], unsigned int bufferSize) {
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
unsigned int ModbusRequest::getLength() {
	return length;
}

boolean ModbusRequest::match(unsigned char id) {
	return address == id;
}

boolean ModbusRequest::isBroadcast() {
	return address == 0x0;
}

unsigned char* ModbusRequest::getData() {
	return data;
}

unsigned int ModbusRequest::getCrc() {
	return crc;
}

unsigned char ModbusRequest::getFunction() {
	return function;
}

unsigned int ModbusRequest::getStartingAddress() {
	return startingAddress;
}

unsigned int ModbusRequest::getNoOfRegisters() {
	return noOfRegisters;
}

ModbusRequest::~ModbusRequest() {
}

/* ################### ReadHoldingRegisters implementation ################### */
ModbusResponse* ReadHoldingRegisters::execute(ModbusRequest req,
		ModbusSlave slave) {
	unsigned int maxData = req.getStartingAddress() + req.getNoOfRegisters();
	unsigned char index;
	unsigned char address;
	unsigned int crc16;

	ModbusContext* ctx = slave.getContext();
	unsigned int id = slave.getId();

	if (req.isBroadcast()) {
		// illegal function: 0x03 with broadcasts client
		return new ExceptionResponse(0x1, &req);
	}

	// check exception 2 ILLEGAL DATA ADDRESS
	if (req.getStartingAddress() < ctx->getHoldingRegisters()->getSize()) {
		// check exception 3 ILLEGAL DATA VALUE
		if (maxData <= ctx->getHoldingRegisters()->getSize()) {
			unsigned char noOfBytes = req.getNoOfRegisters() * 2;

			// ID, function, noOfBytes, (dataLo + dataHi)*number of registers,
			//  crcLo, crcHi
			unsigned char responseFrameSize = 5 + noOfBytes;
			unsigned char response[responseFrameSize];
			response[0] = id;
			response[1] = req.getFunction();
			response[2] = noOfBytes;
			address = 3; // PDU starts at the 4th byte
			unsigned int temp;

			for (index = req.getStartingAddress(); index < maxData; index++) {
				temp = ctx->getHoldingRegisters()->getData()[index];
				response[address] = temp >> 8; // split the register into 2 bytes
				address++;
				response[address] = temp & 0xFF;
				address++;
			}

			crc16 = crc(response, responseFrameSize - 2);
			response[responseFrameSize - 2] = crc16 >> 8; // split crc into 2 bytes
			response[responseFrameSize - 1] = crc16 & 0xFF;

			//sendPacket(response, responseFrameSize, slave);
			return new SuccessResponse(response, responseFrameSize);
		} else
			// exception 3 ILLEGAL DATA VALUE
			return new ExceptionResponse(0x3, &req);
	} else {
		// exception 2 ILLEGAL DATA ADDRESS
		return new ExceptionResponse(0x2, &req);
	}
	return new NullResponse();
}

unsigned char ReadHoldingRegisters::id() {
	return 0x3;
}

/* ################### WriteMultipleRegisters implementation ################### */
ModbusResponse* WriteMultipleRegisters::execute(ModbusRequest request,
		ModbusSlave slave) {

	unsigned int maxData = request.getStartingAddress()
			+ request.getNoOfRegisters();
	unsigned char index;
	unsigned char address;
	unsigned int crc16;

	ModbusContext* ctx = slave.getContext();

	// Check if the recieved number of bytes matches the calculated bytes
	// minus the request bytes.
	// id + function + (2 * address bytes) + (2 * no of register bytes) +
	// byte count + (2 * CRC bytes) = 9 bytes
	if (request.getData()[6] == (request.getLength() - 9)) {
		// check exception 2 ILLEGAL DATA ADDRESS
		if (request.getStartingAddress()
				< ctx->getHoldingRegisters()->getSize()) {
			// check exception 3 ILLEGAL DATA VALUE
			if (maxData <= ctx->getHoldingRegisters()->getSize()) {
				// start at the 8th byte in the frame
				address = 7;

				for (index = request.getStartingAddress(); index < maxData;
						index++) {
					ctx->getHoldingRegisters()->getData()[index] = JOIN(
							request.getData()[address],
							request.getData()[address + 1]);
					address += 2;
				}

				// a function 16 response is an echo of the first 6 bytes from
				// the request + 2 crc bytes
				unsigned char response[7];
				response[0] = request.getData()[0];
				response[1] = request.getData()[1];
				response[2] = request.getData()[2];
				response[3] = request.getData()[3];
				response[4] = request.getData()[4];
				response[5] = request.getData()[5];

				// only the first 6 bytes are used for CRC calculation
				crc16 = crc(request.getData(), 6);
				response[6] = crc16 >> 8; // split crc into 2 bytes
				response[7] = crc16 & 0xFF;

				// don't respond if it's a broadcast message
				if (!request.isBroadcast()) {
					// sendPacket(response, 8, slave);
					return new SuccessResponse(response, 8);
				}
			} else {
				// exception 3 ILLEGAL DATA VALUE
				return new ExceptionResponse(0x3, &request);
			}
		} else {
			// exception 2 ILLEGAL DATA ADDRESS
			return new ExceptionResponse(0x2, &request);
		}
	} else {
		// corrupted packet
	}

	return new NullResponse();
}

unsigned char WriteMultipleRegisters::id() {
	return 0x10;
}

/* ################### ExceptionResponse implementation ################### */
void ModbusResponse::sendPackage(ModbusSlave* slave, unsigned char* data,
		unsigned char size) {
	unsigned char tx = slave->getTxEnablePin();
	digitalWrite(tx, HIGH);

	Stream* port = slave->getPort();
	for (unsigned char i = 0; i < size; i++) {
		port->write(data[i]);
	}
	port->flush();

	// allow a frame delay to indicate end of transmission
	delayMicroseconds(slave->getFrameDelay());

	digitalWrite(tx, LOW);

}

/* ################### NullResponse implementation ################### */
void NullResponse::write(ModbusSlave* slave) {
}

/* ################### SuccessResponse implementation ################### */
SuccessResponse::SuccessResponse(unsigned char* buffer,
		unsigned char bufferSize) {
	this->size = bufferSize;
	memcpy(this->data, buffer, bufferSize);
}

void SuccessResponse::write(ModbusSlave* slave) {
	sendPackage(slave, data, size);
}

/* ################### ExceptionResponse implementation ################### */
ExceptionResponse::ExceptionResponse(unsigned char code,
		ModbusRequest* request) {
	this->code = code;
	this->request = request;
}

void ExceptionResponse::write(ModbusSlave* slave) {
	unsigned char response[5];

	// don't respond if its a broadcast message
	if (!request->isBroadcast()) {
		response[0] = slave->getId();
		response[1] = (request->getFunction() | 0x80); // set MSB bit high, informs the master of an exception
		response[2] = code;
		unsigned int crc16 = crc(response, 3); // ID, function|0x80, exception code
		response[3] = crc16 >> 8;
		response[4] = crc16 & 0xFF;
		// exception response is always 5 bytes
		// ID, function + 0x80, exception code, 2 bytes crc
		sendPackage(slave, response, 5);
	}

}

/* ################### FunctionRegistry implementation ################### */
FunctionRegistry::FunctionRegistry(unsigned int initialSize) {
	this->size = initialSize;
	this->functions = new ModbusFunction*[size];
	for (int i = 0; i < size; i++) {
		this->functions[i] = NULL;
	}
	last = -1;

}

ModbusFunction* FunctionRegistry::add(ModbusFunction* func) {
	if (last + 1 >= this->size) {
		int newSize = size * 2;
		ModbusFunction** resized = new ModbusFunction*[newSize];
		for (int i = 0; i < size; i++) {
			resized[i] = functions[i];
		}
		delete functions;
		functions = resized;
		size = newSize;
	}
	this->functions[++last] = func;

	return func;
}

ModbusFunction* FunctionRegistry::get(unsigned char id) {
	for (int i = 0; i <= last; i++) {
		if (functions[i]->id() == id) {
			return functions[i];
		}
	}
	return NULL;
}

FunctionRegistry::~FunctionRegistry() {
	delete functions;
}

/* ################### helper methods implementation ################### */
static unsigned int crc(unsigned char* buffer, unsigned char bufferSize) {
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

