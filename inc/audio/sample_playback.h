/*
 * Copyright (C) 2019 Alexandros Theodotou <alex at zrythm dot org>
 *
 * This file is part of Zrythm
 *
 * Zrythm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Zrythm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Zrythm.  If not, see <https://www.gnu.org/licenses/>.
 */

/**
 * \file
 *
 * A framework from playing back samples independent
 * of the timeline, such as metronomes and samples
 * from the browser.
 */

#ifndef __AUDIO_SAMPLE_PLAYBACK_H__
#define __AUDIO_SAMPLE_PLAYBACK_H__

#include "utils/types.h"

/**
 * @addtogroup audio
 *
 * @{
 */

/**
 * A sample playback handle to be used by the engine.
 */
typedef struct SamplePlayback
{
  /** A pointer to the original buffer. */
  sample_t ** buf;

  /** The number of channels. */
  channels_t channels;

  /** The number of frames in the buffer. */
  unsigned_frame_t buf_size;

  /** The current offset in the buffer. */
  unsigned_frame_t offset;

  /** The volume to play the sample at (ratio from
   * 0.0 to 2.0, where 1.0 is the normal volume). */
  float volume;

  /** Offset relative to the current processing cycle
   * to start playing the sample. */
  nframes_t start_offset;

  /** Source file initialized from. */
  const char * file;

  /** Function initialized from. */
  const char * func;

  /** Line no initialized from. */
  int lineno;
} SamplePlayback;

/**
 * Initializes a SamplePlayback with a sample to
 * play back.
 */
#define sample_playback_init( \
  self, _buf, _buf_size, _channels, _vol, \
  _start_offset) \
  if (_channels <= 0) \
    { \
      g_critical ("channels: %u", _channels); \
    } \
  (self)->buf = _buf; \
  (self)->buf_size = _buf_size; \
  (self)->volume = _vol; \
  (self)->offset = 0; \
  (self)->channels = _channels; \
  (self)->start_offset = _start_offset; \
  (self)->file = __FILE__; \
  (self)->func = __func__; \
  (self)->lineno = __LINE__

/**
 * @}
 */

#endif
