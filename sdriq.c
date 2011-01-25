// vim:ts=4:sw=4:et
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <fcntl.h>
#include <errno.h> 
#include <string.h>
#include <assert.h>
//#include <sndfile.h>
#include "sdriq.h"

int packet_dump(SDRIQ_Message *message) {
    int i;

    printf("--- DEBUG Dump\n");
    printf("Type: 0x%02X Control Item: 0x%02X%02X\n", message->type, (message->control_item >> 8) & 0xFF, message->control_item & 0xFF);
    printf("Length: %d\n", message->length);

    for (i = 0; i < message->length; i++) {
        printf("%02X ", message->data+i);
        if (i % 16 == 0) {
            printf("\n");
        }
    }
    printf("\n");
    
    return 0;
}

int build_message(SDRIQ *sdriq, SDRIQ_Message *message) { 
    char *p;
    uint16_t message_len = 4;   // Request will always consist of at least the control item header; 2 bytes
                                // and the control item ID itself; another 2 bytes
    if (message->data != NULL && message->length > 0) {
        message_len += message->length;
    }

    p = sdriq->command_buf = (char *)malloc(message_len);
    *p = (message_len & 0xFF);        // Length LSB
	*++p = ((message->type << 5) | ((message_len >> 8) & 0x10));     // Command type and length MSB
	*++p = (message->control_item & 0xFF);
	*++p = ((message->control_item >> 8) & 0xFF);

    if (message->length > 0) {
        memcpy (p, message->data, message->length);
    }

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

SDRIQ *sdriq_init(char *devnode) {
    SDRIQ *handle;

    if (devnode == NULL)
        return NULL;

    handle = (SDRIQ *)malloc(sizeof(SDRIQ));
    memset(handle, 0, sizeof(SDRIQ));
    handle->fd = open(devnode, O_RDWR | O_NDELAY);
    if (handle->fd < 0) {
        free(handle);
        return NULL;
    }

    handle->command_buf = (char *)malloc(COMMAND_BUF_SIZE);
    handle->read_buf = (char *)malloc(READ_BUF_SIZE);

    handle->info = NULL;    // Unallocated until sdriq_get_info called

    return handle;
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
    
    message_len = build_message(sdriq, out_msg);

    rc = write (sdriq->fd, sdriq->command_buf, message_len);
    if (rc < 0) {
        return NULL;
    }

    FD_ZERO(&readfds);
    FD_SET(sdriq->fd, &readfds);
    tv.tv_sec = MAX_DELAY_SECS;
    tv.tv_usec = MAX_DELAY_USECS;

    rc = select((sdriq->fd+1), &readfds, NULL, NULL, &tv);
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
        memset(sdriq->info, 0, sizeof(SDRIQ_Info));
    }    
    
    // Device model name
    out_msg.type = HOST_REQ_CTRL_ITEM;
    out_msg.control_item = TARGET_NAME;
    out_msg.length = 0;
    out_msg.data = NULL;
    reply_msg = message_with_reply(sdriq, &out_msg);
    assert(reply_msg != NULL);
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
    assert(reply_msg != NULL);
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
    assert(reply_msg != NULL);
    if (reply_msg->length > 2) {
        sdriq->info->interface_version = (uint16_t)(reply_msg->data[0] | (reply_msg->data[1] << 8));
    }
    free(reply_msg->data);
    free(reply_msg);
    
    // Firmware version - boot code
    out_msg.type = HOST_REQ_CTRL_ITEM;
    out_msg.control_item = TARGET_FIRMWARE_VER;
    out_msg.length = 1;
    out_msg.data = (char *)malloc(sizeof(char));
    out_msg.data[0] = FIRMWARE_VER_BOOT_CODE;
    reply_msg = message_with_reply(sdriq, &out_msg);
    assert(reply_msg != NULL);
    if (reply_msg->length > 2) {
        sdriq->info->bootcode_version = (uint16_t)(reply_msg->data[0] | (reply_msg->data[1] << 8));
    }
    free(reply_msg->data);
    free(reply_msg);

    // Device interface (I/O protocol) version
    out_msg.type = HOST_REQ_CTRL_ITEM;
    out_msg.control_item = TARGET_FIRMWARE_VER;
    out_msg.length = 1;
    out_msg.data[0] = FIRMWARE_VER_FIRMWARE;
    reply_msg = message_with_reply(sdriq, &out_msg);
    assert(reply_msg != NULL);
    if (reply_msg->length > 2) {
        sdriq->info->firmware_version = (uint16_t)(reply_msg->data[0] | (reply_msg->data[1] << 8));
    }
    free(reply_msg->data);
    free(reply_msg);

    free(out_msg.data);

    return 0;
}

int sdriq_begin_capture(SDRIQ *sdriq, int nblocks) {
    SDRIQ_Message out_msg;
    SDRIQ_Message *reply_msg;

    if (sdriq == NULL) {
        return -1;
    }

    // Set receiver state to capture
    out_msg.type = HOST_REQ_CTRL_ITEM;
    out_msg.control_item = RECEIVER_STATE;
    out_msg.length = 4;
    out_msg.data = (char *)malloc(out_msg.length);
    out_msg.data[0] = 0x81; // Receiver channel ID; only valid value
    out_msg.data[1] = RECEIVER_STATE_RUN;
    out_msg.data[2] = (nblocks <= 0 ? RECEIVE_CONTIGUOUS : RECEIVE_ONESHOT);
    out_msg.data[3] = (nblocks <= 0 ? 0x00 : nblocks); // Number of blocks to capture; ignored in contiguous mode

    reply_msg = message_with_reply(sdriq, &out_msg);
    assert(reply_msg != NULL);
    if (reply_msg->length > 2) {
        packet_dump(reply_msg);
    }
    free(reply_msg->data);
    free(reply_msg);

    free(out_msg.data);

    return 0;
}

int sdriq_end_capture(SDRIQ *sdriq) {
    SDRIQ_Message out_msg;
    SDRIQ_Message *reply_msg;

    if (sdriq == NULL) {
        return -1;
    }

    // Set receiver state to capture
    out_msg.type = HOST_REQ_CTRL_ITEM;
    out_msg.control_item = RECEIVER_STATE;
    out_msg.length = 4;
    out_msg.data = (char *)malloc(out_msg.length);
    out_msg.data[0] = 0x81; // Receiver channel ID; only valid value
    out_msg.data[1] = RECEIVER_STATE_IDLE;
    out_msg.data[2] = 0x00;
    out_msg.data[3] = 0x00;

    reply_msg = message_with_reply(sdriq, &out_msg);
    assert(reply_msg != NULL);
    if (reply_msg->length > 2) {
        packet_dump(reply_msg);
    }
    free(reply_msg->data);
    free(reply_msg);

    free(out_msg.data);

    return 0;
}

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
	SDRIQ *sdriq;
	
	sdriq = sdriq_init("/dev/ft2450");
	sdriq_get_info(sdriq);
    printf("Found %s, serial %s\n", sdriq->info->model, sdriq->info->serial);
    sdriq_begin_capture(sdriq, 1);
    sleep(2);
    sdriq_end_capture(sdriq);
	sdriq_close(sdriq);
	
	return 0;
}
