/*
 * Copyright (C) 2018-2019 Alexandros Theodotou
 *
 * This file is part of Zrythm
 *
 * Zrythm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Zrythm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Zrythm.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "config.h"

#include "audio/engine.h"
#include "audio/transport.h"
#include "project.h"
#include "gui/widgets/arranger.h"
#include "gui/widgets/arranger_playhead.h"
#include "gui/widgets/bot_dock_edge.h"
#include "gui/widgets/center_dock.h"
#include "gui/widgets/digital_meter.h"
#include "gui/widgets/main_window.h"
#include "gui/widgets/midi_arranger.h"
#include "gui/widgets/midi_modifier_arranger.h"
#include "gui/widgets/midi_ruler.h"
#include "gui/widgets/timeline_arranger.h"
#include "gui/widgets/timeline_ruler.h"
#include "gui/widgets/top_bar.h"

#include <gtk/gtk.h>

/**
 * Sets BPM and does any necessary processing (like
 * notifying interested parties).
 */
void
transport_set_bpm (float bpm)
{
  if (bpm < MIN_BPM)
    bpm = MIN_BPM;
  else if (bpm > MAX_BPM)
    bpm = MAX_BPM;
  TRANSPORT->bpm = bpm;
  engine_update_frames_per_tick (
    TRANSPORT->beats_per_bar,
    bpm,
    AUDIO_ENGINE->sample_rate);
}

/**
 * Initialize transport
 */
void
transport_init (Transport * self,
                int         loading)
{
  g_message ("Initializing transport");

  if (!loading)
    {
      // set inital total number of beats
      // this is applied to the ruler
      self->total_bars = DEFAULT_TOTAL_BARS;

      /* set BPM related defaults */
      self->beats_per_bar = DEFAULT_BEATS_PER_BAR;
      self->beat_unit = DEFAULT_BEAT_UNIT;

      // set positions of playhead, start/end markers
      position_set_to_bar (&self->playhead_pos, 1);
      position_set_to_bar (&self->cue_pos, 1);
      position_set_to_bar (&self->start_marker_pos, 1);
      position_set_to_bar (&self->end_marker_pos, 128);
      position_set_to_bar (&self->loop_start_pos, 1);
      position_set_to_bar (&self->loop_end_pos, 8);

      self->loop = 1;

      transport_set_bpm (DEFAULT_BPM);
    }

  /* set playstate */
  self->play_state = PLAYSTATE_PAUSED;

  zix_sem_init(&self->paused, 0);
}

void
transport_request_pause ()
{
  TRANSPORT->play_state = PLAYSTATE_PAUSE_REQUESTED;
}

void
transport_request_roll ()
{
  TRANSPORT->play_state = PLAYSTATE_ROLL_REQUESTED;
}


/**
 * Moves the playhead by the time corresponding to given samples.
 */
inline void
transport_add_to_playhead (
  int frames)
{
  if (TRANSPORT->play_state == PLAYSTATE_ROLLING)
    {
      position_add_frames (&TRANSPORT->playhead_pos,
                           frames);
      EVENTS_PUSH (ET_PLAYHEAD_POS_CHANGED, NULL);
    }
}

/**
 * Moves playhead to given pos.
 *
 * This is only for moves other than while playing
 * and for looping while playing.
 */
void
transport_move_playhead (Position * target, ///< position to set to
                         int      panic) ///< send MIDI panic or not
{
  /* send MIDI note off on currently playing timeline
   * objects */
  Track * track;
  Region * region;
  MidiNote * midi_note;
  Channel * channel;
  MidiEvents * midi_events;
#ifdef HAVE_JACK
  jack_midi_event_t * ev;
#endif
  for (int i = 0; i < TRACKLIST->num_tracks; i++)
    {
      track = TRACKLIST->tracks[i];
      channel = track->channel;

      for (int ii = 0; ii < track->num_regions; ii++)
        {
          region = track->regions[ii];

          if (!region_is_hit (region, &PLAYHEAD))
            continue;

          for (int j = 0;
               j < region->num_midi_notes;
               j++)
            {
              midi_note = region->midi_notes[j];

              if (midi_note_hit (
                    midi_note, &PLAYHEAD))
                {
                  midi_events =
                    channel->piano_roll->midi_events;
#ifdef HAVE_JACK
                  ev =
                    &midi_events->queue->
                      jack_midi_events[
                        midi_events->queue->
                          num_events++];
                  ev->time =
                    position_to_frames (
                      &PLAYHEAD);
                  ev->size = 3;
                  /* status byte */
                  ev->buffer[0] =
                    MIDI_CH1_NOTE_OFF;
                  /* note number */
                  ev->buffer[1] =
                    midi_note->val;
                  /* velocity */
                  ev->buffer[2] =
                    midi_note->vel->vel;
#endif
                }
            }
        }
    }

  /* move to new pos */
  position_set_to_pos (&TRANSPORT->playhead_pos,
                       target);
  /*if (panic)*/
    /*{*/
      /*AUDIO_ENGINE->panic = 1;*/
    /*}*/

  EVENTS_PUSH (ET_PLAYHEAD_POS_CHANGED, NULL);
}

/**
 * Updates the frames in all transport positions
 */
void
transport_update_position_frames ()
{
  position_update_frames (
    &TRANSPORT->playhead_pos);
  position_update_frames (
    &TRANSPORT->cue_pos);
  position_update_frames (
    &TRANSPORT->start_marker_pos);
  position_update_frames (
    &TRANSPORT->end_marker_pos);
  position_update_frames (
    &TRANSPORT->loop_start_pos);
  position_update_frames (
    &TRANSPORT->loop_end_pos);
}

/**
 * Gets beat unit as int.
 */
int
transport_get_beat_unit ()
{
  switch (TRANSPORT->beat_unit)
    {
    case BEAT_UNIT_2:
      return 2;
    case BEAT_UNIT_4:
      return 4;
    case BEAT_UNIT_8:
      return 8;
    case BEAT_UNIT_16:
      return 16;
    }
  g_warn_if_reached ();
  return -1;
}
