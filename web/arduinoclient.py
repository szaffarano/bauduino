#!/usr/bin/env python

import logging
from pymodbus.client.sync import ModbusSerialClient

logging.basicConfig()
log = logging.getLogger()
log.setLevel(logging.INFO)

client_id = 0x3

modbus_config = { 
     'method': 'rtu', 
     'port': '/dev/rfcomm0',
     'stopbits': 2,
     'bytesize': 8,
     'parity': 'N',
     'baudrate': 9600,
     'timeout': 1
}

logging.info("Starting modbus client...")

client = ModbusSerialClient(**modbus_config)
client.connect()

logging.info("Modbus client initialized!")

logging.info("Getting holding registers...")
ex = client.read_holding_registers(address=0, count=9, unit=client_id);
logging.info("Holding registers values: %s" % ex.registers)

ex = client.write_registers(1, [1], unit=client_id)
logging.info("Write registers response: %s" % ex)

logging.info("Getting holding registers with error")
ex = client.read_holding_registers(address=0, count=20, unit=client_id);
logging.info("Write registers response with error: %s" % ex)

logging.info("Getting date and time")
ex = client.read_holding_registers(address=3, count=6, unit=client_id);
regs = ex.registers
logging.info("Current Date: %d/%d/%d %d:%d:%d" % (regs[0], regs[1], regs[2], regs[3], regs[4], regs[5]))

logging.info("Closing modbus client")
client.close()
