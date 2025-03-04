/*
  Copyright 2007-2016 David Robillard <http://drobilla.net>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#ifndef __PLUGINS_LV2_WORKER_H__
#define __PLUGINS_LV2_WORKER_H__

#include "lv2/worker/worker.h"
#include "zix/ring.h"
#include "zix/sem.h"
#include "zix/thread.h"
#include <lilv/lilv.h>

typedef struct Lv2Plugin Lv2Plugin;

typedef struct
{
  Lv2Plugin * plugin; ///< Pointer back to the plugin
  ZixRing *   requests; ///< Requests to the worker
  ZixRing * responses; ///< Responses from the worker
  void *    response; ///< Worker response buffer
  ZixSem    sem;      ///< Worker semaphore
  ZixThread thread;   ///< Worker thread
  const LV2_Worker_Interface *
       iface;    ///< Plugin worker interface
  bool threaded; ///< Run work in another thread
} Lv2Worker;

void
lv2_worker_init (
  Lv2Plugin *                  plugin,
  Lv2Worker *                  worker,
  const LV2_Worker_Interface * iface,
  bool                         threaded);

/**
 * Stops the worker and frees resources.
 */
void
lv2_worker_finish (Lv2Worker * worker);

/**
 * Called from plugins during run() to request that
 * Zrythm calls the work() method in a non-realtime
 * context with the given arguments.
 */
LV2_Worker_Status
lv2_worker_schedule (
  LV2_Worker_Schedule_Handle handle,
  uint32_t                   size,
  const void *               data);

/**
 * Called during run() to process worker replies.
 *
 * Internally calls work_response in
 * https://lv2plug.in/doc/html/group__worker.html.
 */
void
lv2_worker_emit_responses (
  Lv2Worker *    worker,
  LilvInstance * instance);

#endif
