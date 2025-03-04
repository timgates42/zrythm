// SPDX-FileCopyrightText: © 2022 Alexandros Theodotou <alex@zrythm.org>
// SPDX-FileCopyrightText: 2014-2015 David Robillard <d@drobilla.net>
// SPDX-License-Identifier: ISC

#ifndef TEST_INSTRUMENT_URIS_H
#define TEST_INSTRUMENT_URIS_H

#include <lv2/atom/atom.h>
#include <lv2/log/log.h>
#include <lv2/midi/midi.h>
#include <lv2/patch/patch.h>
#include <lv2/state/state.h>

#define TEST_INSTRUMENT_URI \
  "https://lv2.zrythm.org/test-instrument"

typedef struct
{
  LV2_URID atom_Path;
  LV2_URID atom_Resource;
  LV2_URID atom_Sequence;
  LV2_URID atom_URID;
  LV2_URID atom_eventTransfer;
  LV2_URID midi_Event;
  LV2_URID patch_Set;
  LV2_URID patch_property;
  LV2_URID patch_value;
} FifthsURIs;

static inline void
map_fifths_uris (
  LV2_URID_Map * map,
  FifthsURIs *   uris)
{
  uris->atom_Path =
    map->map (map->handle, LV2_ATOM__Path);
  uris->atom_Resource =
    map->map (map->handle, LV2_ATOM__Resource);
  uris->atom_Sequence =
    map->map (map->handle, LV2_ATOM__Sequence);
  uris->atom_URID =
    map->map (map->handle, LV2_ATOM__URID);
  uris->atom_eventTransfer = map->map (
    map->handle, LV2_ATOM__eventTransfer);
  uris->midi_Event =
    map->map (map->handle, LV2_MIDI__MidiEvent);
  uris->patch_Set =
    map->map (map->handle, LV2_PATCH__Set);
  uris->patch_property =
    map->map (map->handle, LV2_PATCH__property);
  uris->patch_value =
    map->map (map->handle, LV2_PATCH__value);
}

#endif /* TEST_INSTRUMENT_URIS_H */
