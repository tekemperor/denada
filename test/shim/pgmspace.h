// Host-build shim for pgmspace.h.
//
// PROGMEM and pgm_read_byte exist to put constant tables in AVR flash and read
// them back through a separate address space. On the host (and on the ESP32,
// for that matter) memory is flat, so both collapse to nothing.
#ifndef HOST_SHIM_PGMSPACE_H
#define HOST_SHIM_PGMSPACE_H

#define PROGMEM
#define pgm_read_byte(address) (*(const unsigned char *)(address))

#endif // HOST_SHIM_PGMSPACE_H
