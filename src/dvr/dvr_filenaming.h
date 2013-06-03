/*
 *  Digital Video Recorder
 *  Copyright (C) 2013 Petri Posio
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef DVR_FILENAMING_H
#define DVR_FILENAMING_H

#include "tvheadend.h"

struct dvr_filename_scheme_advanced_list
{
  char *source;
  char *regex;
  LIST_ENTRY(dvr_filename_scheme_advanced_list) _link;
};

typedef struct dvr_filename_scheme_advanced
{
  char *filename_format;
  LIST_HEAD(, dvr_filename_scheme_advanced_list) regex_list;
} dvr_filename_scheme_advanced_t;

int dvr_filenaming_get_filename(char *destination, size_t max_size, dvr_filename_scheme_advanced_t *scheme, struct dvr_entry *de);

void dvr_filenaming_advanced_init(dvr_filename_scheme_advanced_t *scheme);
void dvr_filenaming_advanced_destroy(dvr_filename_scheme_advanced_t *scheme);

int dvr_filenaming_set_mode(struct dvr_config *cfg, int mode);
int dvr_filenaming_advanced_set_filename_format(dvr_filename_scheme_advanced_t *scheme, const char *format);
int dvr_filenaming_advanced_set_regex(dvr_filename_scheme_advanced_t *scheme, int index, const char *source, const char *regex);
int dvr_filenaming_advanced_set_regex_size(dvr_filename_scheme_advanced_t *scheme, int size);

#endif
