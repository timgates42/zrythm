/*
 * Copyright (C) 2019-2022 Alexandros Theodotou <alex at zrythm dot org>
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
 * API for Regions inside audio Track's.
 */

#ifndef __AUDIO_AUDIO_REGION_H__
#define __AUDIO_AUDIO_REGION_H__

#include "audio/position.h"
#include "audio/region.h"
#include "utils/audio.h"
#include "utils/types.h"

typedef struct _RegionWidget RegionWidget;
typedef struct Channel       Channel;
typedef struct Track         Track;
typedef struct ZRegion       AudioRegion;
typedef struct AudioClip     AudioClip;
typedef struct StereoPorts   StereoPorts;

/**
 * @addtogroup audio
 *
 * @{
 */

#if 0
/** Default fade to inject when playing back. */
#  define AUDIO_REGION_DEFAULT_FADE_MS 1
#endif

/**
 * Number of frames for built-in fade (additional
 * to object fades).
 */
#define AUDIO_REGION_BUILTIN_FADE_FRAMES 10

/**
 * Creates a region for audio data.
 *
 * @param pool_id The pool ID. This is used when
 *   creating clone regions (non-main) and must be
 *   -1 when creating a new clip.
 * @param filename Filename, if loading from
 *   file, otherwise NULL.
 * @param read_from_pool Whether to save the given
 *   @a filename or @a frames to pool and read the
 *   data from the pool. Only used if @a filename or
 *   @a frames is given.
 * @param frames Float array, if loading from
 *   float array, otherwise NULL.
 * @param nframes Number of frames per channel.
 *   Only used if @ref frames is non-NULL.
 * @param clip_name Name of audio clip, if not
 *   loading from file.
 * @param bit_depth Bit depth, if using \ref frames.
 */
ZRegion *
audio_region_new (
  const int              pool_id,
  const char *           filename,
  bool                   read_from_pool,
  const float *          frames,
  const unsigned_frame_t nframes,
  const char *           clip_name,
  const channels_t       channels,
  BitDepth               bit_depth,
  const Position *       start_pos,
  unsigned int           track_name_hash,
  int                    lane_pos,
  int                    idx_inside_lane);

#if 0
/**
 * Allocates the frame caches from the frames in
 * the clip.
 */
void
audio_region_init_frame_caches (
  AudioRegion * self,
  AudioClip *   clip);

/**
 * Updates the region's channel caches from the
 * region's frames.
 */
void
audio_region_update_channel_caches (
  ZRegion *   self,
  AudioClip * clip);
#endif

/**
 * Returns the audio clip associated with the
 * Region.
 */
AudioClip *
audio_region_get_clip (const ZRegion * self);

/**
 * Sets the clip ID on the region and updates any
 * references.
 */
void
audio_region_set_clip_id (
  ZRegion * self,
  int       clip_id);

/**
 * Replaces the region's frames from \ref
 * start_frames with \ref frames.
 *
 * @param duplicate_clip Whether to duplicate the
 *   clip (eg, when other regions refer to it).
 * @param frames Frames, interleaved.
 */
void
audio_region_replace_frames (
  ZRegion *        self,
  float *          frames,
  unsigned_frame_t start_frame,
  unsigned_frame_t num_frames,
  bool             duplicate_clip);

/**
 * Fills audio data from the region.
 *
 * @note The caller already splits calls to this
 *   function at each sub-loop inside the region,
 *   so region loop related logic is not needed.
 *
 * @param stereo_ports StereoPorts to fill.
 */
REALTIME
HOT NONNULL void
audio_region_fill_stereo_ports (
  ZRegion *                           self,
  const EngineProcessTimeInfo * const time_nfo,
  StereoPorts * stereo_ports);

float
audio_region_detect_bpm (
  ZRegion * self,
  GArray *  candidates);

bool
audio_region_validate (ZRegion * self);

/**
 * Frees members only but not the audio region itself.
 *
 * Regions should be free'd using region_free.
 */
void
audio_region_free_members (ZRegion * self);

/**
 * @}
 */

#endif // __AUDIO_AUDIO_REGION_H__
