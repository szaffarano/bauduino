#!/usr/bin/env python
# -*- coding: utf-8 -*- 

import logging
from pymodbus.client.sync import ModbusSerialClient
from pymodbus.pdu import ExceptionResponse
import pymodbus.pdu

logging.basicConfig()
log = logging.getLogger()
log.setLevel(logging.INFO)

client_id = 0x3

modbus_config = {
     'method': 'rtu',
     'port': '/dev/rfcomm0',
     #'port': '/dev/ttyACM0',
     'stopbits': 2,
     'bytesize': 8,
     'parity': 'N',
     'baudrate': 9600,
     'timeout': 1
}

client = ModbusSerialClient(**modbus_config)
client.connect()

while True:
    # testeo errores en invocacion a funcion invalida
    ex = client.read_coils(address=0, count=20, unit=client_id);
    if ex is not None and isinstance(ex, ExceptionResponse):
        logging.debug("read_coils with error: %s" % ex)
    else:
        logging.error("Error invocando read_coils con argumentos invalidos: %s" % type(ex))
    
    # testeo errores en invocacion a funcion con argumentos invalidos
    ex = client.read_holding_registers(address=0, count=99, unit=client_id)
    if ex is not None and isinstance(ex, ExceptionResponse):
        logging.debug("read_holding registers with error: %s" % ex)
    else:
        logging.error("Error invocando write holding registers con argumentos invalidos")
        
    ex = client.read_holding_registers(address=0, count=10, unit=client_id);
    if ex is not None and not isinstance(ex, ExceptionResponse):
        logging.debug("Holding registers: %s" % ex.registers)
    else:
        logging.error("Error pidiendo HRs: %s" % ex)
        continue
    
    regs = ex.registers

    led_value = 0 if regs[0] == 1 else 1
    
    ex = client.write_registers(0, [led_value], unit=client_id)
    if ex is not None and not isinstance(ex, ExceptionResponse):
        logging.debug("Nuevo valor para el led: %d, respuesta: %s" % (led_value, ex))
    else:
        logging.error("Error seteando led con valor %d (%s)" % (led_value, ex))

    logging.info("%02d/%02d/%d %02d:%02d:%02d:\tLed %03s, Light: %s ohms, Counter #1: % 4d, Counter #2: % 4d" % (regs[4], regs[5], regs[6], regs[7], regs[8], regs[9], ("ON" if led_value == 1 else "OFF"), regs[1], regs[2], regs[3]))

    # Leo los registros  una vez más para que el contador del callback cuente el doble que
    # el del callback de write registers
    client.read_holding_registers(address=0, count=1, unit=client_id);

client.close()
