/*
 * Libusb supporting, Brjtag 1.8f
 * Copyright (c) 2010 hugebird @chinadsl.net
 */

#ifndef LIBUSB_H
#define LIBUSB_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>

void libusb_open(DWORD id, int epin, int epout, int timeout, char *vn, char *pn, char *sn);
void libusb_close(void);
int libusb_bulk_read(char *buf, int bytelen);
int libusb_bulk_write(char *buf, int bytelen);
int libusb_msg_read(int cmd, int value, int index, char *buf, int bytelen);
int libusb_msg_write(int cmd, int value, int index, char *buf, int bytelen);

#endif /* LIBUSB_H */
