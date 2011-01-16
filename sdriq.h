/* vim:ts=4:sw=4:et */
#ifndef _SDRIQ_H
#define _SDRIQ_H 1
#include <sys/select.h>

#define MAX_DELAY_SECS 0
#define MAX_DELAY_USECS 5000

#define COMMAND_BUF_SIZE 32
#define READ_BUF_SIZE 9000

#define HOST_SET_CTRL_ITEM 	0x00
#define HOST_REQ_CTRL_ITEM 	0x01
#define HOST_REQ_CTRL_ITEM_RANGE 0x02
#define HOST_DATA_ITEM_ACK 	0x03
#define HOST_DATA_ITEM_0 	0x04
#define HOST_DATA_ITEM_1 	0x05
#define HOST_DATA_ITEM_2 	0x06
#define HOST_DATA_ITEM_3 	0x07

#define TARGET_RESP_CTRL_ITEM 	0x00
#define TARGET_UNSOL_CTRL_ITEM 	0x01
#define TARGET_RESP_CTRL_ITEM_RANGE 0x02
#define TARGET_DATA_ACK 	0x03
#define TARGET_DATA_ITEM_0	0x04
#define TARGET_DATA_ITEM_1	0x05
#define TARGET_DATA_ITEM_2	0x06
#define TARGET_DATA_ITEM_3	0x07

/* Control Items */
#define TARGET_NAME		        0x0001
#define TARGET_SERIAL		    0x0002
#define TARGET_INTERFACE_VER	0x0003
#define TARGET_FIRMWARE_VER	    0x0004
#define FIRMWARWE_VER_BOOT_CODE	0x00		/* Parameters to TARGET_FIRMWARE_VER */
#define FIRMWARWE_VER_FIRMWARE	0x01

#define TARGET_STATUS		    0x0005
#define TARGET_STATUS_IDLE      0x0B        /* Status codes */
#define TARGET_STATUS_BUSY      0x0C
#define LOADING_AD6620          0x0D
#define BOOT_MODE_IDLE          0x0E
#define BOOT_MODE_BUSY_PROG     0x0F
#define ADC_OVERLOAD            0x20
#define BOOT_MODE_PROG_ERR      0x80

#define TARGET_STATUS_STRING	0x0006

#define RECEIVER_STATE		    0x0018
#define RECEIVER_STATE_IDLE     0x01        /* Receiver states */
#define RECEIVER_STATE_RUN      0x02

#define RECEIVER_FREQUENCY      0x0020

#define ADC_SAMPLE_RATE         0x00B0

#define RF_GAIN                 0x0038
#define RF_GAIN_FIXED           0x00		/* 0, -10, -20, and -30 are only allowable */
#define RF_GAIN_MANUAL          0x01		/* In manual mode, bits 0-6 specify linear
	                	    				   gain value for AD8370.  Bit 7 enables or
                		    				   disables a fixed front-end 10dB attenuator. */


typedef struct {
    char *model;
    char *serial;
    uint16_t interface_version;
    uint16_t bootcode_version;
    uint16_t firmware_version;
    // Eventually this will contain other data such as sample rate, receiver state, frequency etc.
    // Alternatively this may be stored in a state structure vs. the more static info structure
} SDRIQ_Info;
    
typedef struct {
    int fd;
    fd_set readfs, writefds, exceptfds;
    struct timeval tv;
    char *command_buf;
    char *read_buf;
    SDRIQ_Info *info;
} SDRIQ;

int sdriq_init(SDRIQ *sdriq, char *devnode);
int sdriq_get_info(SDRIQ *sdriq);
int sdriq_begin_capture(SDRIQ *sdriq);
int sdriq_end_capture(SDRIQ *sdriq);
int sdriq_fetch(SDRIQ *sdriq, void *buffer, uint16_t bufsize);
int sdriq_close(SDRIQ *sdriq);
    
#endif
