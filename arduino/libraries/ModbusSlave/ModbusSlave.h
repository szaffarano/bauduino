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

class ModbusBlock {
private:
	unsigned int size;
	unsigned int* block;
public:
	ModbusBlock(unsigned int size);
	unsigned int getSize();
	unsigned int* getBlock();
	~ModbusBlock();
};

class ModbusContext {
private:
	ModbusBlock* discreteInputs;
	ModbusBlock* inputRegisters;
	ModbusBlock* holdingRegisters;
	ModbusBlock* coils;
public:
	ModbusContext(ModbusBlock* di, ModbusBlock* ir, ModbusBlock* hr, ModbusBlock* c);
	ModbusBlock* getDiscreteInputs();
	ModbusBlock* getInputRegisters();
	ModbusBlock* getHoldingRegisters();
	ModbusBlock* getCoils();
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
