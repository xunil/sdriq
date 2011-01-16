/* vim:ts=4:sw=4:et */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <fcntl.h>
#include <errno.h> 
#include <sndfile.h>

#define LENGTH_LSB(a) (a & 0xff)
#define LENGTH_MSB(a) ((a >> 8) & 0x1f)
#define MSG_TYPE(a) (a<<5)

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

/* Writes a message into message_buf. */
int build_message(uint8_t message_type, uint16_t message, 
                    char *message_buf, uint16_t message_buf_len, 
                    const char *parameters, uint16_t param_len) {
    uint16_t message_len = 4;   // Request will always consist of at least the control item header; 2 bytes
                                // and the control item ID itself; another 2 bytes

    if (message_buf == NULL) {
        return -1;
    }

    if (parameters != NULL && param_len > 0) {
        if (message_len + param_len > message_buf_len) {
            return -2;
        }

        message_len += param_len;
        memcpy (message_buf+4, parameters, param_len);
    }

    *message_buf = (message_len & 0xFF);        // Length LSB
    *(message_buf+1) = ((HOST_REQUEST_CONTROL_ITEM << 5) | ((message_len >> 8) & 0x10))     // Command type and length MSB
    *(message_buf+2) = (message & 0xFF)
    *(message_buf+3) = ((message >> 8) & 0xFF);

    return message_len;
}

int build_set_control_item(uint16_t message, char *message_buf, uint16_t message_buf_len, 
                    const char *parameters, uint16_t param_len) {
    return build_message(HOST_SET_CTRL_ITEM, message, message_buf, message_buf_len, parameters, param_len);
}

int build_req_control_item(uint16_t message, char *message_buf, uint16_t message_buf_len, 
                    const char *parameters, uint16_t param_len) {
    return build_message(HOST_REQ_CTRL_ITEM, message, message_buf, message_buf_len, parameters, param_len);
}

int build_req_control_item_range(uint16_t message, char *message_buf, uint16_t message_buf_len, 
                    const char *parameters, uint16_t param_len) {
    return build_message(HOST_REQ_CTRL_ITEM_RANGE, message, message_buf, message_buf_len, parameters, param_len);
}



int main(int argc, char *argv[]) {
    int fd, i, count;
    char c;
    char cmdbuf[32];
    char readbuf[8200];
    uint8_t msgtype;
    uint16_t msglen;

    fd = open("/dev/ft2450", O_RDWR | O_NDELAY);
    if (fd < 0) {
        perror("Could not open /dev/ft2450");
        exit(1);
    }

    cmdbuf[0] = 0x04;
    cmdbuf[1] = 0x20;
    cmdbuf[2] = 0x01;
    cmdbuf[3] = 0x00;
    write(fd, cmdbuf, 4);

    usleep(250);


    count = read(fd, readbuf, 8200);
    if (count == -1) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            perror("Unexpected I/O error");
            exit(2);
        }
    } else {
        msgtype = (readbuf[1] & 0xE0) >> 5;
        msglen = 0;
        msglen = ((uint16_t)readbuf[0] | ((readbuf[1] & 0x1F) << 8));
        uint16_t control_item = (uint16_t)(readbuf[2] | (readbuf[3] << 8));
        switch (msgtype) {
            case TARGET_RESP_CTRL_ITEM:
                // SDR-IQ is responding to an earlier request
                switch (control_item) {
                    case TARGET_NAME:
                        printf("Received response to control item request: %6s\n", readbuf+4);
                        break;
                }
                break;
            case TARGET_UNSOL_CTRL_ITEM:
                // Unsolicited control item: Probably another data block
                break;
            case TARGET_RESP_CTRL_ITEM_RANGE:
                // Response to request for range of control items
                break;
            case TARGET_DATA_ACK:
                // Acknowledgement of receipt of one of the DATA_ITEM types.
                break;
            case TARGET_DATA_ITEM_0:
            case TARGET_DATA_ITEM_1:
            case TARGET_DATA_ITEM_2:
            case TARGET_DATA_ITEM_3:
                printf("TARGET_DATA_ITEM_%d\n", msgtype-TARGET_DATA_ITEM_0);
                break;
        }
    }



    return 0;
}
