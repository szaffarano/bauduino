#ifndef SIMPLE_MODBUS_SLAVE_H
#define SIMPLE_MODBUS_SLAVE_H

// SimpleModbusSlaveV7

/*
 SimpleModbusSlave allows you to communicate
 to any slave using the Modbus RTU protocol.
 
 This implementation DOES NOT fully comply with the Modbus specifications.
 
 Specifically the frame time out have not been implemented according
 to Modbus standards. The code does however combine the check for
 inter character time out and frame time out by incorporating a maximum
 time out allowable when reading from the message stream.
 
 SimpleModbusSlave implements an unsigned int return value on a call to modbus_update().
 This value is the total error count since the slave started. It's useful for fault finding.
 
 This code is for a Modbus slave implementing functions 3 and 16
 function 3: Reads the binary contents of holding registers (4X references)
 function 16: Presets values into a sequence of holding registers (4X references)
 
 All the functions share the same register array.
 
 Note:  
 The Arduino serial ring buffer is 128 bytes or 64 registers.
 Most of the time you will connect the arduino to a master via serial
 using a MAX485 or similar.
 
 In a function 3 request the master will attempt to read from your
 slave and since 5 bytes is already used for ID, FUNCTION, NO OF BYTES
 and two BYTES CRC the master can only request 122 bytes or 61 registers.
 
 In a function 16 request the master will attempt to write to your 
 slave and since a 9 bytes is already used for ID, FUNCTION, ADDRESS, 
 NO OF REGISTERS, NO OF BYTES and two BYTES CRC the master can only write
 118 bytes or 59 registers.
 
 Using a USB to Serial converter the maximum bytes you can send is 
 limited to its internal buffer which differs between manufactures. 
 
 The functions included here have been derived from the 
 Modbus Specifications and Implementation Guides
 
 http://www.modbus.org/docs/Modbus_over_serial_line_V1_02.pdf
 http://www.modbus.org/docs/Modbus_Application_Protocol_V1_1b.pdf
 http://www.modbus.org/docs/PI_MBUS_300.pdf
 */

#include <Arduino.h>

#define MAX_BUFFER_SIZE 128

typedef struct {
	// discrete inputs
	unsigned int* di;

	// input registers
	unsigned int* ir;

	// holding registers
	unsigned int* hr;

	// coils
	unsigned int* co;

	unsigned int errors;
} modbus_state_t;

typedef struct {
	Stream* port;
	unsigned char slaveId;
	unsigned char txEnablePin;

	unsigned int T1_5; // inter character time out
	unsigned int T3_5; // frame delay

	unsigned int discreteInputsSize;
	unsigned int inputRegistersSize;
	unsigned int holdingRegistersSize;
	unsigned int coilsSize;
} modbus_config_t;

typedef struct {
	unsigned char address;
	unsigned char function;
	unsigned char data[MAX_BUFFER_SIZE];
	unsigned int crc;
	unsigned char length;
	unsigned int startingAddress;
	unsigned int noOfRegisters;
} frame_t;

typedef void(*handler_func)(frame_t);

typedef struct {
	unsigned char id;
	handler_func handler;
	handler_func callback;
} function_t;

// function definitions
void modbus_update();
modbus_state_t modbus_configure(Stream *port, long baud, unsigned char slaveId,
		unsigned char txEnablePin, unsigned int di_size, unsigned int ir_size,
		unsigned int hr_size, unsigned int co_size);
void add_modbus_callback(unsigned char function_id, void(*callback)(frame_t));
#endif
