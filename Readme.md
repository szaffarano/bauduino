Bauduino
========

Sistema de control de riego automatizado.


Construcción
------------

Hace falta tener instalado [cmake](http://www.cmake.org/) y la IDE de [Arduino](http://arduino.cc/) para poder configurar y construir el proyecto:

    $ cd $BAUDUINO_HOME
    $ arduino/configure arduino

    $ git submodule init
    $ git submodule update

    $ cd build # ingresar al directorio de construcción del proyecto
    $ make help # lista todos los targets Make
    $ make all # compila el sketch y las librerias
    $ make upload # sube el sketch compilado a arduino (configurado para nano328)
    $ make Bauduino-serial # se conecta utilizando picocom via serial port

Roadmap
-------
1. Terminar protocolo de comunicación bluetooth.
1. Documentar esquemáticos con Kicad.
1. Agregar sensor de humedad en tierra.
1. Mejorar algoritmo de riego para tomar en cuenta condiciones de humedad y temperatura.
1. Interfaz web para manejo.
1. Interfaz android para manejo.

