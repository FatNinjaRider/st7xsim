//-----------------------------------------------------------------------------
//
//   application.h 
//
//   Author:
//      Rick Stievenart - 09/08/2017
//
//   Updates:
//
//-----------------------------------------------------------------------------

//
//--------------------------------------------------------
// Function prototypes
//--------------------------------------------------------
//
void application_ui_help(void);
int application_ui_parse(int c);

// for i/o emulation
void application_reset(void);
void application_load_io_and_memory_initial_values(void);
int application_get_data_memory_byte(unsigned int address, unsigned char *data);
int application_put_data_memory_byte(unsigned int address, unsigned char data);

void application_triggers_and_breakpoints(void);

// specifc application stuff (tag)
void load_inbound_message(void);
void tag_information(void);

void display_do_loop_vars(void);

