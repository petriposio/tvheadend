/*
 *  Digital Video Recorder - filenaming
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

#include "config.h"
#include "dvr_filenaming.h"
#include "dvr.h"

static int dvr_filenaming_basic_generate(dvr_entry_t *de, dvr_config_t *cfg);

static void remove_slashes(char *s);
static void remove_specialcharacters(char *s, int dvr_flags);
static void dvr_filenaming_basic_make_title(char *output, size_t outlen, dvr_entry_t *de, dvr_config_t *cfg);

static int construct_final_filename(dvr_entry_t *de, const streaming_start_t *ss);

int dvr_filenaming_generate_filename(char *destination, size_t max_size, struct dvr_entry *de, const streaming_start_t *ss)
{
  dvr_config_t *cfg = dvr_config_find_by_name_default(de->de_config_name);

  int ret;

#if !ENABLE_ADVANCEDFILENAMES
  ret = dvr_filenaming_basic_generate(de, cfg, ss);
#else
  switch (cfg->dvr_filename_mode)
  {
  case DVR_FILENAMEMODE_ADVANCED:
    //dvr_filenaming_advanced_generate
    break;
  case DVR_FILENAMEMODE_BASIC:
  default:
    ret = dvr_filenaming_basic_generate(de, cfg, ss);
    break;
  }
#endif

  //remove_specialcharacters();
  //construct_final_filename();

  return ret;
}

static void remove_slashes(char *s)
{
  for (; *s; s++)
  {
    if(*s == '/')
      *s = '-';
  }
}

/**
 * Replace various chars with a dash
 */
static void remove_specialcharacters(char *s, int dvr_flags)
{
  int clean_white = dvr_flags & DVR_WHITESPACE_IN_TITLE;
  int clean_special = dvr_flags & DVR_CLEAN_TITLE;
  if (! (clean_white || clean_special))
    return;

  for (; *s; s++)
  {
    if(clean_white && (*s == ' ' || *s == '\t'))
      *s = '-';
    else if(clean_special &&
             ((*s < 32) || (*s > 122) ||
             (strchr(":\\<>|*?'\"", *s) != NULL)))
      *s = '-';
  }
}

static int construct_final_filename(const char *filename, dvr_entry_t *de, const streaming_start_t *ss)
{
  char *path_end = strrchr(filename, '/');
  if (!path_end)
    return -1;

  char *path;
  if (asprintf(&path, "%.*s", path_end - filename, filename) == -1)
    return -2;

  if(makedirs(path, 0777) != 0)
    return -1;

  const char *suffix = muxer_suffix(de->de_mux, ss);

  const int max_tally = 100000;
  size_t buffer_size = strlen(filename) + strlen(suffix) + 3 + (int) log10(max_tally);
  char *buffer = calloc(buffer_size * sizeof(char));
  if (!buffer)
  {
    free(path);
    return -2;
  }

  /* Construct final name */
  snprintf(buffer, buffer_size, "%s.%s", filename, suffix);

  int tally;
  for (tally = 0; tally <= max_tally; tally++) {
    if(stat(buffer, &st) == -1) {
      tvhlog(LOG_DEBUG, "dvr", "File \"%s\" -- %s -- Using for recording",
	     buffer, strerror(errno));
      break;
    }

    tvhlog(LOG_DEBUG, "dvr", "Overwrite protection, file \"%s\" exists", 
	   buffer);

    tally++;

    snprintf(buffer, buffer_size, "%s-%d.%s",
	     filename, tally, suffix);
  }

  free(path);

  if (de->de_filename)
    free(de->de_filename);
  de->filename = buffer;
}

static void dvr_filenaming_basic_make_title(char *output, size_t outlen, dvr_entry_t *de, dvr_config_t *cfg)
{
  struct tm tm;
  char buf[40];
  int i;

  if(cfg->dvr_flags & DVR_CHANNEL_IN_TITLE)
    snprintf(output, outlen, "%s-", DVR_CH_NAME(de));
  else
    output[0] = 0;

  snprintf(output + strlen(output), outlen - strlen(output),
	   "%s", lang_str_get(de->de_title, NULL));

  localtime_r(&de->de_start, &tm);

  if(cfg->dvr_flags & DVR_DATE_IN_TITLE) {
    strftime(buf, sizeof(buf), "%F", &tm);
    snprintf(output + strlen(output), outlen - strlen(output), ".%s", buf);
  }

  if(cfg->dvr_flags & DVR_TIME_IN_TITLE) {
    strftime(buf, sizeof(buf), "%H-%M", &tm);
    snprintf(output + strlen(output), outlen - strlen(output), ".%s", buf);
  }

  if(cfg->dvr_flags & DVR_EPISODE_IN_TITLE) {
    if(de->de_bcast && de->de_bcast->episode)  
      epg_episode_number_format(de->de_bcast->episode,
                                output + strlen(output),
                                outlen - strlen(output),
                                ".", "S%02d", NULL, "E%02d", NULL);
  }
}

/**
 * Filename generator
 *
 * - convert from utf8
 * - avoid duplicate filenames
 *
 */
static int dvr_filenaming_basic_generate(dvr_entry_t *de, dvr_config_t *cfg, const streaming_start_t *ss)
{
  char fullname[1000];
  char path[500];
  int tally = 0;
  struct stat st;
  char filename[1000];
  struct tm tm;

  dvr_filenaming_basic_make_title(filename, sizeof(filename), de, cfg);
  clean_filename(filename,cfg->dvr_flags);

  snprintf(path, sizeof(path), "%s", cfg->dvr_storage);

  /* Remove trailing slash */

  if (path[strlen(path)-1] == '/')
    path[strlen(path)-1] = '\0';

  /* Append per-day directory */

  if(cfg->dvr_flags & DVR_DIR_PER_DAY) {
    localtime_r(&de->de_start, &tm);
    strftime(fullname, sizeof(fullname), "%F", &tm);
    remove_slashes(fullname);
    snprintf(path + strlen(path), sizeof(path) - strlen(path), 
	     "/%s", fullname);
  }

  /* Append per-channel directory */

  if(cfg->dvr_flags & DVR_DIR_PER_CHANNEL) {

    char *chname = strdup(DVR_CH_NAME(de));
    remove_slashes(chname);
    snprintf(path + strlen(path), sizeof(path) - strlen(path), 
	     "/%s", chname);
    free(chname);
  }

  // TODO: per-brand, per-season

  /* Append per-title directory */

  if(cfg->dvr_flags & DVR_DIR_PER_TITLE) {

    char *title = strdup(lang_str_get(de->de_title, NULL));
    remove_slashes(title);
    snprintf(path + strlen(path), sizeof(path) - strlen(path), 
	     "/%s", title);
    free(title);
  }

  tvh_str_set(&de->de_filename, fullname);

  return 0;
}

