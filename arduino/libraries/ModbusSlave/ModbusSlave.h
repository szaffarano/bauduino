/*
 * ModbusSlave.h
 *
 *  Created on: 12/01/2014
 *      Author: sebas
 */

#ifndef MODBUSSLAVE_H_
#define MODBUSSLAVE_H_

#include <Arduino.h>

#define JOIN(HIGH, LOW) 	((HIGH << 8) | LOW)

#define MAX_BUFFER_SIZE 128

class ModbusContext {
private:
	unsigned int* discreteInputs;
	unsigned int* inputRegisters;
	unsigned int* holdingRegisters;
	unsigned int* coils;
public:
	ModbusContext(unsigned int* di, unsigned int* ir, unsigned int* hr,
			unsigned int* c);
	unsigned int* getDiscreteInputs();
	unsigned int* getInputRegisters();
	unsigned int* getHoldingRegisters();
	unsigned int* getCoils();
};

class Frame {
private:
	unsigned int length;
	unsigned int address;
	unsigned char data[MAX_BUFFER_SIZE];
	unsigned int crc;
	unsigned char function;
	unsigned int startingAddress;
	unsigned int noOfRegisters;
public:
	Frame(unsigned char buffer[], unsigned int bufferSize);
	unsigned int getLength();
	unsigned int getAddress();
	unsigned char* getData();
	unsigned int getCrc();
	unsigned char getFunction();
	unsigned int getStartingAddress();
	unsigned int getNoOfRegisters();
	~Frame();
};

class ModbusSlave {
private:
	Stream* port;
	ModbusContext* ctx;
	unsigned char id;
	unsigned char txEnablePin;
	unsigned int interCharTimeout;
	unsigned int frameDalay;
	Frame getFrame();
	boolean isBroadcast(Frame frame);
	unsigned int crc(unsigned char* buffer, unsigned char bufferSize);

	/* temporal */
	void exceptionResponse(Frame frame, unsigned char exception);
	void sendPacket(unsigned char* data, unsigned char bufferSize);

	void function0x03(Frame frame);
	void function0x10(Frame frame);
public:
	ModbusSlave(Stream *port, ModbusContext* ctx, unsigned char id,
			unsigned char txEnablePin, long baud);
	void update();
};

#endif /* MODBUSSLAVE_H_ */
