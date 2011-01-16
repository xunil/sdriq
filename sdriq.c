/* vim:ts=4:sw=4:et */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>
#include <fcntl.h>
#include <errno.h> 
//#include <sndfile.h>
#include "sdriq.h"

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

int sdriq_get_info(SDRIQ *sdriq) {
    int rc;
    int message_len;

    if (sdriq == NULL) {
        return -1;
    }

    if (sdriq->info == NULL) {
        sdriq->info = (SDRIQ_Info *)malloc(sizeof(SDRIQ_Info));
    }

    message_len = build_message(HOST_REQ_CTRL_ITEM, TARGET_NAME, sdriq->command_buf, COMMAND_BUF_SIZE, NULL, 0);
    if (message_len < 0) {
        return message_len;
    }

    rc = write (sdriq->fd, sdriq->command_buf, message_len);
    if (rc < 0) {
        return -2;
    }

    FD_ZERO(&sdriq->readfds);
    FD_SET(sdriq->fd, &sdriq->readfds);
    sdriq->tv.tv_sec = MAX_DELAY_SECS;
    sdriq->tv.tv_usec = MAX_DELAY_USECS;

    rc = select(1, &sdriq->readfds, NULL, NULL, &sdriq->tv);
    if (rc < 0) {
        // Error!
        return -3;
    } else if (rc == 0) {
        // Timeout!
        return -4;
    }
    
    rc = read(sdriq->fd, sdriq->read_buf, READ_BUF_SIZE);
    if (rc < 0) {
        return -5;
    }

    // Need to read the returned data and decode
}

// int sdriq_begin_capture(SDRIQ *sdriq);
// int sdriq_end_capture(SDRIQ *sdriq);
// int sdriq_fetch(SDRIQ *sdriq, void *buffer, uint16_t bufsize);

int sdriq_close(SDRIQ *sdriq) {
    if (sdriq == NULL)
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
