/*
 * ModbusSlave.h
 *
 *  Created on: 12/01/2014
 *      Author: sebas
 */

#ifndef MODBUSSLAVE_H_
#define MODBUSSLAVE_H_

#include <Arduino.h>

#define JOIN(HIGH, LOW)         ((HIGH << 8) | LOW)
#define MAX_BUFFER_SIZE 128

class ModbusBlock {
private:
	unsigned int data[MAX_BUFFER_SIZE];
	unsigned int size;
public:
	ModbusBlock(unsigned int size, unsigned int initialValue = 0);
	unsigned int* getData();
	unsigned int getSize();
};

class ModbusContext {
private:
	ModbusBlock* holdingRegisters;
public:
	void setHoldingRegisters(ModbusBlock* regs);
	ModbusBlock* getHoldingRegisters();
};

class ModbusRequest {
private:
	unsigned int length;
	unsigned int address;
	unsigned char data[MAX_BUFFER_SIZE];
	unsigned int crc;
	unsigned char function;
	unsigned int startingAddress;
	unsigned int noOfRegisters;
public:
	ModbusRequest(unsigned char buffer[], unsigned int bufferSize);
	unsigned int getLength();
	boolean match(unsigned char id);
	boolean isBroadcast();
	unsigned char* getData();
	unsigned int getCrc();
	unsigned char getFunction();
	unsigned int getStartingAddress();
	unsigned int getNoOfRegisters();
	~ModbusRequest();
};

class ModbusSlave {
private:
	Stream* port;
	ModbusContext* ctx;
	unsigned char id;
	unsigned char txEnablePin;
	unsigned int interCharTimeout;
	unsigned int frameDelay;
protected:
	ModbusRequest readRequest();
public:
	ModbusSlave(Stream *port, ModbusContext* ctx, unsigned char id,
			unsigned char txEnablePin, long baud);
	void update();
	ModbusContext* getContext();
	unsigned char getId();
	unsigned char getTxEnablePin();
	Stream* getPort();
	unsigned int getFrameDelay();
};

class ModbusResponse {
protected:
	void sendPackage(ModbusSlave* slave, unsigned char* data,
			unsigned char size);
public:
	virtual void write(ModbusSlave* slave) = 0;
	virtual ~ModbusResponse() {
	}
};

class ExceptionResponse: public ModbusResponse {
private:
	ModbusRequest* request;
	unsigned char code;
public:
	ExceptionResponse(unsigned char code, ModbusRequest* request);
	void write(ModbusSlave* slave);
};

class NullResponse: public ModbusResponse {
public:
	void write(ModbusSlave* slave);
};

class SuccessResponse: public ModbusResponse {
private:
	unsigned char data[MAX_BUFFER_SIZE];
	unsigned char size;
public:
	SuccessResponse(unsigned char* buffer, unsigned char bufferSize);
	void write(ModbusSlave* slave);
};

class ModbusFunction {
public:
	virtual ModbusResponse* execute(ModbusRequest request,
			ModbusSlave slave) = 0;
	virtual unsigned char id() = 0;
	virtual ~ModbusFunction() {
	}
};

class ReadHoldingRegisters: public ModbusFunction {
public:
	ModbusResponse* execute(ModbusRequest request, ModbusSlave slave);
	unsigned char id();
};

class WriteMultipleRegisters: public ModbusFunction {
public:
	ModbusResponse* execute(ModbusRequest request, ModbusSlave slave);
	unsigned char id();
};

class FunctionRegistry {
private:
	ModbusFunction** functions;
	int size;
	int last;
public:
	FunctionRegistry(unsigned int initialSize = 5);
	ModbusFunction* add(ModbusFunction* func);
	ModbusFunction* get(unsigned char id);
	~FunctionRegistry();
};

#endif /* MODBUSSLAVE_H_ */
