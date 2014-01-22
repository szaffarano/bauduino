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
     #'port': '/dev/ttyACM0',
     'stopbits': 2,
     'bytesize': 8,
     'parity': 'N',
     'baudrate': 9600,
     'timeout': 1
}

client = ModbusSerialClient(**modbus_config)
client.connect()

count = 0
while True:
    ex = client.read_holding_registers(address=0, count=10, unit=client_id);
    if ex is not None:
        logging.info("Holding registers: %s" % ex.registers)
    else:
        logging.error("Error pidiendo HRs")
        continue
    
    regs = ex.registers

    led_value = 0 if regs[0] == 1 else 1
    count += 1
    
    regs[2] = count
    regs[0] = led_value
    
    ex = client.write_registers(0, regs, unit=client_id)
    if ex is not None:
        logging.debug("Nuevo valor para el led: %d, respuesta: %s" % (led_value, ex))
    else:
        logging.error("Error seteando led con valor %d" % led_value)

    logging.info("%02d/%02d/%d %02d:%02d:%02d:\tLed %03s, Light: %s ohms, Counter: %d" % (regs[4], regs[5], regs[6], regs[7], regs[8], regs[9], ("ON" if led_value == 1 else "OFF"), regs[1], regs[3]))

    # testeo errores en invocacion a funcion invalida
    ex = client.read_coils(address=0, count=20, unit=client_id);
    if ex is not None:
        logging.debug("read_coils with error: %s" % ex)
    else:
        logging.error("Error invocando read_coils con argumentos invalidos")


client.close()
