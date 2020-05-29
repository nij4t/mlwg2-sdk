/*******************************************************************************
 mod_h264_streaming.c

 mod_h264_streaming - A lighttpd plugin for pseudo-streaming Quicktime/MPEG4 files.
 http://h264.code-shop.com

 Copyright (C) 2007 CodeShop B.V.

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/ 

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "base.h"
#include "log.h"
#include "buffer.h"
#include "response.h"
#include "http_chunk.h"
#include "stat_cache.h"

#include "plugin.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* plugin config for all request/connections */

typedef struct {
	array *extensions;
} plugin_config;

typedef struct {
	PLUGIN_DATA;

	buffer *query_str;
	array *get_params;

	plugin_config **config_storage;

	plugin_config conf;
} plugin_data;

/* init the plugin data */
INIT_FUNC(mod_h264_streaming_init) {
	plugin_data *p;

	p = calloc(1, sizeof(*p));

	p->query_str = buffer_init();
	p->get_params = array_init();

	return p;
}

/* detroy the plugin data */
FREE_FUNC(mod_h264_streaming_free) {
	plugin_data *p = p_d;

	UNUSED(srv);

	if (!p) return HANDLER_GO_ON;

	if (p->config_storage) {
		size_t i;

		for (i = 0; i < srv->config_context->used; i++) {
			plugin_config *s = p->config_storage[i];

			if (!s) continue;

			array_free(s->extensions);

			free(s);
		}
		free(p->config_storage);
	}

	buffer_free(p->query_str);
	array_free(p->get_params);

	free(p);

	return HANDLER_GO_ON;
}

/* handle plugin config and check values */

SETDEFAULTS_FUNC(mod_h264_streaming_set_defaults) {
	plugin_data *p = p_d;
	size_t i = 0;

	config_values_t cv[] = {
		{ "h264-streaming.extensions",   NULL, T_CONFIG_ARRAY, T_CONFIG_SCOPE_CONNECTION },       /* 0 */
		{ NULL,                         NULL, T_CONFIG_UNSET, T_CONFIG_SCOPE_UNSET }
	};

	if (!p) return HANDLER_ERROR;

	p->config_storage = calloc(1, srv->config_context->used * sizeof(specific_config *));

	for (i = 0; i < srv->config_context->used; i++) {
		plugin_config *s;

		s = calloc(1, sizeof(plugin_config));
		s->extensions     = array_init();

		cv[0].destination = s->extensions;

		p->config_storage[i] = s;

		if (0 != config_insert_values_global(srv, ((data_config *)srv->config_context->data[i])->value, cv)) {
			return HANDLER_ERROR;
		}
	}

	return HANDLER_GO_ON;
}

#define PATCH(x) \
	p->conf.x = s->x;
static int mod_h264_streaming_patch_connection(server *srv, connection *con, plugin_data *p) {
	size_t i, j;
	plugin_config *s = p->config_storage[0];

	PATCH(extensions);

	/* skip the first, the global context */
	for (i = 1; i < srv->config_context->used; i++) {
		data_config *dc = (data_config *)srv->config_context->data[i];
		s = p->config_storage[i];

		/* condition didn't match */
		if (!config_check_cond(srv, con, dc)) continue;

		/* merge config */
		for (j = 0; j < dc->value->used; j++) {
			data_unset *du = dc->value->data[j];

			if (buffer_is_equal_string(du->key, CONST_STR_LEN("h264-streaming.extensions"))) {
				PATCH(extensions);
			}
		}
	}

	return 0;
}
#undef PATCH

static int split_get_params(array *get_params, buffer *qrystr) {
	size_t is_key = 1;
	size_t i;
	char *key = NULL, *val = NULL;

	key = qrystr->ptr;

	/* we need the \0 */
	for (i = 0; i < qrystr->used; i++) {
		switch(qrystr->ptr[i]) {
		case '=':
			if (is_key) {
				val = qrystr->ptr + i + 1;

				qrystr->ptr[i] = '\0';

				is_key = 0;
			}

			break;
		case '&':
		case '\0': /* fin symbol */
			if (!is_key) {
				data_string *ds;
				/* we need at least a = since the last & */

				/* terminate the value */
				qrystr->ptr[i] = '\0';

				if (NULL == (ds = (data_string *)array_get_unused_element(get_params, TYPE_STRING))) {
					ds = data_string_init();
				}
				buffer_copy_string_len(ds->key, key, strlen(key));
				buffer_copy_string_len(ds->value, val, strlen(val));

				array_insert_unique(get_params, (data_unset *)ds);
			}

			key = qrystr->ptr + i + 1;
			val = NULL;
			is_key = 1;
			break;
		}
	}

	return 0;
}

extern unsigned int moov_seek(unsigned char* moov_data,
                              uint64_t*  moov_size,
                              float start_time,
                              float end_time,
                              uint64_t* mdat_start,
                              uint64_t* mdat_size,
                              uint64_t offset);

void write_char(unsigned char* outbuffer, int value)
{
  outbuffer[0] = (unsigned char)(value);
}

void write_int32(unsigned char* outbuffer, long value)
{
  outbuffer[0] = (unsigned char)((value >> 24) & 0xff);
  outbuffer[1] = (unsigned char)((value >> 16) & 0xff);
  outbuffer[2] = (unsigned char)((value >> 8) & 0xff);
  outbuffer[3] = (unsigned char)((value >> 0) & 0xff);
}

struct atom_t
{
  unsigned char type_[4];
  uint64_t size_;
  uint64_t start_;
  uint64_t end_;
};

#define ATOM_PREAMBLE_SIZE 8

unsigned int atom_header_size(unsigned char* atom_bytes)
{
  return (atom_bytes[0] << 24) +
         (atom_bytes[1] << 16) +
         (atom_bytes[2] << 8) +
         (atom_bytes[3]);
}

int atom_read_header(FILE* infile, struct atom_t* atom)
{
  unsigned char atom_bytes[ATOM_PREAMBLE_SIZE];

  atom->start_ = ftell(infile);

  fread(atom_bytes, ATOM_PREAMBLE_SIZE, 1, infile);
  memcpy(&atom->type_[0], &atom_bytes[4], 4);
  atom->size_ = atom_header_size(atom_bytes);
  atom->end_ = atom->start_ + atom->size_;

  return 1;
}

void atom_write_header(unsigned char* outbuffer, struct atom_t* atom)
{
  int i;
  write_int32(outbuffer, atom->size_);
  for(i = 0; i != 4; ++i)
    write_char(outbuffer + 4 + i, atom->type_[i]);
}

int atom_is(struct atom_t const* atom, const char* type)
{
  return (atom->type_[0] == type[0] &&
          atom->type_[1] == type[1] &&
          atom->type_[2] == type[2] &&
          atom->type_[3] == type[3])
         ;
}

void atom_skip(FILE* infile, struct atom_t const* atom)
{
  fseek(infile, atom->end_, SEEK_SET);
}

void atom_print(struct atom_t const* atom)
{
  printf("Atom(%c%c%c%c,%lld)\n", atom->type_[0], atom->type_[1],
          atom->type_[2], atom->type_[3], atom->size_);
}


URIHANDLER_FUNC(mod_h264_streaming_path_handler) {
	plugin_data *p = p_d;
	int s_len;
	size_t k;

	UNUSED(srv);

	if (buffer_is_empty(con->physical.path)) return HANDLER_GO_ON;

	mod_h264_streaming_patch_connection(srv, con, p);

	s_len = con->physical.path->used - 1;

	for (k = 0; k < p->conf.extensions->used; k++) {
		data_string *ds = (data_string *)p->conf.extensions->data[k];
		int ct_len = ds->value->used - 1;

		if (ct_len > s_len) continue;
		if (ds->value->used == 0) continue;

		if (0 == strncmp(con->physical.path->ptr + s_len - ct_len, ds->value->ptr, ct_len)) {
			data_string *get_param;
			stat_cache_entry *sce = NULL;
			buffer *b;
			double start = 0.0;
			double end = 0.0;
			char *err = NULL;

			array_reset(p->get_params);
			buffer_copy_string_buffer(p->query_str, con->uri.query);
			split_get_params(p->get_params, p->query_str);

			/* if there is a start=[0-9]+ in the header use it as start,
			 * otherwise send the full file */
			if (NULL != (get_param = (data_string *)array_get_element(p->get_params, "start")))
                        {
			  /* check if it is a number */
			  start = strtod(get_param->value->ptr, &err);
			  if (*err != '\0') {
			  	return HANDLER_GO_ON;
			  }
			  if (start < 0) return HANDLER_GO_ON;
                        }

			/* if there is an end=[0-9]+ in the header use it as end */
			if (NULL != (get_param = (data_string *)array_get_element(p->get_params, "end")))
                        {
			  /* check if it is a number */
			  end = strtod(get_param->value->ptr, &err);
			  if (*err != '\0') {
				  return HANDLER_GO_ON;
                          }
			  if (end < 0 || start >= end) return HANDLER_GO_ON;
			}

                        /* get file info */
			if (HANDLER_GO_ON != stat_cache_get_entry(srv, con, con->physical.path, &sce)) {
				return HANDLER_GO_ON;
			}

			/* we are safe now, let's build a h264 header */
			b = chunkqueue_get_append_buffer(con->write_queue);
#if 0
			BUFFER_COPY_STRING_CONST(b, "FLV\x1\x1\0\0\0\x9\0\0\0\x9");
			http_chunk_append_file(srv, con, con->physical.path, start, sce->st.st_size - start);
#else
			{
			FILE* infile;
			struct atom_t ftyp_atom;
			struct atom_t moov_atom;
			struct atom_t mdat_atom;
			unsigned char* moov_data = 0;
			unsigned char* ftyp_data = 0;

			infile = fopen(con->physical.path->ptr, "rb");
			if(!infile) {
				return HANDLER_GO_ON;
			}

			{
				unsigned int filesize = sce->st.st_size;

				struct atom_t leaf_atom;
				while(ftell(infile) < filesize)
				{
					if(!atom_read_header(infile, &leaf_atom))
						break;

					atom_print(&leaf_atom);

					if(atom_is(&leaf_atom, "ftyp"))
					{
						ftyp_atom = leaf_atom;
						ftyp_data = malloc(ftyp_atom.size_);
						fseek(infile, ftyp_atom.start_, SEEK_SET);
						fread(ftyp_data, ftyp_atom.size_, 1, infile);
					}
					else
					if(atom_is(&leaf_atom, "moov"))
					{
						moov_atom = leaf_atom;
						moov_data = malloc(moov_atom.size_);
						fseek(infile, moov_atom.start_, SEEK_SET);
						fread(moov_data, moov_atom.size_, 1, infile);
					}
					else
					if(atom_is(&leaf_atom, "mdat"))
					{
						mdat_atom = leaf_atom;
					}
					atom_skip(infile, &leaf_atom);
				}
			}
			fclose(infile);

            if(!moov_data) {
                if (ftyp_data) free(ftyp_data);
                return HANDLER_GO_ON;
            }
			{

			unsigned int mdat_start = (ftyp_data ? ftyp_atom.size_ : 0) + moov_atom.size_ + 42;
			if(!moov_seek(moov_data,
					&moov_atom.size_,
					start, end,
					&mdat_atom.start_, &mdat_atom.size_,
					mdat_start - mdat_atom.start_))
				return HANDLER_GO_ON;

			if(ftyp_data)
			{
				buffer_append_memory(b, ftyp_data, ftyp_atom.size_);
				free(ftyp_data);
			}

      {
        static char const free_data[] = {
          0x0, 0x0, 0x0,  42, 'f', 'r', 'e', 'e',
          'v', 'i', 'd', 'e', 'o', ' ', 's', 'e',
          'r', 'v', 'e', 'd', ' ', 'b', 'y', ' ',
          'm', 'o', 'd', '_', 'h', '2', '6', '4',
          '_', 's', 't', 'r', 'e', 'a', 'm', 'i',
          'n', 'g'
        };
        buffer_append_memory(b, free_data, sizeof(free_data));
      }

			buffer_append_memory(b, moov_data, moov_atom.size_);
			free(moov_data);

			{
			unsigned char mdat_bytes[ATOM_PREAMBLE_SIZE];
//			mdat_atom.size_ -= bytes_to_skip;
			atom_write_header(mdat_bytes, &mdat_atom);
			buffer_append_memory(b, mdat_bytes, ATOM_PREAMBLE_SIZE);
			b->used++; /* add virtual \0 */
			}

			http_chunk_append_file(srv, con, con->physical.path, mdat_atom.start_ + ATOM_PREAMBLE_SIZE,
						mdat_atom.size_ - ATOM_PREAMBLE_SIZE);
			}
			}
#endif
			response_header_overwrite(srv, con, CONST_STR_LEN("Content-Type"), CONST_STR_LEN("video/mp4"));
			response_header_overwrite(srv, con, CONST_STR_LEN("X-Mod-H264-Streaming"), CONST_STR_LEN("1.0"));

			con->file_finished = 1;

			return HANDLER_FINISHED;
		}
	}

	/* not found */
	return HANDLER_GO_ON;
}

/* this function is called at dlopen() time and inits the callbacks */

int mod_h264_streaming_plugin_init(plugin *p) {
	p->version     = LIGHTTPD_VERSION_ID;
	p->name        = buffer_init_string("h264_streaming");

	p->init        = mod_h264_streaming_init;
	p->handle_physical = mod_h264_streaming_path_handler;
	p->set_defaults  = mod_h264_streaming_set_defaults;
	p->cleanup     = mod_h264_streaming_free;

	p->data        = NULL;

	return 0;
}

