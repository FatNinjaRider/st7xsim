st7sim - this an implemention of an instruction set simulator for an STMicroelectronics ST6/7/8 processor.
As implemented by Rick Stievenart.
This particular implementation was targeted to an ST7 superset which include a custom 0x70 prefix byte
This is a fully functional ISS which was used to aid in reverse engineering efforts -it inlcudes the capability
to load motorola SREC and binary files, it supports execution and data access breakpoints, it allows saving
and loading snapshots of the RAM and IO space values, it supports emulation of hardware peripherals, it also
can provide a faily acurate execution time profile.
Here is a link to the wikipedia page for the processor: https://en.wikipedia.org/wiki/ST6_and_ST7
