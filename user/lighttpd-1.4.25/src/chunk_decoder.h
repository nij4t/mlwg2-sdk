/*
  chunk_decoder.h

  small library for decoding http requests with "transfer-encoding: chunked".

  (c) by Andreas Stockel 2010

  This file is licensed under the revised BSD license, see ../COPYING for more info
*/

#ifndef _CHUNK_DECODER_H_
#define _CHUNK_DECODER_H_

#include <stddef.h>

#define CHUNK_DECODER_STATE_START 0
#define CHUNK_DECODER_STATE_READING 1
#define CHUNK_DECODER_STATE_END 2
#define CHUNK_DECODER_STATE_ERROR 3

//Int64 data type used for storing the size information for (hypotetically) very large files
typedef long long chk_int64;

/*Public data of the chunk decoder. You must not set any of these values, they
are for read access only.*/
typedef struct{
	/*The current state of the chunk decoder. May be CHUNK_DECODER_STATE_START if
	the chunk decoder has just been created, CHUNK_DECODER_STATE_READING if the
	chunk decoder is just working on the data, CHUNK_DECODER_STATE_END if the
	chunk decoder has come to a normal end and CHUNK_DEOCDER_STATE_ERROR if the
	chunk decoder came to an abnormal ending and has not been able to go on decoding
	the data. */
	int state;
	/*Contains the full size of the decoded data.*/
	chk_int64 size;
} chunk_decoder;

/*Pointer on chunk_decoder*/
typedef chunk_decoder* lp_chunk_decoder;

/*Create an instance of chunk_decoder*/
lp_chunk_decoder chunk_decoder_create();

/*Feed the deocder with data from mem_in of read_size size. The deocder should
write the decoded data to mem_out. The size of the data written is stored in
write_size. The memory reserved on mem_out should at least have the size of
read_size. chunk_decoder_work will return 0 if the operation had been executed
successfully, -1 if an error occurred and 1 if decoding has finished. Additionally
it set decoder->state accordingly.*/
int chunk_decoder_work(lp_chunk_decoder decoder, char *mem_in, char *mem_out,
	size_t read_size, size_t *write_size);

/*Free the instance of chunk_decoder*/
void chunk_decoder_free(lp_chunk_decoder decoder);

#endif /*_CHUNK_DECODER_H_*/
