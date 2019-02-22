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

int dumpNBTFile(char *file);
int dumpAnvilFile(char *file);
int dumpAnvilFilePart(int fd, int pos, int size);
unsigned char *decompress(unsigned char *in, int *size);

void recurPrint(struct nbt_tag* nbt, int ri) {
	char unknownbuf[32];
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
		tn = unknownbuf;
		snprintf(unknownbuf, sizeof unknownbuf, "INTARRAY[%d]", nbt->data.nbt_intarray.count);
		valbuf = malloc(16 * nbt->data.nbt_intarray.count + 16);
		valbuf[0] = '{';
		valbuf[1] = 0;
		for (int32_t i = 0; i < nbt->data.nbt_intarray.count; i++) {
			char nta[16];
			snprintf(nta, 16, i == 0 ? "%i" : ", %i", nbt->data.nbt_intarray.ints[i]);
			strcat(valbuf, nta);
		}
		strcat(valbuf, "}");
	}
	else if (nbt->id == NBT_TAG_LONGARRAY) {
		tn = unknownbuf;
		snprintf(unknownbuf, sizeof unknownbuf, "LONGARRAY[%d]", nbt->data.nbt_longarray.count);
		valbuf = malloc(25 * nbt->data.nbt_longarray.count + 16);
		valbuf[0] = '{';
		valbuf[1] = 0;
		for (int32_t i = 0; i < nbt->data.nbt_longarray.count; i++) {
			char nta[25];
			snprintf(nta, 25, i == 0 ? "%016lx" : ", %016lx", nbt->data.nbt_longarray.longs[i]);
			strcat(valbuf, nta);
		}
		strcat(valbuf, "}");
	} else {
		tn=unknownbuf;
		snprintf(unknownbuf, sizeof unknownbuf, "UNKNOWN %d", nbt->id);
	}
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
		printf("Usage: nbtview <file> ...\n");
		exit(0);
	}
	int errs=0;
	for (int i=1; i<argc; i++) {
		char *ext = strrchr(argv[i], '.');
		if (ext != NULL
		    && (strcmp(ext, ".mca") == 0 || strcmp(ext, ".mcr") == 0)) {
		    	errs+=dumpAnvilFile(argv[i]);
		} else {
			errs+=dumpNBTFile(argv[i]);
		}
	}
	return errs;
}

int dumpAnvilFile(char *file) {
	uint32_t locations[1024];
	int fd = open(file, O_RDONLY);
	if (fd < 0) {
		printf("Error opening %s: %s\n", file, strerror(errno));
		return 1;
	}
	if (read(fd, locations, sizeof locations) != sizeof locations) {
		printf("File header too short on %s\n", file);
	}
	for (int i = 0; i < 1024; i++) {
		if (locations[i] == 0) {
			printf("chunk %d/%d not generated\n", i%32, i/32);
		} else {
			int offset=
				(locations[i] & 0x0ff ) << 16
				| (locations[i] & 0xff00)
				| (locations[i] & 0xff0000) >> 16;
			int sectors=(locations[i] & 0xff000000) >> 24;
			printf("chunk %d/%d\n", i%32, i/32);
			dumpAnvilFilePart(fd, offset*4096, sectors*4096);
		}
	}
	return 0;
}

int dumpAnvilFilePart(int fd, int pos, int size) {
	unsigned char *compressed=malloc(size);
	lseek(fd, pos, SEEK_SET);
	if (read(fd, compressed, size)!=size) {
		printf("can't read %d bytes\n", size);
		return 1;
	}
	// printf("length=%08x type=%d\n", *(int32_t *)compressed, compressed[4]);
	unsigned char *uncomp = decompress(compressed+5, &size);
	free(compressed);
	if (uncomp==NULL) {
		return 1;
	}
	struct nbt_tag* nbt = NULL;
	if (!readNBT(&nbt, uncomp, size)) {
		printf("Failed reading NBT: %s\n", strerror(errno));
		return 1;
	}
	recurPrint(nbt, 0);
	return 0;
}

unsigned char *decompress(unsigned char *in, int *size) {
	z_stream stream;
	stream.zalloc = Z_NULL;
	stream.zfree = Z_NULL;
	stream.opaque = Z_NULL;
	if (inflateInit2(&stream, (32 + MAX_WBITS)) != Z_OK) {
		printf("inflateInit Error\n");
		return NULL;
	}

	size_t outsize=*size*3;
	unsigned char *out=malloc(outsize);
	stream.avail_in=*size;
	stream.next_in=in;
	stream.avail_out=outsize;
	stream.next_out=out;

	do {
		if (outsize - stream.total_out < 8192) {
			outsize += 16384;
			out = realloc(out, outsize);
		}
		stream.avail_out = outsize - stream.total_out;
		stream.next_out = out + stream.total_out;
		if (inflate(&stream, Z_FINISH) == Z_STREAM_ERROR) {
			printf("Compression Read Error!\n");
			inflateEnd(&stream);
			free(out);
			return NULL;
		}
	} while (stream.avail_out == 0);
	inflateEnd(&stream);
	*size=outsize;
	return out;
}


int dumpNBTFile(char *file) {
	int fd = open(file, O_RDONLY);
	if (fd < 0) {
		printf("Error opening %s: %s\n", file, strerror(errno));
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
