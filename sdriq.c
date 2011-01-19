/* vim:ts=4:sw=4:et */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <fcntl.h>
#include <errno.h> 
#include <string.h>
//#include <sndfile.h>
#include "sdriq.h"

int build_message(uint8_t message_type, uint16_t message, char *message_buf, uint16_t message_buf_len, 
                    const char *parameters, uint16_t param_len) {
    uint16_t message_len = 4;   // Request will always consist of at least the control item header; 2 bytes
                                // and the control item ID itself; another 2 bytes
	char *p;

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

	p = message_buf;
    *p = (message_len & 0xFF);        // Length LSB
	*++p = ((message_type << 5) | ((message_len >> 8) & 0x10));     // Command type and length MSB
	*++p = (message & 0xFF);
	*++p = ((message >> 8) & 0xFF);

    return message_len;
}

SDRIQ_Message *decode_message(SDRIQ *sdriq) {
    SDRIQ_Message *message;
    
    message = (SDRIQ_Message *)malloc(sizeof(SDRIQ_Message));
    message->length = (uint16_t)(sdriq->read_buf[0] & 0xFF);
    message->type = (uint8_t)((sdriq->read_buf[1] >> 5) & 0xFF);
    message->length |= (uint16_t)((sdriq->read_buf[1] & 0x1F) << 8);
    message->control_item = (uint16_t)(sdriq->read_buf[2]);
    message->control_item |= (uint16_t)(sdriq->read_buf[3] << 8);
    
    if (message->length > 4) {
        /* Data follows */
        message->data = (char *)malloc(message->length - 4);
        memcpy(message->data, sdriq->read_buf+4, message->length-4);
    } else {
        message->data = NULL;
    }
    
    return message;
}

int sdriq_init(SDRIQ *sdriq, char *devnode) {
    SDRIQ *handle;

    if (sdriq == NULL || devnode == NULL)
        return -1;

    handle = (SDRIQ *)malloc(sizeof(SDRIQ));
    handle->fd = open(devnode, O_RDWR | O_NDELAY);
    if (handle->fd < 0) {
        free(handle);
        return -2;
    }

    handle->command_buf = (char *)malloc(COMMAND_BUF_SIZE);
    handle->read_buf = (char *)malloc(READ_BUF_SIZE);

    handle->info = NULL;    // Unallocated until sdriq_get_info called

    sdriq = handle;

    return 0;
}

SDRIQ_Message *message_with_reply(SDRIQ *sdriq, SDRIQ_Message *out_msg) {
    int rc;
    int message_len;
    SDRIQ_Message *message;
	fd_set readfds;
	struct timeval tv;

    if (sdriq == NULL || out_msg == NULL) {
        return NULL;
    }
    
    message_len = build_message(out_msg->type, out_msg->control_item, sdriq->command_buf, COMMAND_BUF_SIZE, out_msg->data, out_msg->length);
    if (message_len < 0) {
        return NULL;
    }

    rc = write (sdriq->fd, sdriq->command_buf, message_len);
    if (rc < 0) {
        return NULL;
    }

    FD_ZERO(&readfds);
    FD_SET(sdriq->fd, &readfds);
    tv.tv_sec = MAX_DELAY_SECS;
    tv.tv_usec = MAX_DELAY_USECS;

    rc = select(1, &readfds, NULL, NULL, &tv);
    if (rc < 0) {
        // Error!
        return NULL;
    } else if (rc == 0) {
        // Timeout!
        return NULL;
    }

    rc = read(sdriq->fd, sdriq->read_buf, READ_BUF_SIZE);
    if (rc < 0) {
        return NULL;
    }

    message = decode_message(sdriq);
    
    return message;
}

int sdriq_get_info(SDRIQ *sdriq) {
    int i;
    SDRIQ_Message out_msg;
    SDRIQ_Message *reply_msg;

    if (sdriq == NULL) {
        return -1;
    }

    if (sdriq->info == NULL) {
        sdriq->info = (SDRIQ_Info *)malloc(sizeof(SDRIQ_Info));
    }    
    
    // Device model name
    out_msg.type = HOST_REQ_CTRL_ITEM;
    out_msg.control_item = TARGET_NAME;
    out_msg.length = 0;
    out_msg.data = NULL;
    reply_msg = message_with_reply(sdriq, &out_msg);
    sdriq->info->model = (char *)malloc(reply_msg->length); // Includes trailing zero!
    strncpy(sdriq->info->model, reply_msg->data, reply_msg->length);
    free(reply_msg->data);
    free(reply_msg);

    // Device serial number
    out_msg.type = HOST_REQ_CTRL_ITEM;
    out_msg.control_item = TARGET_SERIAL;
    out_msg.length = 0;
    out_msg.data = NULL;
    reply_msg = message_with_reply(sdriq, &out_msg);
    sdriq->info->serial = (char *)malloc(reply_msg->length); // Includes trailing zero!
    strncpy(sdriq->info->serial, reply_msg->data, reply_msg->length);
    free(reply_msg->data);
    free(reply_msg);
        
    // Device interface (I/O protocol) version
    out_msg.type = HOST_REQ_CTRL_ITEM;
    out_msg.control_item = TARGET_INTERFACE_VER;
    out_msg.length = 0;
    out_msg.data = NULL;
    reply_msg = message_with_reply(sdriq, &out_msg);
    sdriq->info->interface_version = (uint16_t)(reply_msg->data[0] | (reply_msg->data[1] << 8));
    free(reply_msg->data);
    free(reply_msg);
    
    // Device interface (I/O protocol) version
    out_msg.type = HOST_REQ_CTRL_ITEM;
    out_msg.control_item = TARGET_FIRMWARE_VER;
    out_msg.length = 1;
    out_msg.data = (char *)malloc(sizeof(char));
    out_msg.data[0] = FIRMWARWE_VER_BOOT_CODE;
    reply_msg = message_with_reply(sdriq, &out_msg);
    sdriq->info->interface_version = (uint16_t)(reply_msg->data[0] | (reply_msg->data[1] << 8));
    free(reply_msg->data);
    free(reply_msg);

    // Device interface (I/O protocol) version
    out_msg.type = HOST_REQ_CTRL_ITEM;
    out_msg.control_item = TARGET_FIRMWARE_VER;
    out_msg.length = 1;
    out_msg.data[0] = FIRMWARWE_VER_FIRMWARE;
    reply_msg = message_with_reply(sdriq, &out_msg);
    sdriq->info->interface_version = (uint16_t)(reply_msg->data[0] | (reply_msg->data[1] << 8));
    free(reply_msg->data);
    free(reply_msg);

    free(out_msg.data);

    return 0;
}

// int sdriq_begin_capture(SDRIQ *sdriq);
// int sdriq_end_capture(SDRIQ *sdriq);
// int sdriq_fetch(SDRIQ *sdriq, void *buffer, uint16_t bufsize);

int sdriq_close(SDRIQ *sdriq) {
    if (sdriq == NULL) {
        return -1;
    }

    // Future: Check receiver state and stop capture if running
    close(sdriq->fd);

    if (sdriq->info != NULL) {
        if (sdriq->info->model != NULL)
            free(sdriq->info->model);
        if (sdriq->info->serial != NULL)
            free(sdriq->info->serial);
        free(sdriq->info);
    }

    if (sdriq->command_buf != NULL)
        free(sdriq->command_buf);
    if (sdriq->read_buf != NULL)
        free(sdriq->read_buf);

    free(sdriq);

    return 0;
}

int main(int argc, char *argv[]) {
	SDRIQ sdriq;
	
	sdriq_init(&sdriq, "/dev/ft2450");
	sdriq_get_info(&sdriq);
	sdriq_close(&sdriq);
	
	return 0;
}
