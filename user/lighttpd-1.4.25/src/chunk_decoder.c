/*
  chunk_decoder.c

  small library for decoding http requests with "transfer-encoding: chunked".

  (c) by Andreas Stockel 2010

  This file is licensed under the revised BSD license, see ../COPYING for more info
*/

#include "chunk_decoder.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define INT_STATE_READ_NUMBER 100
#define INT_STATE_READ_NUMBER_WAIT_FOR_CR 101
#define INT_STATE_READ_NUMBER_WAIT_FOR_LF 102
#define INT_STATE_READ_CHUNK 103
#define INT_STATE_READ_CHUNK_WAIT_FOR_CR 104
#define INT_STATE_READ_CHUNK_WAIT_FOR_LF 105
#define IS_NUMBER(nb) ((nb >= 48) && (nb <= 57))
#define IS_HEX_NUMBER(nb) (((nb >= '0') && (nb <= '9')) || ((nb >= 'A') && (nb <= 'F')) || ((nb >= 'a') && (nb <= 'f')))
#define IS_CRLF(c) ((c == '\r') || (c == '\n'))
#define IS_CR(c) (c == '\r')
#define IS_LF(c) (c == '\n')
#define IS_SPACE(c) (c == ' ')
#define CHAR_TO_NB(nb) (nb - 48)
#define HEX_CHAR_TO_NB(nb) (\
	(nb <= '9') ?\
		(nb - 48) :\
		((nb <= 'F') ?\
			(nb - 65 + 10) :\
			(nb - 97 + 10))\
)

typedef struct{
	chunk_decoder dec;
	int int_state;
	chk_int64 numberbuf;
	chk_int64 cur_chunk_size;
} chunk_decoder_int;
typedef chunk_decoder_int* lp_chunk_decoder_int;

lp_chunk_decoder chunk_decoder_create()
{
	/* Reserve some memory for the chunk_decoder structure and return it */
	lp_chunk_decoder_int dec = malloc(sizeof(*dec));
	memset(dec, 0, sizeof(*dec));
	dec->dec.state = CHUNK_DECODER_STATE_START;
	dec->dec.size = 0;

	return (lp_chunk_decoder)dec;
}

void chunk_decoder_free(lp_chunk_decoder decoder)
{
	/* Free the memory reserved for the chunk_decoder */
	if (decoder != NULL)
		free(decoder);
}

#define READCHAR() {mem_in++; data_remaining--;}
#define WRITECHAR(c) {mem_out[0] = c; mem_out++; dec->dec.size++; dec->cur_chunk_size--; (*write_size)++;}

/* Decode mem_in and write it to mem_out */
int chunk_decoder_work(lp_chunk_decoder decoder, char *mem_in, char *mem_out,
	size_t read_size, size_t *write_size)
{
	*write_size = 0;
	size_t data_remaining = read_size;
	lp_chunk_decoder_int dec = (lp_chunk_decoder_int)decoder;

	while (data_remaining > 0)
	{
		switch (dec->dec.state)
		{

			/* Handle the chunk decoder start state */
			case CHUNK_DECODER_STATE_START:
				if (IS_CRLF(mem_in[0]))
				{
					READCHAR();
				}
				else
				{
					dec->dec.state = CHUNK_DECODER_STATE_READING;
					dec->int_state = INT_STATE_READ_NUMBER;
					dec->numberbuf = -1;
				}
					
				break;
			
			/* Handle the chunk decoder read state. This is the state where the
			   acutal information is read */
			case CHUNK_DECODER_STATE_READING:
				switch (dec->int_state)
				{
					/* Reading the preceding number from every chunk */
					case INT_STATE_READ_NUMBER:
						/* Check whether the current char is a hex number char */
						if (IS_HEX_NUMBER(mem_in[0]))
						{
							/* Append the hex number char to the current hex number being read */
							if (dec->numberbuf == -1)
								dec->numberbuf = HEX_CHAR_TO_NB(mem_in[0]);
							else
								dec->numberbuf = (dec->numberbuf << 4) + HEX_CHAR_TO_NB(mem_in[0]);
							READCHAR();
						}
						else
						{
							/* As there had been no other hex number following, go on
							   reading the actual chunk data */
							if (dec->numberbuf == -1)
							{
								/* There was no chunk size read at all, exit */
								dec->dec.state = CHUNK_DECODER_STATE_ERROR;
								return -1;
							}
							else
							{
								/* If the number read was zero, we're at the end of
								   the chunked encoded data */
								dec->cur_chunk_size = dec->numberbuf;
								if (dec->cur_chunk_size == 0)
								{
									dec->dec.state = CHUNK_DECODER_STATE_END;
									return 1;
								}
								dec->numberbuf = -1;
								dec->int_state = INT_STATE_READ_NUMBER_WAIT_FOR_CR;
							}
						}

						break;

					/* Wait for an CR after reading the number */
					case INT_STATE_READ_NUMBER_WAIT_FOR_CR:
						if (IS_CR(mem_in[0]))
						{
							dec->int_state = INT_STATE_READ_NUMBER_WAIT_FOR_LF;
						}
						READCHAR();

						break;

					/* Wait for an LF after reading the CR. LF has to follow the
					   CR immediately */
					case INT_STATE_READ_NUMBER_WAIT_FOR_LF:
						if (IS_LF(mem_in[0]))
						{
							dec->int_state = INT_STATE_READ_CHUNK;
							READCHAR();
						}
						else
						{
							dec->dec.state = CHUNK_DECODER_STATE_ERROR;
							return -1;
						}
							
						break;

					/* Read the actual chunk data */
					case INT_STATE_READ_CHUNK:
						if (dec->cur_chunk_size > 0)
						{
							WRITECHAR(mem_in[0]);
							READCHAR();
						}
						else
						{
							dec->int_state = INT_STATE_READ_CHUNK_WAIT_FOR_CR;
						}

						break;

					/* Wait for the CR which should follow each chunk */
					case INT_STATE_READ_CHUNK_WAIT_FOR_CR:
						if (IS_CR(mem_in[0]))
						{
							dec->int_state = INT_STATE_READ_CHUNK_WAIT_FOR_LF;
						}
						READCHAR();

						break;

					/* Wait for the LF which should follow the CR (see above) */
					case INT_STATE_READ_CHUNK_WAIT_FOR_LF:
						if (IS_LF(mem_in[0]))
						{
							dec->int_state = INT_STATE_READ_NUMBER;
							READCHAR();
						}
						else
						{
							dec->dec.state = CHUNK_DECODER_STATE_ERROR;
							return -1;
						}

						break;

					/* Default must not happen! */
					default:
						dec->dec.state = CHUNK_DECODER_STATE_ERROR;
						return -1;
				}

				break;

			case CHUNK_DECODER_STATE_ERROR:
				return -1;

			case CHUNK_DECODER_STATE_END:
				/* This function must not be called if the chunk decoder is already finished! */
				dec->dec.state = CHUNK_DECODER_STATE_ERROR;
				return -1;

			/* Default must not happen! */
			default:
				dec->dec.state = CHUNK_DECODER_STATE_ERROR;
				return -1;
		}
	}
	return 0;
}

