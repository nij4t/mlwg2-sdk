/*******************************************************************************
 moov.c

 moov - A library for splitting Quicktime/MPEG4 files.
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

/* 
  Uses code snippets from the libquicktime library:
   http://libquicktime.sourceforge.net

  The QuickTime File Format PDF from Apple:
    http://developer.apple.com/techpubs/quicktime/qtdevdocs/PDF/QTFileFormat.pdf
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <stdint.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_STDINT_H
# include <stdint.h>
#endif
#ifdef HAVE_INTTYPES_H
# include <inttypes.h>
#endif
#ifdef HAVE_ZLIB_H
// Compress the MOOV atom. Turn this off for Flash as it doesn't support it.
// # define COMPRESS_MOOV_ATOM
# include <zlib.h>
#endif

static int read_char(unsigned char const* buffer)
{
  return buffer[0];
}

static unsigned int read_int32(void const* buffer)
{
  unsigned char* p = (unsigned char*)buffer;
  return (p[0] << 24) + (p[1] << 16) + (p[2] << 8) + p[3];
}

static void write_int32(void* outbuffer, uint32_t value)
{
  unsigned char* p = (unsigned char*)outbuffer;
  p[0] = (unsigned char)((value >> 24) & 0xff);
  p[1] = (unsigned char)((value >> 16) & 0xff);
  p[2] = (unsigned char)((value >> 8) & 0xff);
  p[3] = (unsigned char)((value >> 0) & 0xff);
}

static uint64_t read_int64(void const* buffer)
{
  unsigned char* p = (unsigned char*)buffer;
  return ((uint64_t)(read_int32(p)) << 32) + read_int32(p + 4);
}

static void write_int64(void* outbuffer, uint64_t value)
{
  unsigned char* p = (unsigned char*)outbuffer;
  write_int32(p + 0, (uint32_t)(value >> 32));
  write_int32(p + 4, (uint32_t)(value >>  0));
}

struct atom_t
{
  unsigned char type_[4];
  unsigned int size_;
  unsigned char* start_;
  unsigned char* end_;
};

#define ATOM_PREAMBLE_SIZE 8

static unsigned int atom_header_size(unsigned char* atom_bytes)
{
  return (atom_bytes[0] << 24) +
         (atom_bytes[1] << 16) +
         (atom_bytes[2] << 8) +
         (atom_bytes[3]);
}

static unsigned char* atom_read_header(unsigned char* buffer, struct atom_t* atom)
{
  atom->start_ = buffer;
  memcpy(&atom->type_[0], &buffer[4], 4);
  atom->size_ = atom_header_size(buffer);
  atom->end_ = atom->start_ + atom->size_;

  return buffer + ATOM_PREAMBLE_SIZE;
}

static unsigned char* atom_skip(unsigned char* buffer, struct atom_t const* atom)
{
  return atom->end_;
}

static int atom_is(struct atom_t const* atom, const char* type)
{
  return (atom->type_[0] == type[0] &&
          atom->type_[1] == type[1] &&
          atom->type_[2] == type[2] &&
          atom->type_[3] == type[3])
         ;
}

static void atom_print(struct atom_t const* atom)
{
  printf("Atom(%c%c%c%c,%d)\n", atom->type_[0], atom->type_[1],
          atom->type_[2], atom->type_[3], atom->size_);
}

#define MAX_TRACKS 8

unsigned int stts_get_entries(unsigned char const* stts)
{
  return read_int32(stts + 4);
}

void stts_get_sample_count_and_duration(unsigned char const* stts,
  unsigned int idx, unsigned int* sample_count, unsigned int* sample_duration)
{
  unsigned char const* table = stts + 8 + idx * 8;
  *sample_count = read_int32(table);
  *sample_duration = read_int32(table + 4);
}

struct stts_table_t
{
  uint32_t sample_count_;
  uint32_t sample_duration_;
};

unsigned int ctts_get_entries(unsigned char const* ctts)
{
  return read_int32(ctts + 4);
}

void ctts_get_sample_count_and_offset(unsigned char const* ctts,
  unsigned int idx, unsigned int* sample_count, unsigned int* sample_offset)
{
  unsigned char const* table = ctts + 8 + idx * 8;
  *sample_count = read_int32(table);
  *sample_offset = read_int32(table + 4);
}

unsigned int ctts_get_samples(unsigned char const* ctts)
{
  unsigned int samples = 0;
  unsigned int entries = ctts_get_entries(ctts);
  unsigned int i;
  for(i = 0; i != entries; ++i)
  {
    unsigned int sample_count;
    unsigned int sample_offset;
    ctts_get_sample_count_and_offset(ctts, i, &sample_count, &sample_offset);
    samples += sample_count;
  }

  return samples;
}

struct ctts_table_t
{
  uint32_t sample_count_;
  uint32_t sample_offset_;
};

struct stsc_table_t
{
  uint32_t chunk_;
  uint32_t samples_;
  uint32_t id_;
};

unsigned int stsc_get_entries(unsigned char const* stsc)
{
  return read_int32(stsc + 4);
}

void stsc_get_table(unsigned char const* stsc, unsigned int i, struct stsc_table_t *stsc_table)
{
  struct stsc_table_t* table = (struct stsc_table_t*)(stsc + 8);
  stsc_table->chunk_ = read_int32(&table[i].chunk_) - 1;
  stsc_table->samples_ = read_int32(&table[i].samples_);
  stsc_table->id_ = read_int32(&table[i].id_);
}

unsigned int stsc_get_chunk(unsigned char* stsc, unsigned int sample)
{
  unsigned int entries = read_int32(stsc + 4);
  struct stsc_table_t* table = (struct stsc_table_t*)(stsc + 8);

  if(entries == 0)
  {
    return 0;
  }
  else
//  if(entries == 1)
//  {
//    unsigned int table_samples = read_int32(&table[0].samples_);
//    unsigned int chunk = (sample + 1) / table_samples;
//    return chunk - 1;
//  }
//  else
  {
    unsigned int total = 0;
    unsigned int chunk1 = 1;
    unsigned int chunk1samples = 0;
    unsigned int chunk2entry = 0;
    unsigned int chunk, chunk_sample;
  
    do
    {
      unsigned int range_samples;
      unsigned int chunk2 = read_int32(&table[chunk2entry].chunk_);
      chunk = chunk2 - chunk1;
      range_samples = chunk * chunk1samples;

      if(sample < total + range_samples)
        break;

      chunk1samples = read_int32(&table[chunk2entry].samples_);
      chunk1 = chunk2;

      if(chunk2entry < entries)
      {
        chunk2entry++;
        total += range_samples;
      }
    } while(chunk2entry < entries);

    if(chunk1samples)
    {
      unsigned int sample_in_chunk = (sample - total) % chunk1samples;
      if(sample_in_chunk != 0)
      {
        printf("ERROR: sample must be chunk aligned: %d\n", sample_in_chunk);
      }
      chunk = (sample - total) / chunk1samples + chunk1;
    }
    else
      chunk = 1;

    chunk_sample = total + (chunk - chunk1) * chunk1samples;

    return chunk;
  }
}

unsigned int stsc_get_samples(unsigned char* stsc)
{
  unsigned int entries = read_int32(stsc + 4);
  struct stsc_table_t* table = (struct stsc_table_t*)(stsc + 8);
  unsigned int samples = 0;
  unsigned int i;
  for(i = 0; i != entries; ++i)
  {
    samples += read_int32(&table[i].samples_);
  }
  return samples;
}

unsigned int stco_get_entries(unsigned char const* stco)
{
  return read_int32(stco + 4);
}

uint32_t stco_get_offset(unsigned char const* stco, int idx)
{
  uint32_t const* table = (uint32_t const*)(stco + 8);
  return read_int32(&table[idx]);
}

#if 0
void stco_erase(unsigned char* stco, unsigned int chunks_to_delete)
{
  int entries = read_int32(stco + 4);
  long* table = (long*)(stco + 8);
//  unsigned int bytes_to_skip;

//  // TODO: remove up to the last chunk. This is problematic as we don't know
//  // the size of the last chunk. For now, we leave the last chunk in 'mdat'.
//  unsigned int stco_end = chunks_to_delete;
//  if(stco_end == entries && entries)
//    --stco_end;

//  bytes_to_skip = read_int32(&table[stco_end]) - read_int32(&table[0]);

  memmove(&table[0], &table[chunks_to_delete],
          (entries - chunks_to_delete) * sizeof(long));

  write_int32(stco + 4, entries - chunks_to_delete);

//  return bytes_to_skip;
}
#endif

unsigned int stsz_get_sample_size(unsigned char const* stsz)
{
  return read_int32(stsz + 4);
}

unsigned int stsz_get_entries(unsigned char const* stsz)
{
  return read_int32(stsz + 8);
}

unsigned int stsz_get_size(unsigned char const* stsz, unsigned int idx)
{
  uint32_t const* table = (uint32_t const*)(stsz + 12);
  return read_int32(&table[idx]);
}

uint64_t stts_get_duration(unsigned char const* stts)
{
  long duration = 0;
  unsigned int entries = stts_get_entries(stts);
  unsigned int i;
  for(i = 0; i != entries; ++i)
  {
    unsigned int sample_count;
    unsigned int sample_duration;
    stts_get_sample_count_and_duration(stts, i,
                                       &sample_count, &sample_duration);
    duration += sample_duration * sample_count;
  }

  return duration;
}

unsigned int stts_get_samples(unsigned char const* stts)
{
  unsigned int samples = 0;
  unsigned int entries = stts_get_entries(stts);
  unsigned int i;
  for(i = 0; i != entries; ++i)
  {
    unsigned int sample_count;
    unsigned int sample_duration;
    stts_get_sample_count_and_duration(stts, i,
                                       &sample_count, &sample_duration);
    samples += sample_count;
  }

  return samples;
}

unsigned int stts_get_sample(unsigned char const* stts, unsigned int time)
{
  unsigned int stts_index = 0;
  unsigned int stts_count;

  unsigned int ret = 0;
  unsigned int time_count = 0;

  unsigned int entries = stts_get_entries(stts);
  for(; stts_index != entries; ++stts_index)
  {
    unsigned int sample_count;
    unsigned int sample_duration;
    stts_get_sample_count_and_duration(stts, stts_index,
                                       &sample_count, &sample_duration);
    if(time_count + sample_duration * sample_count >= time)
    {
      stts_count = (time - time_count) / sample_duration;
      time_count += stts_count * sample_duration;
      ret += stts_count;
      break;
    }
    else
    {
      time_count += sample_duration * sample_count;
      ret += sample_count;
//      stts_index++;
    }
//    if(stts_index >= table_.size())
//      break;
  }
//  *time = time_count;
  return ret;
}

unsigned int stts_get_time(unsigned char const* stts, unsigned int sample)
{
  unsigned int ret = 0;
  unsigned int stts_index = 0;
  unsigned int sample_count = 0;
  
  for(;;)
  {
    unsigned int table_sample_count;
    unsigned int table_sample_duration;
    stts_get_sample_count_and_duration(stts, stts_index,
                                       &table_sample_count, &table_sample_duration);

    if(sample_count + table_sample_count > sample)
    {
      unsigned int stts_count = (sample - sample_count);
      ret += stts_count * table_sample_duration;
      break;
    }
    else
    {
      sample_count += table_sample_count;
      ret += table_sample_count * table_sample_duration;
      stts_index++;
    }
  }
  return ret;
}


struct stbl_t
{
  unsigned char* start_;
  unsigned char* stts_;     // decoding time-to-sample
  unsigned char* stss_;     // sync sample
  unsigned char* stsc_;     // sample-to-chunk
  unsigned char* stsz_;     // sample size
  unsigned char* stco_;     // chunk offset
  unsigned char* ctts_;     // composition time-to-sample

  unsigned char* new_;      // the newly generated stbl
  unsigned char* newp_;     // the newly generated stbl
};

void stbl_parse(struct stbl_t* stbl, unsigned char* buffer, unsigned int size)
{
  struct atom_t leaf_atom;
  unsigned char* buffer_start = buffer;
  stbl->stss_ = 0;
  stbl->ctts_ = 0;

  stbl->start_ = buffer;
  stbl->new_ = malloc(size);
  stbl->newp_ = stbl->new_;
  
  while(buffer < buffer_start + size)
  {
    buffer = atom_read_header(buffer, &leaf_atom);

    atom_print(&leaf_atom);

    if(atom_is(&leaf_atom, "stts"))
    {
      stbl->stts_ = buffer;
    }
    else
    if(atom_is(&leaf_atom, "stss"))
    {
      stbl->stss_ = buffer;
    }
    else
    if(atom_is(&leaf_atom, "stsc"))
    {
      stbl->stsc_ = buffer;
    }
    else
    if(atom_is(&leaf_atom, "stsz"))
    {
      stbl->stsz_ = buffer;
    }
    else
    if(atom_is(&leaf_atom, "stco"))
    {
      stbl->stco_ = buffer;
    }
    else
    if(atom_is(&leaf_atom, "co64"))
    {
      printf("TODO: co64");
    }
    else
    if(atom_is(&leaf_atom, "ctts"))
    {
      stbl->ctts_ = buffer;
    }
    else
    {
      // copy unknown/unused atoms directly (e.g. stsd)
      memcpy(stbl->newp_, buffer - ATOM_PREAMBLE_SIZE, leaf_atom.size_);
      stbl->newp_ += leaf_atom.size_;
    }

    buffer = atom_skip(buffer, &leaf_atom);
  }
}

struct minf_t
{
  unsigned char* start_;
  struct stbl_t stbl_;
};

void minf_parse(struct minf_t* minf, unsigned char* buffer, unsigned int size)
{
  struct atom_t leaf_atom;
  unsigned char* buffer_start = buffer;

  minf->start_ = buffer;
  
  while(buffer < buffer_start + size)
  {
    buffer = atom_read_header(buffer, &leaf_atom);

    atom_print(&leaf_atom);

    if(atom_is(&leaf_atom, "stbl"))
    {
      stbl_parse(&minf->stbl_, buffer, leaf_atom.size_ - ATOM_PREAMBLE_SIZE);
    }

    buffer = atom_skip(buffer, &leaf_atom);
  }
}

struct mdia_t
{
  unsigned char* start_;
  unsigned char* mdhd_;
  struct minf_t minf_;
//  hdlr hdlr_;
};

void mdia_parse(struct mdia_t* mdia, unsigned char* buffer, unsigned int size)
{
  struct atom_t leaf_atom;
  unsigned char* buffer_start = buffer;

  mdia->start_ = buffer;
  
  while(buffer < buffer_start + size)
  {
    buffer = atom_read_header(buffer, &leaf_atom);

    atom_print(&leaf_atom);

    if(atom_is(&leaf_atom, "mdhd"))
    {
      mdia->mdhd_ = buffer;
    }
    else
    if(atom_is(&leaf_atom, "minf"))
    {
      minf_parse(&mdia->minf_, buffer, leaf_atom.size_ - ATOM_PREAMBLE_SIZE);
    }

    buffer = atom_skip(buffer, &leaf_atom);
  }
}

struct chunks_t
{
  unsigned int sample_;   // number of the first sample in the chunk
  unsigned int size_;     // number of samples in the chunk
  int id_;                // for multiple codecs mode - not used
  uint64_t pos_;          // start byte position of chunk
};

struct samples_t
{
  unsigned int pts_;      // decoding/presentation time
  unsigned int size_;     // size in bytes
  uint64_t pos_;          // byte offset
  unsigned int cto_;      // composition time offset
};

struct trak_t
{
  unsigned char* start_;
  unsigned char* tkhd_;
  struct mdia_t mdia_;

  /* temporary indices */
  unsigned int chunks_size_;
  struct chunks_t* chunks_;

  unsigned int samples_size_;
  struct samples_t* samples_;
};

void trak_init(struct trak_t* trak)
{
  trak->chunks_ = 0;
  trak->samples_ = 0;
}

void trak_exit(struct trak_t* trak)
{
  if(trak->chunks_)
      free(trak->chunks_);
  if(trak->samples_)
    free(trak->samples_);
}

void trak_parse(struct trak_t* trak, unsigned char* buffer, unsigned int size)
{
  struct atom_t leaf_atom;
  unsigned char* buffer_start = buffer;

  trak->start_ = buffer;
  
  while(buffer < buffer_start + size)
  {
    buffer = atom_read_header(buffer, &leaf_atom);

    atom_print(&leaf_atom);

    if(atom_is(&leaf_atom, "tkhd"))
    {
      trak->tkhd_ = buffer;
    }
    else
    if(atom_is(&leaf_atom, "mdia"))
    {
      mdia_parse(&trak->mdia_, buffer, leaf_atom.size_ - ATOM_PREAMBLE_SIZE);
    }

    buffer = atom_skip(buffer, &leaf_atom);
  }
}

struct moov_t
{
  unsigned char* start_;
  unsigned int tracks_;
  unsigned char* mvhd_;
  struct trak_t traks_[MAX_TRACKS];
};

void moov_init(struct moov_t* moov)
{
  moov->tracks_ = 0;
}

void moov_exit(struct moov_t* moov)
{
  unsigned int i;
  for(i = 0; i != moov->tracks_; ++i)
  {
    trak_exit(&moov->traks_[i]);
  }
}

void trak_build_index(struct trak_t* trak)
{
  void const* stco = trak->mdia_.minf_.stbl_.stco_;

  trak->chunks_size_ = stco_get_entries(stco);
  trak->chunks_ = malloc(trak->chunks_size_ * sizeof(struct chunks_t));

  {
    unsigned int i;
    for(i = 0; i != trak->chunks_size_; ++i)
    {
      trak->chunks_[i].pos_ = stco_get_offset(stco, i);
    }
  }

  // process chunkmap:
  {
    void const* stsc = trak->mdia_.minf_.stbl_.stsc_;
    unsigned int last = trak->chunks_size_;
    unsigned int i = stsc_get_entries(stsc);
    while(i > 0)
    {
      struct stsc_table_t stsc_table;
      unsigned int j;

      --i;

      stsc_get_table(stsc, i, &stsc_table);
      for(j = stsc_table.chunk_; j < last; j++)
      {
        trak->chunks_[j].id_ = stsc_table.id_;
        trak->chunks_[j].size_ = stsc_table.samples_;
      }
      last = stsc_table.chunk_;
    }
  }

  // calc pts of chunks:
  {
    void const* stsz = trak->mdia_.minf_.stbl_.stsz_;
    unsigned int sample_size = stsz_get_sample_size(stsz);
    unsigned int s = 0;
    {
      unsigned int j;
      for(j = 0; j < trak->chunks_size_; j++)
      {
        trak->chunks_[j].sample_ = s;
        s += trak->chunks_[j].size_;
      }
    }

    if(sample_size == 0)
    {
      trak->samples_size_ = stsz_get_entries(stsz);
    }
    else
    {
      trak->samples_size_ = s;
    }

    trak->samples_ = malloc(trak->samples_size_ * sizeof(struct samples_t));

    if(sample_size == 0)
    {
      unsigned int i;
      for(i = 0; i != trak->samples_size_ ; ++i)
        trak->samples_[i].size_ = stsz_get_size(stsz, i);
    }
    else
    {
      unsigned int i;
      for(i = 0; i != trak->samples_size_ ; ++i)
        trak->samples_[i].size_ = sample_size;
    }
  }

//  i = 0;
//  for (j = 0; j < trak->durmap_size; j++)
//    i += trak->durmap[j].num;
//  if (i != s) {
//    mp_msg(MSGT_DEMUX, MSGL_WARN,
//           "MOV: durmap and chunkmap sample count differ (%i vs %i)\n", i, s);
//    if (i > s) s = i;
//  }

  // calc pts:
  {
    void const* stts = trak->mdia_.minf_.stbl_.stts_;
    unsigned int s = 0;
    unsigned int pts = 0;
    unsigned int entries = stts_get_entries(stts);
    unsigned int j;
    for(j = 0; j < entries; j++)
    {
      unsigned int i;
      unsigned int sample_count;
      unsigned int sample_duration;
      stts_get_sample_count_and_duration(stts, j,
                                         &sample_count, &sample_duration);
      for(i = 0; i < sample_count; i++)
      {
        trak->samples_[s].pts_ = pts;
        ++s;
        pts += sample_duration;
      }
    }
  }

  // calc composition times:
  {
    void const* ctts = trak->mdia_.minf_.stbl_.ctts_;
    if(ctts)
    {
      unsigned int s = 0;
      unsigned int entries = ctts_get_entries(ctts);
      unsigned int j;
      for(j = 0; j < entries; j++)
      {
        unsigned int i;
        unsigned int sample_count;
        unsigned int sample_offset;
        ctts_get_sample_count_and_offset(ctts, j, &sample_count, &sample_offset);
        for(i = 0; i < sample_count; i++)
        {
          trak->samples_[s].cto_ = sample_offset;
          ++s;
        }
      }
    }
  }

  // calc sample offsets
  {
    unsigned int s = 0;
    unsigned int j;
    for(j = 0; j != trak->chunks_size_; j++)
    {
      uint64_t pos = trak->chunks_[j].pos_;
      unsigned int i;
      for(i = 0; i != trak->chunks_[j].size_; i++)
      {
        trak->samples_[s].pos_ = pos;
        pos += trak->samples_[s].size_;
        ++s;
      }
    }
  }
}

int trak_write_index(struct trak_t* trak, unsigned int start, unsigned int end)
{
  // write samples [start,end>

  unsigned char* newp = trak->mdia_.minf_.stbl_.newp_;

  // stts = [entries * [sample_count, sample_duration]
  {
    unsigned char* stts_atom_start = newp;
    unsigned char* stts = trak->mdia_.minf_.stbl_.stts_;

    unsigned int entries = 0;
//    struct stts_table_t const* table = (struct stts_table_t*)(stts + 8);
    unsigned int s;

    // copy header
    // atom + version + flags
    memcpy(newp, stts - ATOM_PREAMBLE_SIZE, ATOM_PREAMBLE_SIZE + 4);
    newp += ATOM_PREAMBLE_SIZE + 4;
    newp += 4;  // Number Of Entries

    for(s = start; s != end; ++s)
    {
      unsigned int sample_count = 1;
      unsigned int sample_duration =
        trak->samples_[s + 1].pts_ - trak->samples_[s].pts_;
      while(s != end - 1)
      {
        if((trak->samples_[s + 1].pts_ - trak->samples_[s].pts_) != sample_duration)
          break;
        ++sample_count;
        ++s;
      }
      // write entry
      write_int32(newp, sample_count);
      newp += 4;
      write_int32(newp, sample_duration);
      newp += 4;

      ++entries;
    }
    stts = trak->mdia_.minf_.stbl_.stts_ = stts_atom_start + ATOM_PREAMBLE_SIZE;
    write_int32(stts_atom_start + ATOM_PREAMBLE_SIZE + 4, entries);
    write_int32(stts_atom_start, newp - stts_atom_start);
    printf("Atom(%c%c%c%c,%d)\n", stts_atom_start[4], stts_atom_start[5],
            stts_atom_start[6], stts_atom_start[7], read_int32(stts_atom_start));
    if(stts_get_samples(stts_atom_start + ATOM_PREAMBLE_SIZE) != end - start)
    {
      printf("ERROR: stts_get_samples=%d, should be %d\n",
             stts_get_samples(stts_atom_start + ATOM_PREAMBLE_SIZE), end - start);
    }
  }

  // ctts = [entries * [sample_count, sample_offset]
  {
    unsigned char* ctts_atom_start = newp;
    unsigned char* ctts = trak->mdia_.minf_.stbl_.ctts_;
    if(ctts)
    {
      unsigned int entries = 0;
//      struct ctts_table_t const* table = (struct ctts_table_t*)(ctts + 8);
      unsigned int s;

      // copy header
      // atom + version + flags
      memcpy(newp, ctts - ATOM_PREAMBLE_SIZE, ATOM_PREAMBLE_SIZE + 4);
      newp += ATOM_PREAMBLE_SIZE + 4;
      newp += 4;  // Number Of Entries

      for(s = start; s != end; ++s)
      {
        unsigned int sample_count = 1;
        unsigned int sample_offset = trak->samples_[s].cto_;
        while(s != end - 1)
        {
          if(trak->samples_[s + 1].cto_ != sample_offset)
            break;
          ++sample_count;
          ++s;
        }
        // write entry
        write_int32(newp, sample_count);
        newp += 4;
        write_int32(newp, sample_offset);
        newp += 4;
        ++entries;
      }
      trak->mdia_.minf_.stbl_.ctts_ = ctts_atom_start + ATOM_PREAMBLE_SIZE;
      write_int32(ctts_atom_start + ATOM_PREAMBLE_SIZE + 4, entries);
      write_int32(ctts_atom_start, newp - ctts_atom_start);
      printf("Atom(%c%c%c%c,%d)\n", ctts_atom_start[4], ctts_atom_start[5],
              ctts_atom_start[6], ctts_atom_start[7], read_int32(ctts_atom_start));
      if(ctts_get_samples(ctts_atom_start + ATOM_PREAMBLE_SIZE) != end - start)
      {
        printf("ERROR: ctts_get_samples=%d, should be %d\n",
               ctts_get_samples(ctts_atom_start + ATOM_PREAMBLE_SIZE), end - start);
      }
    }
  }

  // process chunkmap:
  {
    unsigned char* stsc_atom_start = newp;
    unsigned char* stsc = trak->mdia_.minf_.stbl_.stsc_;
//    struct stsc_table_t const* stsc_table = (struct stsc_table_t*)(stsc + 8);
    unsigned int i;

    // copy header
    // atom + version + flags
    memcpy(newp, stsc - ATOM_PREAMBLE_SIZE, ATOM_PREAMBLE_SIZE + 4);
    newp += ATOM_PREAMBLE_SIZE + 4;
    newp += 4;  // Number Of Entries

    for(i = 0; i != trak->chunks_size_; ++i)
    {
      if(trak->chunks_[i].sample_ + trak->chunks_[i].size_ > start)
        break;
    }

    {
      unsigned int stsc_entries = 0;
      unsigned int chunk_start = i;
      unsigned int chunk_end;
      // problem.mp4: reported by Jin-seok Lee. Second track contains no samples
      if(trak->chunks_size_ != 0)
      {
        unsigned int samples =
          trak->chunks_[i].sample_ + trak->chunks_[i].size_ - start;
        unsigned int id = trak->chunks_[i].id_;

        // write entry [chunk,samples,id]
        write_int32(newp, 1);
          newp += 4;
        write_int32(newp, samples);
        newp += 4;
        write_int32(newp, id);
        newp += 4;
        ++stsc_entries;
        if(i != trak->chunks_size_)
        {
          for(i += 1; i != trak->chunks_size_; ++i)
          {
            if(trak->chunks_[i].sample_ >= end)
              break;

            if(trak->chunks_[i].size_ != samples)
            {
              samples = trak->chunks_[i].size_;
              id = trak->chunks_[i].id_;
              write_int32(newp, i - chunk_start + 1);
              newp += 4;
              write_int32(newp, samples);
              newp += 4;
              write_int32(newp, id);
              newp += 4;
              ++stsc_entries;
            }
          }
        }
      }
      chunk_end = i;
      trak->mdia_.minf_.stbl_.stsc_ = stsc_atom_start + ATOM_PREAMBLE_SIZE;
      write_int32(stsc_atom_start + ATOM_PREAMBLE_SIZE + 4, stsc_entries);
      write_int32(stsc_atom_start, newp - stsc_atom_start);
      printf("Atom(%c%c%c%c,%d)\n", stsc_atom_start[4], stsc_atom_start[5],
              stsc_atom_start[6], stsc_atom_start[7], read_int32(stsc_atom_start));

      {
        unsigned char* stco_atom_start = newp;
        unsigned char const* stco = trak->mdia_.minf_.stbl_.stco_;
        unsigned int entries = chunk_end;
        uint32_t const* stco_table = (uint32_t*)(stco + 8);

        // copy header
        // atom + version + flags
        memcpy(newp, stco - ATOM_PREAMBLE_SIZE, ATOM_PREAMBLE_SIZE + 4);
        newp += ATOM_PREAMBLE_SIZE + 4;
        newp += 4;  // Number Of Entries

        for(i = chunk_start; i != entries; ++i)
        {
          write_int32(newp, read_int32(&stco_table[i]));
          newp += 4;
        }
        trak->mdia_.minf_.stbl_.stco_ = stco_atom_start + ATOM_PREAMBLE_SIZE;
        write_int32(stco_atom_start + ATOM_PREAMBLE_SIZE + 4, entries - chunk_start);
        write_int32(stco_atom_start, newp - stco_atom_start);
        printf("Atom(%c%c%c%c,%d)\n", stco_atom_start[4], stco_atom_start[5],
                stco_atom_start[6], stco_atom_start[7], read_int32(stco_atom_start));

        // patch first chunk with correct sample offset
        write_int32(stco_atom_start + ATOM_PREAMBLE_SIZE + 8,
                    (uint32_t)trak->samples_[start].pos_);
      }
    }
  }

  // process sync samples:
  if(trak->mdia_.minf_.stbl_.stss_)
  {
    unsigned char* stss_atom_start = newp;
    unsigned char* stss = trak->mdia_.minf_.stbl_.stss_;
    unsigned int entries = read_int32(stss + 4);
    uint32_t const* table = (uint32_t*)(stss + 8);
    unsigned int stss_start;
    unsigned int i;

    // copy header
    // atom + version + flags
    memcpy(newp, stss - ATOM_PREAMBLE_SIZE, ATOM_PREAMBLE_SIZE + 4);
    newp += ATOM_PREAMBLE_SIZE + 4;
    newp += 4;  // Number Of Entries

    for(i = 0; i != entries; ++i)
    {
      if(read_int32(&table[i]) >= start + 1)
        break;
    }
    stss_start = i;
    for(; i != entries; ++i)
    {
      unsigned int sync_sample = read_int32(&table[i]);
      if(sync_sample >= end + 1)
        break;
      write_int32(newp, sync_sample - start);
      newp += 4;
    }
    trak->mdia_.minf_.stbl_.stss_ = stss_atom_start + ATOM_PREAMBLE_SIZE;
    write_int32(stss_atom_start + ATOM_PREAMBLE_SIZE + 4, i - stss_start);
    write_int32(stss_atom_start, newp - stss_atom_start);
    printf("Atom(%c%c%c%c,%d)\n", stss_atom_start[4], stss_atom_start[5],
            stss_atom_start[6], stss_atom_start[7], read_int32(stss_atom_start));
  }

  // process sample sizes
  {
    unsigned char* stsz_atom_start = newp;
    unsigned char* stsz = trak->mdia_.minf_.stbl_.stsz_;

    // copy header
    // atom + version + flags, sample_size, number_of_etries
    memcpy(newp, stsz - ATOM_PREAMBLE_SIZE, ATOM_PREAMBLE_SIZE + 12);
    newp += ATOM_PREAMBLE_SIZE + 12;

    if(stsz_get_sample_size(stsz) == 0)
    {
      uint32_t const* table = (uint32_t*)(stsz + 12);
      memcpy(newp, &table[start], (end - start) * sizeof(uint32_t));
      newp += (end - start) * sizeof(uint32_t);
      write_int32(stsz_atom_start + ATOM_PREAMBLE_SIZE + 8, end - start);
    }
    trak->mdia_.minf_.stbl_.stsz_ = stsz_atom_start + ATOM_PREAMBLE_SIZE;
    write_int32(stsz_atom_start, newp - stsz_atom_start);
    printf("Atom(%c%c%c%c,%d)\n", stsz_atom_start[4], stsz_atom_start[5],
            stsz_atom_start[6], stsz_atom_start[7], read_int32(stsz_atom_start));
  }

  trak->mdia_.minf_.stbl_.newp_ = newp;

  // copy newly generated stbl over old one
  {
    struct stbl_t* stbl = &trak->mdia_.minf_.stbl_;

    unsigned int old_stbl_size = read_int32(stbl->start_ - ATOM_PREAMBLE_SIZE);
    unsigned int new_stbl_size = stbl->newp_ - stbl->new_ + ATOM_PREAMBLE_SIZE;
    printf("Atom(stbl,old=%d,new=%d)\n", old_stbl_size, new_stbl_size);

    if(new_stbl_size > old_stbl_size)
    {
      printf("new meta data larger than old: error");
      return 0;
    }

    memcpy(stbl->start_, stbl->new_, new_stbl_size - ATOM_PREAMBLE_SIZE);
//    write_int32(stbl->start_ - ATOM_PREAMBLE_SIZE, new_stbl_size);

    // relocate stbl pointers
    if(stbl->stts_)
      stbl->stts_ += stbl->start_ - stbl->new_;
    if(stbl->stss_)
      stbl->stss_ += stbl->start_ - stbl->new_;
    if(stbl->stsc_)
      stbl->stsc_ += stbl->start_ - stbl->new_;
    if(stbl->stsz_)
      stbl->stsz_ += stbl->start_ - stbl->new_;
    if(stbl->stco_)
      stbl->stco_ += stbl->start_ - stbl->new_;
    if(stbl->ctts_)
      stbl->ctts_ += stbl->start_ - stbl->new_;

    free(trak->mdia_.minf_.stbl_.new_);
    trak->mdia_.minf_.stbl_.new_= 0;

    // add free atom for left over
    if(new_stbl_size < old_stbl_size - ATOM_PREAMBLE_SIZE)
    {
      unsigned int free_size = old_stbl_size - new_stbl_size;
      unsigned char* free_atom = stbl->start_ + new_stbl_size - ATOM_PREAMBLE_SIZE;
      write_int32(free_atom, free_size);
      free_atom[4] = 'f';
      free_atom[5] = 'r';
      free_atom[6] = 'e';
      free_atom[7] = 'e';
      {
        const char free_bytes[8] =
        {
          'C', 'o', 'd', 'e','S','h', 'o', 'p'
        };
        uint32_t padding_index;
        for(padding_index = ATOM_PREAMBLE_SIZE; padding_index != free_size; ++padding_index)
        {
          free_atom[padding_index] = free_bytes[padding_index % 8];
        }
      }
    }
  }

  return 1;
}

int moov_parse(struct moov_t* moov, unsigned char* buffer, uint64_t size)
{
  struct atom_t leaf_atom;
  unsigned char* buffer_start = buffer;

  moov->start_ = buffer;
  
  while(buffer < buffer_start + size)
  {
    buffer = atom_read_header(buffer, &leaf_atom);

    atom_print(&leaf_atom);

    if(atom_is(&leaf_atom, "cmov"))
    {
      return 0;
    }
    else
    if(atom_is(&leaf_atom, "mvhd"))
    {
      moov->mvhd_ = buffer;
    }
    else
    if(atom_is(&leaf_atom, "trak"))
    {
      if(moov->tracks_ == MAX_TRACKS)
        return 0;
      else
      {
        struct trak_t* trak = &moov->traks_[moov->tracks_];
        trak_init(trak);
        trak_parse(trak, buffer, leaf_atom.size_ - ATOM_PREAMBLE_SIZE);
        ++moov->tracks_;
      }
    }
    buffer = atom_skip(buffer, &leaf_atom);
  }

  // build the indexing tables
  {
    unsigned int i;
    for(i = 0; i != moov->tracks_; ++i)
    {
      trak_build_index(&moov->traks_[i]);
    }
  }

  return 1;
}

void stco_shift_offsets(unsigned char* stco, int offset)
{
  unsigned int entries = read_int32(stco + 4);
  unsigned int* table = (unsigned int*)(stco + 8);
  unsigned int i;
  for(i = 0; i != entries; ++i)
    write_int32(&table[i], (read_int32(&table[i]) + offset));
}

void trak_shift_offsets(struct trak_t* trak, int64_t offset)
{
  unsigned char* stco = trak->mdia_.minf_.stbl_.stco_;
  stco_shift_offsets(stco, (int32_t)offset);
}

void moov_shift_offsets(struct moov_t* moov, int64_t offset)
{
  unsigned int i;
  for(i = 0; i != moov->tracks_; ++i)
  {
    trak_shift_offsets(&moov->traks_[i], offset);
  }
}

long mvhd_get_time_scale(unsigned char* mvhd)
{
  int version = read_char(mvhd);
  unsigned char* p = mvhd + (version == 0 ? 12 : 20);
  return read_int32(p);
}

void mvhd_set_duration(unsigned char* mvhd, uint64_t duration)
{
  int version = read_char(mvhd);
  if(version == 0)
  {
    write_int32(mvhd + 16, (uint32_t)duration);
  }
  else
  {
    write_int64(mvhd + 24, duration);
  }
}

long mdhd_get_time_scale(unsigned char* mdhd)
{
  int version = read_char(mdhd);
  unsigned char* p = mdhd + (version == 0 ? 12 : 20);

  return read_int32(p);
}

uint64_t mdhd_get_duration(unsigned char* mdhd)
{
  int version = read_char(mdhd);
  if(version == 0)
  {
    return read_int32(mdhd + 16);
  }
  else
  {
    return read_int64(mdhd + 24);
  }
}

void mdhd_set_duration(unsigned char* mdhd, uint64_t duration)
{
  int version = read_char(mdhd);
  if(version == 0)
  {
    write_int32(mdhd + 16, (uint32_t)duration);
  }
  else
  {
    write_int64(mdhd + 24, duration);
  }
}

void tkhd_set_duration(unsigned char* tkhd, uint64_t duration)
{
  int version = read_char(tkhd);
  if(version == 0)
  {
    write_int32(tkhd + 20, (uint32_t)duration);
  }
  else
  {
    write_int64(tkhd + 28, duration);
  }
}

unsigned int stss_get_entries(unsigned char const* stss)
{
  return read_int32(stss + 4);
}

long stss_get_sample(unsigned char const* stss, unsigned int idx)
{
  unsigned char const* p = stss + 8 + idx * 4;
  return read_int32(p);
}

unsigned int stss_get_nearest_keyframe(unsigned char const* stss, unsigned int sample)
{
  // scan the sync samples to find the key frame that precedes the sample number
  unsigned int i;
  unsigned int entries = stss_get_entries(stss);
  unsigned int table_sample = 0;
  for(i = 0; i != entries; ++i)
  {
    table_sample = stss_get_sample(stss, i);
    if(table_sample >= sample)
      break;
  }
  if(table_sample == sample)
    return table_sample;
  else
    return stss_get_sample(stss, i - 1);
}

unsigned int stbl_get_nearest_keyframe(struct stbl_t const* stbl, unsigned int sample)
{
  // If the sync atom is not present, all samples are implicit sync samples.
  if(!stbl->stss_)
    return sample;

  return stss_get_nearest_keyframe(stbl->stss_, sample);
}

unsigned int moov_seek(unsigned char* moov_data,
                       uint64_t* moov_size,
                       float start_time,
                       float end_time,
                       uint64_t* mdat_start,
                       uint64_t* mdat_size,
                       uint64_t offset)
{
  struct moov_t* moov = malloc(sizeof(struct moov_t));
  moov_init(moov);
  if(!moov_parse(moov, moov_data + ATOM_PREAMBLE_SIZE, *moov_size - ATOM_PREAMBLE_SIZE))
  {
    moov_exit(moov);
    free(moov);
    return 0;
  }

  {
    long moov_time_scale = mvhd_get_time_scale(moov->mvhd_);
    unsigned int start = (unsigned int)(start_time * moov_time_scale);
    unsigned int end = (unsigned int)(end_time * moov_time_scale);
    uint64_t skip_from_start = UINT64_MAX;
    uint64_t end_offset = 0;
    unsigned int i;
    unsigned int pass;

    // for every trak, convert seconds to sample (time-to-sample).
    // adjust sample to keyframe
    unsigned int trak_sample_start[MAX_TRACKS];
    unsigned int trak_sample_end[MAX_TRACKS];

    uint64_t moov_duration = 0;

    // clayton.mp4 has a third track with one sample that lasts the whole clip.
    // Assuming the first two tracks are the audio and video track, we patch
    // the remaining tracks to 'free' atoms.
    if(moov->tracks_ > 2)
    {
      for(i = 2; i != moov->tracks_; ++i)
      {
        // patch 'trak' to 'free'
        unsigned char* p = moov->traks_[i].start_ - 4;
        p[0] = 'f';
        p[1] = 'r';
        p[2] = 'e';
        p[3] = 'e';
      }
      moov->tracks_ = 2;
    }

    // reported by everwanna:
    // av out of sync because: 
    // audio track 0 without stss, seek to the exact time. 
    // video track 1 with stss, seek to the nearest key frame time.
    //
    // fixed:
    // first pass we get the new aligned times for traks with an stss present
    // second pass is for traks without an stss
    for(pass = 0; pass != 2; ++pass)
    {
      for(i = 0; i != moov->tracks_; ++i)
      {
        struct trak_t* trak = &moov->traks_[i];
        struct stbl_t* stbl = &trak->mdia_.minf_.stbl_;
        long trak_time_scale = mdhd_get_time_scale(trak->mdia_.mdhd_);
        float moov_to_trak_time = (float)trak_time_scale / (float)moov_time_scale;
        float trak_to_moov_time = (float)moov_time_scale / (float)trak_time_scale;

        // 1st pass: stss present, 2nd pass: no stss present
        if(pass == 0 && !stbl->stss_)
          continue;
        if(pass == 1 && stbl->stss_)
          continue;

        // ignore empty track
        if(mdhd_get_duration(trak->mdia_.mdhd_) == 0)
          continue;

        // get start
        if(start == 0)
        {
          trak_sample_start[i] = start;
        }
        else
        {
          start = stts_get_sample(stbl->stts_, (unsigned int)(start * moov_to_trak_time));
          printf("start=%u (trac time)=%.2f (seconds)", start,
            stts_get_time(stbl->stts_, start) / (float)trak_time_scale);
          start = stbl_get_nearest_keyframe(stbl, start + 1) - 1;
          printf("=%u (zero based keyframe)", start);
          trak_sample_start[i] = start;
          start = (unsigned int)(stts_get_time(stbl->stts_, start) * trak_to_moov_time);
          printf("=%u (moov time)\n", start);
        }

        // get end
        if(end == 0)
        {
          trak_sample_end[i] = trak->samples_size_;
        }
        else
        {
          end = stts_get_sample(stbl->stts_, (unsigned int)(end * moov_to_trak_time));
          if(end >= trak->samples_size_)
          {
            end = trak->samples_size_;
          }
          else
          {
            end = stbl_get_nearest_keyframe(stbl, end + 1) - 1;
          }
          trak_sample_end[i] = end;
          printf("endframe=%u, samples_size_=%u\n", end, trak->samples_size_);
          end = (unsigned int)(stts_get_time(stbl->stts_, end) * trak_to_moov_time);
        }
      }
    }

    printf("start=%u\n", start);
    printf("end=%u\n", end);

    if(end && start >= end)
      return 0;

    for(i = 0; i != moov->tracks_; ++i)
    {
      struct trak_t* trak = &moov->traks_[i];
      struct stbl_t* stbl = &trak->mdia_.minf_.stbl_;

      unsigned int start_sample = trak_sample_start[i];
      unsigned int end_sample = trak_sample_end[i];

      // ignore empty track
      if(mdhd_get_duration(trak->mdia_.mdhd_) == 0)
        continue;

      if(!trak_write_index(trak, start_sample, end_sample))
        return 0;

      {
        uint64_t skip =
          trak->samples_[start_sample].pos_ - trak->samples_[0].pos_;
        if(skip < skip_from_start)
          skip_from_start = skip;
        printf("Trak can skip %llu bytes\n", skip);

        if(end_sample != trak->samples_size_)
        {
          uint64_t end_pos = trak->samples_[end_sample].pos_;
          if(end_pos > end_offset)
            end_offset = end_pos;
          printf("New endpos=%llu\n", end_pos);
          printf("Trak can skip %llu bytes at end\n",
                 *mdat_start + *mdat_size - end_offset);
        }
      }

      {
        // fixup trak (duration)
        uint64_t trak_duration = stts_get_duration(stbl->stts_);
        long trak_time_scale = mdhd_get_time_scale(trak->mdia_.mdhd_);
        float trak_to_moov_time = (float)moov_time_scale / (float)trak_time_scale;
        uint64_t duration = (long)((float)trak_duration * trak_to_moov_time);
        mdhd_set_duration(trak->mdia_.mdhd_, trak_duration);
        tkhd_set_duration(trak->tkhd_, duration);
        printf("trak: new_duration=%lld\n", duration);

        if(duration > moov_duration)
          moov_duration = duration;
      }

      printf("stco.size=%d, ", read_int32(stbl->stco_ + 4));
      printf("stts.size=%d samples=%d\n", read_int32(stbl->stts_ + 4), stts_get_samples(stbl->stts_));
      printf("stsz.size=%d\n", read_int32(stbl->stsz_ + 8));
      printf("stsc.samples=%d\n", stsc_get_samples(stbl->stsc_));
    }
    mvhd_set_duration(moov->mvhd_, moov_duration);

    offset -= skip_from_start;

    printf("shifting offsets by %lld\n", offset);
    moov_shift_offsets(moov, offset);

#ifdef COMPRESS_MOOV_ATOM
    {
      uLong sourceLen = *moov_size - ATOM_PREAMBLE_SIZE;
      uLong destLen = compressBound(sourceLen);
      unsigned char* cmov = malloc(destLen);
      int zstatus = compress(cmov, &destLen, moov_data, sourceLen);
      if(zstatus == Z_OK)
      {
        printf("cmov size = %lu (%ld%%)\n", destLen, 100 * destLen / sourceLen);
      }

      {
        const int extra_space = 4096;
        if(destLen + extra_space < sourceLen)
        {
          const int bytes_saved = sourceLen - destLen;
          uLong destLen2;
          int extra = 0;
          printf("shifting offsets by %d\n", -bytes_saved);
          moov_shift_offsets(moov, -bytes_saved);

          extra += ATOM_PREAMBLE_SIZE + 4;            // dcom
          extra += ATOM_PREAMBLE_SIZE + 4;            // cmvd
          extra += ATOM_PREAMBLE_SIZE;                // cmov
          extra += ATOM_PREAMBLE_SIZE + extra_space;  // free

          printf("shifting offsets by %d\n", extra);
          moov_shift_offsets(moov, extra);

          // recompress
          destLen2 = compressBound(sourceLen);
          zstatus = compress(cmov, &destLen2, moov_data, sourceLen);
          if(zstatus == Z_OK)
          {
            printf("cmov size = %lu (%ld%%)\n", destLen2, 100 * destLen2 / sourceLen);

            if(destLen2 < destLen + extra_space)
            {
              // copy compressed movie atom
              unsigned char* outbuffer = moov_data;

              uint32_t dcom_size = ATOM_PREAMBLE_SIZE + 4;
              uint32_t cmvd_size = ATOM_PREAMBLE_SIZE + 4 + destLen2;
              uint32_t cmov_size = ATOM_PREAMBLE_SIZE + dcom_size + cmvd_size;
              uint32_t free_size = ATOM_PREAMBLE_SIZE + extra_space + destLen - destLen2;
              *moov_size = ATOM_PREAMBLE_SIZE + cmov_size + free_size;

              write_int32(outbuffer, (uint32_t)*moov_size);
              outbuffer += 4;

              // skip 'moov'
              outbuffer += 4;

              write_int32(outbuffer, cmov_size);
              outbuffer += 4;
              {
                outbuffer[0] = 'c';
                outbuffer[1] = 'm';
                outbuffer[2] = 'o';
                outbuffer[3] = 'v';
                outbuffer += 4;

                write_int32(outbuffer, dcom_size);
                outbuffer += 4;
                {
                  outbuffer[0] = 'd';
                  outbuffer[1] = 'c';
                  outbuffer[2] = 'o';
                  outbuffer[3] = 'm';
                  outbuffer += 4;

                  outbuffer[0] = 'z';
                  outbuffer[1] = 'l';
                  outbuffer[2] = 'i';
                  outbuffer[3] = 'b';
                  outbuffer += 4;
                }

                write_int32(outbuffer, cmvd_size);
                outbuffer += 4;
                {
                  outbuffer[0] = 'c';
                  outbuffer[1] = 'm';
                  outbuffer[2] = 'v';
                  outbuffer[3] = 'd';
                  outbuffer += 4;

                  write_int32(outbuffer, sourceLen);
                  outbuffer += 4;

                  memcpy(outbuffer, cmov, destLen2);
                  outbuffer += destLen2;
                }
              }

              // add final padding
              write_int32(outbuffer, free_size);
              outbuffer += 4;

              outbuffer[0] = 'f';
              outbuffer[1] = 'r';
              outbuffer[2] = 'e';
              outbuffer[3] = 'e';
              outbuffer += 4;

              {
                const char free_bytes[8] =
                {
                  'C', 'o', 'd', 'e','S','h', 'o', 'p'
                };
                uint32_t padding_index;
                for(padding_index = ATOM_PREAMBLE_SIZE; padding_index != free_size; ++padding_index)
                {
                  outbuffer[padding_index] = free_bytes[padding_index % 8];
                }
              }
            }
            else
            {
              printf("2nd pass compress overflow\n");
            }
          }
        }
      }
      free(cmov);
    }
#endif

    *mdat_start += skip_from_start;
    if(end_offset != 0)
    {
      *mdat_size = end_offset;
    }
    *mdat_size -= skip_from_start;
  }

  moov_exit(moov);
  free(moov);

  return 1;
}

// End Of File

