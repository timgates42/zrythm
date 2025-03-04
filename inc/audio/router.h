/*
 * Copyright (C) 2019-2021 Alexandros Theodotou <alex at zrythm dot org>
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
 *
 * This file incorporates work covered by the following copyright and
 * permission notice:
 *
 * Copyright (C) 2017, 2019 Robin Gareus <robin@gareus.org>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

/**
 * \file
 *
 * DSP router.
 */

#ifndef __AUDIO_ROUTING_H__
#define __AUDIO_ROUTING_H__

#include "zrythm-config.h"

#include "audio/engine.h"
#include "utils/types.h"

#include <gtk/gtk.h>

#include "zix/ring.h"
#include "zix/sem.h"
#include <pthread.h>

typedef struct GraphNode GraphNode;
typedef struct Graph     Graph;
typedef struct PassthroughProcessor
                         PassthroughProcessor;
typedef struct MPMCQueue MPMCQueue;
typedef struct Port      Port;
typedef struct Fader     Fader;
typedef struct Track     Track;
typedef struct Plugin    Plugin;
typedef struct Position  Position;
typedef struct ControlPortChange ControlPortChange;
typedef struct EngineProcessTimeInfo
  EngineProcessTimeInfo;

#ifdef HAVE_JACK
#  include "weak_libjack.h"
#endif

/**
 * @addtogroup audio
 *
 * @{
 */

#define ROUTER (AUDIO_ENGINE->router)

typedef struct Router
{
  Graph * graph;

  /** Time info for this processing cycle. */
  EngineProcessTimeInfo time_nfo;

  /** Stored for the currently processing cycle */
  nframes_t max_route_playback_latency;

  /** Current global latency offset (max latency
   * of all routes - remaining latency from
   * engine). */
  nframes_t global_offset;

  /** Used when recalculating the graph. */
  ZixSem graph_access;

  bool callback_in_progress;

  /** Thread that calls kicks off the cycle. */
  GThread * process_kickoff_thread;

  /** Message queue for control port changes, used
   * for BPM/time signature changes. */
  ZixRing * ctrl_port_change_queue;

} Router;

Router *
router_new (void);

/**
 * Recalculates the process acyclic directed graph.
 *
 * @param soft If true, only readjusts latencies.
 */
void
router_recalc_graph (Router * self, bool soft);

/**
 * Starts a new cycle.
 */
void
router_start_cycle (
  Router *              self,
  EngineProcessTimeInfo time_nfo);

/**
 * Returns the max playback latency of the trigger
 * nodes.
 */
nframes_t
router_get_max_route_playback_latency (
  Router * router);

/**
 * Returns if the current thread is a
 * processing thread.
 */
WARN_UNUSED_RESULT
HOT NONNULL
  ACCESS_READ_ONLY (1) bool router_is_processing_thread (
    const Router * const router);

/**
 * Returns whether this is the thread that kicks
 * off processing (thread that calls
 * router_start_cycle()).
 */
WARN_UNUSED_RESULT
HOT NONNULL
  ACCESS_READ_ONLY (1) bool router_is_processing_kickoff_thread (
    const Router * const self);

/**
 * Queues a control port change to be applied
 * when processing starts.
 *
 * Currently only applies to BPM/time signature
 * changes.
 */
NONNULL
void
router_queue_control_port_change (
  Router *                  self,
  const ControlPortChange * change);

void
router_free (Router * self);

/**
 * @}
 */

#endif
