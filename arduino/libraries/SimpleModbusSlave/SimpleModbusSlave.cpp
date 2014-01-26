#include <SimpleModbusSlave.h>
#include <string.h>

#define JOIN(HIGH, LOW) 	((HIGH << 8) | LOW)
#define IS_BROADCAST(adu)	adu.address == 0x0

static modbus_context context;

static ADU exceptionResponse(ADU adu, unsigned char exception);
static unsigned int crc(ADU adu);
static void send(ADU adu);

static ADU read_request();
static ADU create_response(unsigned int length);

static modbus_function* search_function(unsigned char id);

static void read_hr(ADU frame);
static void write_mr(ADU frame);

// id, handler, callback
modbus_function functions[] = {
		{ READ_HOLDING_REGISTERS, read_hr, NULL },
		{ WRITE_MULTIPLE_REGISTERS, write_mr, NULL },
		{ 0x0, NULL, NULL }
};

modbus_state modbus_configure(Stream *port, long baud, unsigned char slaveId,
		unsigned char txEnablePin, unsigned int di_size, unsigned int ir_size,
		unsigned int hr_size, unsigned int co_size) {

	context.port = port;
	context.slaveId = slaveId;
	context.discreteInputsSize = di_size;
	context.inputRegistersSize = ir_size;
	context.holdingRegistersSize = hr_size;
	context.coilsSize = co_size;
	context.txEnablePin = txEnablePin;

	pinMode(context.txEnablePin, OUTPUT);
	digitalWrite(context.txEnablePin, LOW);

	if (baud > 19200) {
		context.T1_5 = 750;
		context.T3_5 = 1750;
	} else {
		context.T1_5 = 15000000 / baud; // 1T * 1.5 = T1.5
		context.T3_5 = 35000000 / baud; // 1T * 3.5 = T3.5
	}

	context.state.di = (unsigned int*) malloc(
			sizeof(unsigned int) * context.discreteInputsSize);
	context.state.ir = (unsigned int*) malloc(
			sizeof(unsigned int) * context.inputRegistersSize);
	context.state.hr = (unsigned int*) malloc(
			sizeof(unsigned int) * context.holdingRegistersSize);
	context.state.co = (unsigned int*) malloc(
			sizeof(unsigned int) * context.coilsSize);

	context.state.errors = 0;

	return context.state;
}

void add_modbus_callback(unsigned char function_id, function_handler handler) {
	modbus_function* found = search_function(function_id);
	if (found != NULL) {
		found->callback = handler;
	}
}

ADU read_request() {
	ADU adu;

	unsigned char buffer[MAX_BUFFER_SIZE];
	unsigned char overflow = 0;
	unsigned char buffer_size = 0;

	if (context.port->available()) {
		while (context.port->available()) {
			if (overflow) {
				context.port->read();
			} else {
				if (buffer_size == MAX_BUFFER_SIZE) {
					overflow = 1;
				} else {
					buffer[buffer_size++] = context.port->read();
				}
			}
			delayMicroseconds(context.T1_5);
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
	ADU adu = read_request();

	// The minimum request packet is 8 bytes for function 3 & 16
	if (adu.pdu.length > 7 && adu.pdu.length < MAX_BUFFER_SIZE) {
		// if the recieved ID matches the slaveID or broadcasting id (0), continue
		if (adu.address == context.slaveId || IS_BROADCAST(adu)) {
			// if the calculated crc matches the recieved crc continue
			if (crc(adu) == adu.crc) {
				modbus_function* found = search_function(adu.pdu.function);
				if (found != NULL) {
					found->handler(adu);
					if (found->callback != NULL) {
						found->callback(adu);
					}
				} else {
					ADU exception = exceptionResponse(adu, ILLEGAL_FUNCTION);
					send(exception);
				}
			} else {
				// checksum failed
				context.state.errors++;
			}
		} // incorrect id
	} else if (adu.pdu.length > 0 && adu.pdu.length < 8) {
		context.state.errors++; // corrupted packet
	}
}

ADU exceptionResponse(ADU adu, unsigned char exception) {
	// each call to exceptionResponse() will increment state.errors
	context.state.errors++;

	// don't respond if its a broadcast message
	//if (!IS_BROADCAST(adu)) {
		ADU response = create_response(5);

		response.address = context.slaveId;
		response.pdu.function = (adu.pdu.function | 0x80); // set MSB bit high, informs the master of an exception

		response.pdu.data[0] = context.slaveId;
		response.pdu.data[1] = (adu.pdu.function | 0x80);

		response.pdu.data[2] = exception;
		unsigned int crc16 = crc(response); // ID, function|0x80, exception code
		response.pdu.data[3] = crc16 >> 8;
		response.pdu.data[4] = crc16 & 0xFF;
		// exception response is always 5 bytes
		// ID, function + 0x80, exception code, 2 bytes crc

		//send(response);
	//}
	return response;

}

/**
 * ADU: address + function + data + crc
 */
unsigned int crc(ADU adu) {
	unsigned int temp, temp2, flag;
	unsigned char data[adu.pdu.length - 2];

	data[0] = adu.address;
	data[1] = adu.pdu.function;

	for (int i = 2; i < adu.pdu.length - 2; i++) {
		data[i] = adu.pdu.data[i];
	}
	temp = 0xFFFF;
	for (unsigned char i = 0; i < (adu.pdu.length - 2); i++) {
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

void send(ADU adu) {
	digitalWrite(context.txEnablePin, HIGH);

	for (unsigned char i = 0; i < adu.pdu.length; i++) {
		context.port->write(adu.pdu.data[i]);
	}
	context.port->flush();

	// allow a frame delay to indicate end of transmission
	delayMicroseconds(context.T3_5);

	digitalWrite(context.txEnablePin, LOW);
}

void read_hr(ADU adu) {
	unsigned int startingAddress = JOIN(adu.pdu.data[2], adu.pdu.data[3]);
	unsigned int noOfRegisters = JOIN(adu.pdu.data[4], adu.pdu.data[5]);

	unsigned int maxData = startingAddress + noOfRegisters;
	unsigned char index;
	unsigned char address;
	unsigned int crc16;

	// broadcasting is not supported for function 3
	if (IS_BROADCAST(adu)) {
		ADU exception = exceptionResponse(adu, ILLEGAL_FUNCTION);
		send(exception);
		return;
	}

	// check exception 2 ILLEGAL DATA ADDRESS
	if (startingAddress < context.holdingRegistersSize) {
		// check exception 3 ILLEGAL DATA VALUE
		if (maxData <= context.holdingRegistersSize) {
			unsigned char noOfBytes = noOfRegisters * 2;

			// ID, function, noOfBytes, (dataLo + dataHi)*number of registers,
			//  crcLo, crcHi
			ADU response = create_response(5 + noOfBytes);

			response.address = context.slaveId;
			response.pdu.function = adu.pdu.function;

			response.pdu.data[0] = context.slaveId;
			response.pdu.data[1] = adu.pdu.function;

			response.pdu.data[2] = noOfBytes;
			address = 3; // PDU starts at the 4th byte
			unsigned int temp;

			for (index = startingAddress; index < maxData; index++) {
				temp = context.state.hr[index];
				response.pdu.data[address] = temp >> 8; // split the register into 2 bytes
				address++;
				response.pdu.data[address] = temp & 0xFF;
				address++;
			}

			crc16 = crc(response);
			response.pdu.data[response.pdu.length - 2] = crc16 >> 8; // split crc into 2 bytes
			response.pdu.data[response.pdu.length - 1] = crc16 & 0xFF;
			send(response);
		} else {
			ADU exception = exceptionResponse(adu, ILLEGAL_DATA_VALUE); // exception 3 ILLEGAL DATA VALUE
			send(exception);
		}
	} else {
		ADU exception = exceptionResponse(adu, ILLEGAL_DATA_ADDRESS); // exception 2 ILLEGAL DATA ADDRESS
		send(exception);
	}
}

void write_mr(ADU adu) {
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
		if (startingAddress < context.holdingRegistersSize) {
			// check exception 3 ILLEGAL DATA VALUE
			if (maxData <= context.holdingRegistersSize) {
				// start at the 8th byte in the frame
				address = 7;

				for (index = startingAddress; index < maxData; index++) {
					context.state.hr[index] = JOIN(adu.pdu.data[address],
							adu.pdu.data[address + 1]);
					address += 2;
				}

				// don't respond if it's a broadcast message
				if (!IS_BROADCAST(adu)) {
					// a function 16 response is an echo of the first 6 bytes from
					// the request + 2 crc bytes
					ADU response = create_response(8);

					response.address = adu.address;
					response.pdu.function = adu.pdu.function;

					response.pdu.data[0] = adu.pdu.data[0];
					response.pdu.data[1] = adu.pdu.data[1];
					response.pdu.data[2] = adu.pdu.data[2];
					response.pdu.data[3] = adu.pdu.data[3];
					response.pdu.data[4] = adu.pdu.data[4];
					response.pdu.data[5] = adu.pdu.data[5];

					// only the first 6 bytes are used for CRC calculation
					crc16 = crc(response);
					response.pdu.data[6] = crc16 >> 8; // split crc into 2 bytes
					response.pdu.data[7] = crc16 & 0xFF;

					send(response);
				}
			} else {
				ADU exception = exceptionResponse(adu, ILLEGAL_DATA_VALUE); // exception 3 ILLEGAL DATA VALUE
				send(exception);
			}
		} else {
			ADU exception = exceptionResponse(adu, ILLEGAL_DATA_ADDRESS); // exception 2 ILLEGAL DATA ADDRESS
			send(exception);
		}
	} else {
		context.state.errors++; // corrupted packet
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

static ADU create_response(unsigned int length) {
	ADU response;
	response.pdu.length = length;
	return response;
}
