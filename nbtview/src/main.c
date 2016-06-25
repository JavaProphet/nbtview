/*
 * main.c
 *
 *  Created on: Jun 22, 2016
 *      Author: root
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include "nbt.h"
#include "xstring.h"
#include <fcntl.h>
#include <errno.h>
#include <zlib.h>

void recurPrint(struct nbt_tag* nbt, int ri) {
	if (nbt == NULL) return;
	char* tn = NULL;
	char* valbuf = NULL;
	if (nbt->id == NBT_TAG_END) tn = "END";
	else if (nbt->id == NBT_TAG_BYTE) {
		tn = "BYTE";
		valbuf = malloc(32);
		valbuf[31] = 0;
		snprintf(valbuf, 31, "%i", nbt->data.nbt_byte);
	} else if (nbt->id == NBT_TAG_SHORT) {
		tn = "SHORT";
		valbuf = malloc(32);
		valbuf[31] = 0;
		snprintf(valbuf, 31, "%i", nbt->data.nbt_short);
	} else if (nbt->id == NBT_TAG_INT) {
		tn = "INT";
		valbuf = malloc(32);
		valbuf[31] = 0;
		snprintf(valbuf, 31, "%i", nbt->data.nbt_int);
	} else if (nbt->id == NBT_TAG_LONG) {
		tn = "LONG";
		valbuf = malloc(32);
		valbuf[31] = 0;
		snprintf(valbuf, 31, "%li", nbt->data.nbt_long);
	} else if (nbt->id == NBT_TAG_FLOAT) {
		tn = "FLOAT";
		valbuf = malloc(32);
		valbuf[31] = 0;
		snprintf(valbuf, 31, "%f", nbt->data.nbt_float);
	} else if (nbt->id == NBT_TAG_DOUBLE) {
		tn = "DOUBLE";
		valbuf = malloc(64);
		valbuf[63] = 0;
		snprintf(valbuf, 63, "%f", nbt->data.nbt_double);
	} else if (nbt->id == NBT_TAG_BYTEARRAY) {
		tn = "BYTEARRAY";
		valbuf = malloc(nbt->data.nbt_bytearray.len * 2 + 1);
		valbuf[nbt->data.nbt_bytearray.len * 2] = 0;
		for (uint32_t i = 0; i < nbt->data.nbt_bytearray.len; i++) {
			snprintf(valbuf + i * 2, 3, "%02X", nbt->data.nbt_bytearray.data[i]);
		}
	} else if (nbt->id == NBT_TAG_STRING) {
		tn = "STRING";
		valbuf = strdup(nbt->data.nbt_string);
	} else if (nbt->id == NBT_TAG_LIST) tn = "LIST";
	else if (nbt->id == NBT_TAG_COMPOUND) tn = "COMPOUND";
	else if (nbt->id == NBT_TAG_INTARRAY) {
		tn = "INTARRAY";
		valbuf = malloc(11 * nbt->data.nbt_intarray.count + 16);
		valbuf[0] = '{';
		valbuf[1] = 0;
		for (int32_t i = 0; i < nbt->data.nbt_intarray.count; i++) {
			char nta[16];
			snprintf(nta, 16, i == 0 ? "%i" : ", %i", nbt->data.nbt_intarray.ints[i]);
			strcat(valbuf, nta);
		}
		strcat(valbuf, "}");
	} else tn = "UNKNOWN";
	char indents[ri * 4 + 1];
	for (int i = 0; i < ri * 4; i++) {
		indents[i] = ' ';
	}
	indents[ri * 4] = 0;
	printf("%s\"%s\" <%s>", indents, nbt->name == NULL ? "" : nbt->name, tn);
	if (valbuf != NULL) {
		printf(" = \"%s\"\n", valbuf);
		free(valbuf);
	} else printf("\n");

	for (size_t i = 0; i < nbt->children_count; i++) {
		recurPrint(nbt->children[i], ri + 1);
	}
}

int main(int argc, char* argv[]) {
	if (argc <= 1) {
		printf("Usage: nbtview <file>\n");
	}
	int fd = open(argv[argc - 1], O_RDONLY);
	if (fd < 0) {
		printf("Error opening file: %s\n", strerror(errno));
		return 1;
	}
	unsigned char* tbuf = malloc(4096);
	size_t ir = 0;
	size_t tc = 4096;
	size_t tr = 0;
	while ((ir = read(fd, tbuf + tr, tc - tr)) > 0) {
		tc += ir;
		tr += ir;
		tbuf = realloc(tbuf, tc);
	}
	tbuf = realloc(tbuf, tr);
	printf("%02x %02X\n", tbuf[0], tbuf[1]);
	//if (tr >= 2 && tbuf[0] == 0x1f && tbuf[1] == 0x8b) {
	{
		void* rtbuf = malloc(16384);
		size_t rtc = 16384;
		z_stream strm;
		strm.zalloc = Z_NULL;
		strm.zfree = Z_NULL;
		strm.opaque = Z_NULL;
		int dr = 0;
		if ((dr = inflateInit2(&strm, (32 + MAX_WBITS))) != Z_OK) { //
			//printf("Compression initialization error!\n");
			free(rtbuf);
			goto mx;
		}
		strm.avail_in = tr;
		strm.next_in = tbuf;
		strm.avail_out = rtc;
		strm.next_out = rtbuf;
		do {
			if (rtc - strm.total_out < 8192) {
				rtc += 16384;
				rtbuf = realloc(rtbuf, rtc);
			}
			strm.avail_out = rtc - strm.total_out;
			strm.next_out = rtbuf + strm.total_out;
			dr = inflate(&strm, Z_FINISH);
			if (dr == Z_STREAM_ERROR) {
				//printf("Compression Read Error!\n");
				inflateEnd(&strm);
				free(rtbuf);
				goto mx;
			}
		} while (strm.avail_out == 0);
		inflateEnd(&strm);
		free(tbuf);
		tbuf = rtbuf;
		tr = strm.total_out;
		mx: ;
	}
	//}
	struct nbt_tag* nbt = NULL;
	if (!readNBT(&nbt, tbuf, tr)) {
		printf("Failed reading NBT: %s\n", strerror(errno));
		return 1;
	}
	recurPrint(nbt, 0);
	return 0;
}
