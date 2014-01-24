#ifndef MODBUS_CONSTANTS_H
#define MODBUS_CONSTANTS_H

// functions
#define	READ_HOLDING_REGISTERS				0x03
#define	WRITE_MULTIPLE_REGISTERS			0x10

// exception condes
#define ILLEGAL_FUNCTION					0x1
#define	ILLEGAL_DATA_ADDRESS				0x2
#define	ILLEGAL_DATA_VALUE					0x3
#define	SERVER_DEVICE_FAILURE				0x4
#define	ACKNOWLEDGE							0x5
#define	SERVER_DEVICE_BUSY					0x6
#define	MEMORY_PARITY_ERROR					0x8
#define	GATEWAY_PATH_UNAVAILABLE			0xA
#define	GATEWAY_TARGET_FAILED_TO_RESPOND	0xB

// max ADU size
#define MAX_BUFFER_SIZE 					128

#endif // MODBUS_CONSTANTS_H
