/*
 * Copyright (C) 2018-2022 Alexandros Theodotou <alex at zrythm dot org>
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

#include "actions/tracklist_selections.h"
#include "audio/audio_region.h"
#include "audio/channel.h"
#include "audio/chord_track.h"
#include "audio/group_target_track.h"
#include "audio/master_track.h"
#include "audio/midi_file.h"
#include "audio/router.h"
#include "audio/track.h"
#include "audio/tracklist.h"
#include "gui/backend/event.h"
#include "gui/backend/event_manager.h"
#include "gui/widgets/arranger.h"
#include "gui/widgets/bot_dock_edge.h"
#include "gui/widgets/center_dock.h"
#include "gui/widgets/main_window.h"
#include "gui/widgets/mixer.h"
#include "gui/widgets/timeline_arranger.h"
#include "gui/widgets/track.h"
#include "gui/widgets/tracklist.h"
#include "project.h"
#include "utils/arrays.h"
#include "utils/error.h"
#include "utils/flags.h"
#include "utils/object_utils.h"
#include "utils/objects.h"
#include "utils/string.h"
#include "zrythm_app.h"

#include <glib/gi18n.h>

/**
 * Initializes the tracklist when loading a project.
 */
void
tracklist_init_loaded (
  Tracklist *       self,
  Project *         project,
  SampleProcessor * sample_processor)
{
  self->project = project;
  self->sample_processor = sample_processor;

  g_message ("initializing loaded Tracklist...");
  for (int i = 0; i < self->num_tracks; i++)
    {
      Track * track = self->tracks[i];
      track_set_magic (track);
    }

  for (int i = 0; i < self->num_tracks; i++)
    {
      Track * track = self->tracks[i];

      if (track->type == TRACK_TYPE_CHORD)
        self->chord_track = track;
      else if (track->type == TRACK_TYPE_MARKER)
        self->marker_track = track;
      else if (track->type == TRACK_TYPE_MASTER)
        self->master_track = track;
      else if (track->type == TRACK_TYPE_TEMPO)
        self->tempo_track = track;
      else if (track->type == TRACK_TYPE_MODULATOR)
        self->modulator_track = track;

      track_init_loaded (track, self, NULL);
    }
}

/**
 * Selects or deselects all tracks.
 *
 * @note When deselecting the last track will become
 *   selected (there must always be >= 1 tracks
 *   selected).
 */
void
tracklist_select_all (
  Tracklist * self,
  bool        select,
  bool        fire_events)
{
  for (int i = 0; i < self->num_tracks; i++)
    {
      Track * track = self->tracks[i];

      track_select (
        track, select, F_NOT_EXCLUSIVE,
        fire_events);

      if (!select && i == self->num_tracks - 1)
        {
          track_select (
            track, F_SELECT, F_EXCLUSIVE,
            fire_events);
        }
    }
}

/**
 * Finds visible tracks and puts them in given array.
 */
void
tracklist_get_visible_tracks (
  Tracklist * self,
  Track **    visible_tracks,
  int *       num_visible)
{
  *num_visible = 0;
  for (int i = 0; i < self->num_tracks; i++)
    {
      Track * track = self->tracks[i];
      if (track_get_should_be_visible (track))
        {
          visible_tracks[*num_visible++] = track;
        }
    }
}

/**
 * Returns the number of visible Tracks between
 * src and dest (negative if dest is before src).
 */
int
tracklist_get_visible_track_diff (
  Tracklist *   self,
  const Track * src,
  const Track * dest)
{
  g_return_val_if_fail (src && dest, 0);

  int count = 0;
  if (src->pos < dest->pos)
    {
      for (int i = src->pos; i < dest->pos; i++)
        {
          Track * track = self->tracks[i];
          if (track_get_should_be_visible (track))
            {
              count++;
            }
        }
    }
  else if (src->pos > dest->pos)
    {
      for (int i = dest->pos; i < src->pos; i++)
        {
          Track * track = self->tracks[i];
          if (track_get_should_be_visible (track))
            {
              count--;
            }
        }
    }

  return count;
}

int
tracklist_contains_master_track (Tracklist * self)
{
  for (int i = 0; self->num_tracks; i++)
    {
      Track * track = self->tracks[i];
      if (track->type == TRACK_TYPE_MASTER)
        return 1;
    }
  return 0;
}

int
tracklist_contains_chord_track (Tracklist * self)
{
  for (int i = 0; self->num_tracks; i++)
    {
      Track * track = self->tracks[i];
      if (track->type == TRACK_TYPE_CHORD)
        return 1;
    }
  return 0;
}

void
tracklist_print_tracks (Tracklist * self)
{
  g_message ("----- tracklist tracks ------");
  Track * track;
  for (int i = 0; i < self->num_tracks; i++)
    {
      track = self->tracks[i];
      if (track)
        {
          char parent_str[200];
          strcpy (parent_str, "");
          GPtrArray * parents = g_ptr_array_new ();
          track_add_folder_parents (
            track, parents, false);
          for (size_t j = 0; j < parents->len; j++)
            {
              strcat (parent_str, "--");
              if (j == parents->len - 1)
                strcat (parent_str, " ");
            }

          g_message (
            "[%03d] %s%s (pos %d, parents %d, "
            "size %d)",
            i, parent_str, track->name, track->pos,
            parents->len, track->size);
          g_ptr_array_unref (parents);
        }
      else
        {
          g_message ("[%03d] (null)", i);
        }
    }
  g_message ("------ end ------");
}

static void
swap_tracks (Tracklist * self, int src, int dest)
{
  self->swapping_tracks = true;

  Track * src_track = self->tracks[src];
  Track * dest_track = self->tracks[dest];
  g_debug (
    "swapping tracks %s [%d] and %s [%d]...",
    src_track ? src_track->name : "(null)", src,
    dest_track ? dest_track->name : "(null)", dest);

  /* move src somewhere temporarily */
  self->tracks[src] = NULL;
  self->tracks[self->num_tracks + 1] = src_track;
  if (src_track)
    src_track->pos = self->num_tracks + 1;

  /* move dest to src */
  self->tracks[src] = dest_track;
  self->tracks[dest] = NULL;
  if (dest_track)
    dest_track->pos = src;

  /* move src from temp pos to dest */
  self->tracks[dest] = src_track;
  self->tracks[self->num_tracks + 1] = NULL;
  if (src_track)
    src_track->pos = dest;

  self->swapping_tracks = false;
  g_debug ("tracks swapped");
}

/**
 * Adds given track to given spot in tracklist.
 *
 * @param publish_events Publish UI events.
 * @param recalc_graph Recalculate routing graph.
 */
void
tracklist_insert_track (
  Tracklist * self,
  Track *     track,
  int         pos,
  int         publish_events,
  int         recalc_graph)
{
  g_message (
    "inserting %s at %d (has output %d)...",
    track->name, pos,
    track->channel && track->channel->has_output);

  /* set to -1 so other logic knows it is a new
   * track */
  track->pos = -1;
  if (track->channel)
    {
      track->channel->track_pos = -1;
    }

  /* this needs to be called before appending the
   * track to the tracklist */
  track_set_name (
    track, track->name, F_NO_PUBLISH_EVENTS);

  /* append the track at the end */
  array_append (
    self->tracks, self->num_tracks, track);
  track->tracklist = self;

  /* add flags for auditioner track ports */
  if (tracklist_is_auditioner (self))
    {
      GPtrArray * ports = g_ptr_array_new ();
      track_append_ports (track, ports, true);
      for (size_t i = 0; i < ports->len; i++)
        {
          Port * port =
            g_ptr_array_index (ports, i);
          port->id.flags2 |=
            PORT_FLAG2_SAMPLE_PROCESSOR_TRACK;
        }
      object_free_w_func_and_null (
        g_ptr_array_unref, ports);
    }

  /* if inserting it, swap until it reaches its
   * position */
  if (pos != self->num_tracks - 1)
    {
      for (int i = self->num_tracks - 1; i > pos;
           i--)
        {
          swap_tracks (self, i, i - 1);
        }
    }

  track->pos = pos;

  if (
    tracklist_is_in_active_project (self)
    /* auditioner doesn't need automation */
    && !tracklist_is_auditioner (self))
    {
      /* make the track the only selected track */
      tracklist_selections_select_single (
        TRACKLIST_SELECTIONS, track,
        publish_events);

      /* set automation track on ports */
      AutomationTracklist * atl =
        track_get_automation_tracklist (track);
      if (atl)
        {
          for (int i = 0; i < atl->num_ats; i++)
            {
              AutomationTrack * at = atl->ats[i];
              Port *            port =
                port_find_from_identifier (
                  &at->port_id);
              g_return_if_fail (
                IS_PORT_AND_NONNULL (port));
              port->at = at;
            }
        }
    }

  if (track->channel)
    {
      channel_connect (track->channel);
    }

  /* if audio output route to master */
  if (
    track->out_signal_type == TYPE_AUDIO
    && track->type != TRACK_TYPE_MASTER)
    {
      group_target_track_add_child (
        self->master_track,
        track_get_name_hash (track), F_CONNECT,
        F_NO_RECALC_GRAPH, F_NO_PUBLISH_EVENTS);
    }

  if (tracklist_is_in_active_project (self))
    track_activate_all_plugins (track, F_ACTIVATE);

  if (!tracklist_is_auditioner (self))
    {
      /* verify */
      track_validate (track);
    }

  if (ZRYTHM_TESTING)
    {
      for (int i = 0; i < self->num_tracks; i++)
        {
          Track * cur_track = self->tracks[i];
          if (track_type_has_channel (
                cur_track->type))
            {
              Channel * ch = cur_track->channel;
              if (ch->has_output)
                {
                  g_return_if_fail (
                    ch->output_name_hash
                    != track_get_name_hash (
                      cur_track));
                }
            }
        }
    }

  if (ZRYTHM_HAVE_UI && !tracklist_is_auditioner (self))
    {
      /* generate track widget */
      track->widget = track_widget_new (track);
    }

  if (recalc_graph)
    {
      router_recalc_graph (ROUTER, F_NOT_SOFT);
    }

  if (publish_events)
    {
      EVENTS_PUSH (ET_TRACK_ADDED, track);
    }

  g_message (
    "%s: done - inserted track '%s' (%u) at %d",
    __func__, track->name,
    track_get_name_hash (track), pos);
}

ChordTrack *
tracklist_get_chord_track (const Tracklist * self)
{
  Track * track;
  for (int i = 0; i < self->num_tracks; i++)
    {
      track = self->tracks[i];
      if (track->type == TRACK_TYPE_CHORD)
        {
          return (ChordTrack *) track;
        }
    }
  g_warn_if_reached ();
  return NULL;
}

/**
 * Returns the Track matching the given name, if
 * any.
 */
Track *
tracklist_find_track_by_name (
  Tracklist *  self,
  const char * name)
{
  if (G_UNLIKELY (
        ROUTER
        && router_is_processing_thread (ROUTER)))
    {
      g_critical (
        "attempted to call from DSP thread");
      return NULL;
    }

  for (int i = 0; i < self->num_tracks; i++)
    {
      Track * track = self->tracks[i];
      if (string_is_equal (name, track->name))
        return track;
    }
  return NULL;
}

/**
 * Returns the Track matching the given name, if
 * any.
 */
NONNULL
Track *
tracklist_find_track_by_name_hash (
  Tracklist *  self,
  unsigned int hash)
{
  if (
    G_LIKELY (tracklist_is_in_active_project (self))
    && ROUTER
    && router_is_processing_thread (ROUTER)
    && !tracklist_is_auditioner (self))
    {
      for (int i = 0; i < self->num_tracks; i++)
        {
          Track * track = self->tracks[i];
          if (track->name_hash == hash)
            return track;
        }
    }
  else
    {
      for (int i = 0; i < self->num_tracks; i++)
        {
          Track * track = self->tracks[i];
          g_return_val_if_fail (
            IS_TRACK_AND_NONNULL (track), NULL);
          if (track_get_name_hash (track) == hash)
            return track;
        }
    }
  return NULL;
}

void
tracklist_append_track (
  Tracklist * self,
  Track *     track,
  int         publish_events,
  int         recalc_graph)
{
  tracklist_insert_track (
    self, track, self->num_tracks, publish_events,
    recalc_graph);
}

/**
 * Multiplies all tracks' heights and returns if
 * the operation was valid.
 *
 * @param visible_only Only apply to visible tracks.
 */
bool
tracklist_multiply_track_heights (
  Tracklist * self,
  double      multiplier,
  bool        visible_only,
  bool        check_only,
  bool        fire_events)
{
  for (int i = 0; i < self->num_tracks; i++)
    {
      Track * tr = self->tracks[i];

      if (
        visible_only
        && !track_get_should_be_visible (tr))
        continue;

      bool ret = track_multiply_heights (
        tr, multiplier, visible_only, check_only);
      /*g_debug ("%s can multiply %d",*/
      /*tr->name, ret);*/

      if (!ret)
        {
          return false;
        }

      if (!check_only && fire_events)
        {
          /* FIXME should be event */
          track_widget_update_size (tr->widget);
        }
    }

  return true;
}

/**
 * Returns the track at the given index or NULL if
 * the index is invalid.
 */
Track *
tracklist_get_track (Tracklist * self, int idx)
{
  if (idx < 0 || idx >= self->num_tracks)
    {
      g_warning ("invalid track idx %d", idx);
      return NULL;
    }

  Track * tr = self->tracks[idx];
  g_return_val_if_fail (
    IS_TRACK_AND_NONNULL (tr), NULL);

  return tr;
}

int
tracklist_get_track_pos (
  Tracklist * self,
  Track *     track)
{
  return array_index_of (
    (void **) self->tracks, self->num_tracks,
    (void *) track);
}

/**
 * Returns the first track found with the given
 * type.
 */
Track *
tracklist_get_track_by_type (
  Tracklist * self,
  TrackType   type)
{
  for (int i = 0; i < self->num_tracks; i++)
    {
      Track * track = self->tracks[i];
      if (track->type == type)
        return track;
    }
  return NULL;
}

bool
tracklist_validate (Tracklist * self)
{
  for (int i = 0; i < self->num_tracks; i++)
    {
      Track * track = self->tracks[i];
      g_return_val_if_fail (
        track && track_is_in_active_project (track),
        false);

      if (!track_validate (track))
        return false;

      /* validate size */
      g_return_val_if_fail (
        track->pos + track->size <= self->num_tracks,
        false);

      /* validate connections */
      if (track->channel)
        {
          Channel * ch = track->channel;
          for (int j = 0; j < STRIP_SIZE; j++)
            {
              ChannelSend * send = ch->sends[j];
              channel_send_validate (send);
            }
        }
    }

  return true;
}

/**
 * Returns the index of the last Track.
 *
 * @param pin_opt Pin option.
 * @param visible_only Only consider visible
 *   Track's.
 */
int
tracklist_get_last_pos (
  Tracklist *              self,
  const TracklistPinOption pin_opt,
  const bool               visible_only)
{
  Track * tr;
  for (int i = self->num_tracks - 1; i >= 0; i--)
    {
      tr = self->tracks[i];

      if (
        pin_opt == TRACKLIST_PIN_OPTION_PINNED_ONLY
        && !track_is_pinned (tr))
        {
          continue;
        }
      if (
        pin_opt == TRACKLIST_PIN_OPTION_UNPINNED_ONLY
        && track_is_pinned (tr))
        {
          continue;
        }
      if (
        visible_only
        && !track_get_should_be_visible (tr))
        {
          continue;
        }

      return i;
    }

  /* no track with given options found,
   * select the last */
  return self->num_tracks - 1;
}

/**
 * Returns the last Track.
 *
 * @param pin_opt Pin option.
 * @param visible_only Only consider visible
 *   Track's.
 */
Track *
tracklist_get_last_track (
  Tracklist *              self,
  const TracklistPinOption pin_opt,
  const int                visible_only)
{
  int idx = tracklist_get_last_pos (
    self, pin_opt, visible_only);
  g_return_val_if_fail (
    idx >= 0 && idx < self->num_tracks, NULL);
  Track * tr = self->tracks[idx];

  return tr;
}

/**
 * Returns the Track after delta visible Track's.
 *
 * Negative delta searches backwards.
 *
 * This function searches tracks only in the same
 * Tracklist as the given one (ie, pinned or not).
 */
Track *
tracklist_get_visible_track_after_delta (
  Tracklist * self,
  Track *     track,
  int         delta)
{
  if (delta > 0)
    {
      Track * vis_track = track;
      while (delta > 0)
        {
          vis_track =
            tracklist_get_next_visible_track (
              self, vis_track);

          if (!vis_track)
            return NULL;

          delta--;
        }
      return vis_track;
    }
  else if (delta < 0)
    {
      Track * vis_track = track;
      while (delta < 0)
        {
          vis_track =
            tracklist_get_prev_visible_track (
              self, vis_track);

          if (!vis_track)
            return NULL;

          delta++;
        }
      return vis_track;
    }
  else
    return track;
}

/**
 * Returns the first visible Track.
 *
 * @param pinned 1 to check the pinned tracklist,
 *   0 to check the non-pinned tracklist.
 */
Track *
tracklist_get_first_visible_track (
  Tracklist * self,
  const int   pinned)
{
  Track * tr;
  for (int i = 0; i < self->num_tracks; i++)
    {
      tr = self->tracks[i];
      if (
        track_get_should_be_visible (tr)
        && track_is_pinned (tr) == pinned)
        {
          return self->tracks[i];
        }
    }
  g_warn_if_reached ();
  return NULL;
}

/**
 * Returns the previous visible Track.
 */
Track *
tracklist_get_prev_visible_track (
  Tracklist * self,
  Track *     track)
{
  Track * tr;
  for (int i =
         tracklist_get_track_pos (self, track) - 1;
       i >= 0; i--)
    {
      tr = self->tracks[i];
      if (track_get_should_be_visible (tr))
        {
          g_warn_if_fail (tr != track);
          return tr;
        }
    }
  return NULL;
}

/**
 * Returns the next visible Track in the same
 * Tracklist.
 */
Track *
tracklist_get_next_visible_track (
  Tracklist * self,
  Track *     track)
{
  Track * tr;
  for (int i =
         tracklist_get_track_pos (self, track) + 1;
       i < self->num_tracks; i++)
    {
      tr = self->tracks[i];
      if (track_get_should_be_visible (tr))
        {
          g_warn_if_fail (tr != track);
          return tr;
        }
    }
  return NULL;
}

/**
 * Removes a track from the Tracklist and the
 * TracklistSelections.
 *
 * Also disconnects the channel.
 *
 * @param rm_pl Remove plugins or not.
 * @param free Free the track or not (free later).
 * @param publish_events Push a track deleted event
 *   to the UI.
 * @param recalc_graph Recalculate the mixer graph.
 */
void
tracklist_remove_track (
  Tracklist * self,
  Track *     track,
  bool        rm_pl,
  bool        free_track,
  bool        publish_events,
  bool        recalc_graph)
{
  g_return_if_fail (IS_TRACK (track));
  g_message (
    "%s: removing [%d] %s - remove plugins %d - "
    "free track %d - pub events %d - "
    "recalc graph %d - "
    "num tracks before deletion: %d",
    __func__, track->pos, track->name, rm_pl,
    free_track, publish_events, recalc_graph,
    self->num_tracks);

  Track * prev_visible = NULL;
  Track * next_visible = NULL;
  if (!tracklist_is_auditioner (self))
    {
      prev_visible =
        tracklist_get_prev_visible_track (
          self, track);
      next_visible =
        tracklist_get_next_visible_track (
          self, track);
    }

  /* remove/deselect all objects */
  track_clear (track);

  int idx = array_index_of (
    self->tracks, self->num_tracks, track);
  g_warn_if_fail (track->pos == idx);

  track_disconnect (
    track, rm_pl, F_NO_RECALC_GRAPH);

  /* move track to the end */
  int end_pos = self->num_tracks - 1;
  tracklist_move_track (
    self, track, end_pos, false,
    F_NO_PUBLISH_EVENTS, F_NO_RECALC_GRAPH);

  if (!tracklist_is_auditioner (self))
    {
      tracklist_selections_remove_track (
        TRACKLIST_SELECTIONS, track,
        publish_events);
    }

  array_delete (
    self->tracks, self->num_tracks, track);

  if (
    tracklist_is_in_active_project (self)
    && !tracklist_is_auditioner (self))
    {
      /* if it was the only track selected, select
       * the next one */
      if (TRACKLIST_SELECTIONS->num_tracks == 0)
        {
          Track * track_to_select = NULL;
          if (prev_visible || next_visible)
            {
              track_to_select =
                next_visible
                  ? next_visible
                  : prev_visible;
            }
          else
            {
              track_to_select = self->tracks[0];
            }
          tracklist_selections_add_track (
            TRACKLIST_SELECTIONS, track_to_select,
            publish_events);
        }
    }

  track->pos = -1;

  if (free_track)
    {
      track_free (track);
      self->tracks[end_pos] = NULL;
    }

  if (recalc_graph)
    {
      router_recalc_graph (ROUTER, F_NOT_SOFT);
    }

  if (publish_events)
    {
      EVENTS_PUSH (ET_TRACKS_REMOVED, NULL);
    }

  g_message ("%s: done", __func__);
}

/**
 * Moves a track from its current position to the
 * position given by \p pos.
 *
 * @param pos Position to insert at, or -1 to
 *   insert at the end.
 * @param always_before_pos Whether the track
 *   should always be put before the track currently
 *   at @ref pos. If this is true, when moving
 *   down, the resulting track position will be
 *   @ref pos - 1.
 * @param publish_events Push UI update events or
 *   not.
 * @param recalc_graph Recalculate routing graph.
 */
void
tracklist_move_track (
  Tracklist * self,
  Track *     track,
  int         pos,
  bool        always_before_pos,
  bool        publish_events,
  bool        recalc_graph)
{
  g_message (
    "%s: %s from %d to %d", __func__, track->name,
    track->pos, pos);

  if (pos == track->pos)
    return;

  bool move_higher = pos < track->pos;

  Track * prev_visible =
    tracklist_get_prev_visible_track (self, track);
  Track * next_visible =
    tracklist_get_next_visible_track (self, track);

  int idx = array_index_of (
    self->tracks, self->num_tracks, track);
  g_warn_if_fail (track->pos == idx);

  if (
    tracklist_is_in_active_project (self)
    && !tracklist_is_auditioner (self))
    {
      /* clear the editor region if it exists and
       * belongs to this track */
      ZRegion * region =
        clip_editor_get_region (CLIP_EDITOR);
      if (
        region
        && arranger_object_get_track (
             (ArrangerObject *) region)
             == track)
        {
          clip_editor_set_region (
            CLIP_EDITOR, NULL, publish_events);
        }

      /* deselect all objects */
      track_unselect_all (track);

      tracklist_selections_remove_track (
        TRACKLIST_SELECTIONS, track,
        publish_events);

      /* if it was the only track selected, select
       * the next one */
      if (
        TRACKLIST_SELECTIONS->num_tracks == 0
        && (prev_visible || next_visible))
        {
          tracklist_selections_add_track (
            TRACKLIST_SELECTIONS,
            next_visible ? next_visible : prev_visible,
            publish_events);
        }
    }

  if (move_higher)
    {
      /* move all other tracks 1 track further */
      for (int i = track->pos; i > pos; i--)
        {
          swap_tracks (self, i, i - 1);
        }
    }
  else
    {
      /* move all other tracks 1 track earlier */
      for (int i = track->pos; i < pos; i++)
        {
          swap_tracks (self, i, i + 1);
        }

      if (always_before_pos && pos > 0)
        {
          /* swap with previous track */
          swap_tracks (self, pos, pos - 1);
        }
    }

  if (
    tracklist_is_in_active_project (self)
    && !tracklist_is_auditioner (self))
    {
      /* make the track the only selected track */
      tracklist_selections_select_single (
        TRACKLIST_SELECTIONS, track,
        publish_events);
    }

  if (recalc_graph)
    {
      router_recalc_graph (ROUTER, F_NOT_SOFT);
    }

  if (publish_events)
    {
      EVENTS_PUSH (ET_TRACKS_MOVED, NULL);
    }

  g_debug ("%s: finished moving track", __func__);
}

/**
 * Returns 1 if the track name is not taken.
 *
 * @param track_to_skip Track to skip when searching.
 */
bool
tracklist_track_name_is_unique (
  Tracklist *  self,
  const char * name,
  Track *      track_to_skip)
{
  for (int i = 0; i < self->num_tracks; i++)
    {
      if (
        string_is_equal (name, self->tracks[i]->name)
        && self->tracks[i] != track_to_skip)
        return false;
    }
  return true;
}

/**
 * Returns if the tracklist has soloed tracks.
 */
bool
tracklist_has_soloed (const Tracklist * self)
{
  Track * track;
  for (int i = 0; i < self->num_tracks; i++)
    {
      track = self->tracks[i];

      if (track->channel && track_get_soloed (track))
        return true;
    }
  return false;
}

/**
 * Returns if the tracklist has listened tracks.
 */
NONNULL
bool
tracklist_has_listened (const Tracklist * self)
{
  Track * track;
  for (int i = 0; i < self->num_tracks; i++)
    {
      track = self->tracks[i];

      if (track->channel && track_get_listened (track))
        return true;
    }
  return false;
}

int
tracklist_get_num_muted_tracks (
  const Tracklist * self)
{
  int count = 0;
  for (int i = 0; i < self->num_tracks; i++)
    {
      Track * track = self->tracks[i];

      if (
        track_type_has_channel (track->type)
        && track_get_muted (track))
        {
          count++;
        }
    }

  return count;
}

int
tracklist_get_num_soloed_tracks (
  const Tracklist * self)
{
  int count = 0;
  for (int i = 0; i < self->num_tracks; i++)
    {
      Track * track = self->tracks[i];

      if (
        track_type_has_channel (track->type)
        && track_get_soloed (track))
        {
          count++;
        }
    }

  return count;
}

int
tracklist_get_num_listened_tracks (
  const Tracklist * self)
{
  int count = 0;
  for (int i = 0; i < self->num_tracks; i++)
    {
      Track * track = self->tracks[i];

      if (
        track_type_has_channel (track->type)
        && track_get_listened (track))
        {
          count++;
        }
    }

  return count;
}

/**
 * Fills in the given array (if non-NULL) with all
 * plugins in the tracklist and returns the number
 * of plugins.
 */
int
tracklist_get_plugins (
  const Tracklist * const self,
  GPtrArray *             arr)
{
  int total = 0;
  for (int i = 0; i < self->num_tracks; i++)
    {
      total +=
        track_get_plugins (self->tracks[i], arr);
    }

  return total;
}

/**
 * Activate or deactivate all plugins.
 *
 * This is useful for exporting: deactivating and
 * reactivating a plugin will reset its state.
 */
void
tracklist_activate_all_plugins (
  Tracklist * self,
  bool        activate)
{
  for (int i = 0; i < self->num_tracks; i++)
    {
      track_activate_all_plugins (
        self->tracks[i], activate);
    }
}

/**
 * @param visible 1 for visible, 0 for invisible.
 */
int
tracklist_get_num_visible_tracks (
  Tracklist * self,
  int         visible)
{
  int ret = 0;
  for (int i = 0; i < self->num_tracks; i++)
    {
      Track * track = self->tracks[i];

      if (track_get_should_be_visible (track) == visible)
        ret++;
    }

  return ret;
}

/**
 * Exposes each track's ports that should be
 * exposed to the backend.
 *
 * This should be called after setting up the
 * engine.
 */
void
tracklist_expose_ports_to_backend (Tracklist * self)
{
  g_return_if_fail (self);

  for (int i = 0; i < self->num_tracks; i++)
    {
      Track * track = self->tracks[i];
      g_return_if_fail (track);

      if (track_type_has_channel (track->type))
        {
          Channel * ch = track_get_channel (track);
          g_return_if_fail (
            IS_CHANNEL_AND_NONNULL (ch));
          channel_expose_ports_to_backend (ch);
        }
    }
}

/**
 * Handles a file drop inside the timeline or in
 * empty space in the tracklist.
 *
 * @param uri_list URI list, if URI list was dropped.
 * @param file File, if SupportedFile was dropped.
 * @param track Track, if any.
 * @param lane TrackLane, if any.
 * @param pos Position the file was dropped at, if
 *   inside track.
 * @param perform_actions Whether to perform
 *   undoable actions in addition to creating the
 *   regions/tracks.
 */
void
tracklist_handle_file_drop (
  Tracklist *     self,
  char **         uri_list,
  SupportedFile * orig_file,
  Track *         track,
  TrackLane *     lane,
  Position *      pos,
  bool            perform_actions)
{
  GPtrArray * file_arr =
    g_ptr_array_new_with_free_func (
      (GDestroyNotify) supported_file_free);
  if (orig_file)
    {
      SupportedFile * file =
        supported_file_clone (orig_file);
      g_ptr_array_add (file_arr, file);
    }
  else
    {
      g_return_if_fail (uri_list);

      char * uri = NULL;
      int    i = 0;
      while ((uri = uri_list[i++]) != NULL)
        {
          /* strip "file://" */
          if (!string_contains_substr (
                uri, "file://"))
            continue;

          GError * err = NULL;
          char *   filepath =
            g_filename_from_uri (uri, NULL, &err);
          if (err)
            {
              g_warning ("%s", err->message);
            }

          if (filepath)
            {
              SupportedFile * file =
                supported_file_new_from_path (
                  filepath);
              g_free (filepath);
              g_ptr_array_add (file_arr, file);
            }
        }
    }

  if (file_arr->len == 0)
    {
      ui_show_error_message (
        MAIN_WINDOW, true, _ ("No file was found"));

      goto free_file_array_and_return;
    }

  for (size_t i = 0; i < file_arr->len; i++)
    {
      SupportedFile * file =
        g_ptr_array_index (file_arr, i);

      TrackType track_type = 0;
      if (
        supported_file_type_is_supported (file->type)
        && supported_file_type_is_audio (file->type))
        {
          track_type = TRACK_TYPE_AUDIO;
        }
      else if (supported_file_type_is_midi (
                 file->type))
        {
          track_type = TRACK_TYPE_MIDI;
        }
      else
        {
          char * descr =
            supported_file_type_get_description (
              file->type);
          char * msg = g_strdup_printf (
            _ ("Unsupported file type %s"), descr);
          g_free (descr);
          ui_show_error_message (
            MAIN_WINDOW, true, msg);
          g_free (msg);

          goto free_file_array_and_return;
        }

      int num_nonempty_midi_tracks = 0;
      if (track_type == TRACK_TYPE_MIDI)
        {
          num_nonempty_midi_tracks =
            midi_file_get_num_tracks (
              file->abs_path, true);
          if (num_nonempty_midi_tracks == 0)
            {
              if (ZRYTHM_HAVE_UI)
                {
                  ui_show_error_message (
                    MAIN_WINDOW, true,
                    _ ("This MIDI file contains "
                       "no data"));
                }
              goto free_file_array_and_return;
            }
        }

      if (perform_actions)
        {
          /* if current track exists and track
           * type are incompatible, do nothing */
          if (track)
            {
              if (file_arr->len > 1)
                {
                  ui_show_error_message (
                    MAIN_WINDOW, true,
                    _ ("Can only drop 1 file at a "
                       "time on existing tracks"));
                  goto free_file_array_and_return;
                }

              if (track_type == TRACK_TYPE_MIDI)
                {
                  if (
                    track->type != TRACK_TYPE_MIDI
                    && track->type
                         != TRACK_TYPE_INSTRUMENT)
                    {
                      ui_show_error_message (
                        MAIN_WINDOW, true,
                        _ (
                          "Can only drop MIDI files on "
                          "MIDI/instrument tracks"));
                      goto free_file_array_and_return;
                    }

                  if (num_nonempty_midi_tracks > 1)
                    {
                      char msg[600];
                      sprintf (
                        msg,
                        _ (
                          "This MIDI file contains %d "
                          "tracks. It cannot be dropped "
                          "into an existing track"),
                        num_nonempty_midi_tracks);
                      ui_show_error_message (
                        MAIN_WINDOW, true, msg);
                      goto free_file_array_and_return;
                    }
                }
              else if (
                track_type == TRACK_TYPE_AUDIO
                && track->type != TRACK_TYPE_AUDIO)
                {
                  ui_show_error_message (
                    MAIN_WINDOW, true,
                    _ ("Can only drop audio files on "
                       "audio tracks"));
                  goto free_file_array_and_return;
                }

              int lane_pos =
                lane
                  ? lane->pos
                  : (
                    track->num_lanes == 1
                      ? 0
                      : track->num_lanes - 2);
              int idx_in_lane =
                track->lanes[lane_pos]->num_regions;
              ZRegion * region = NULL;
              switch (track_type)
                {
                case TRACK_TYPE_AUDIO:
                  /* create audio region in audio
                   * track */
                  region = audio_region_new (
                    -1, file->abs_path, true, NULL,
                    0, NULL, 0, 0, pos,
                    track_get_name_hash (track),
                    lane_pos, idx_in_lane);
                  break;
                case TRACK_TYPE_MIDI:
                  region =
                    midi_region_new_from_midi_file (
                      pos, file->abs_path,
                      track_get_name_hash (track),
                      lane_pos, idx_in_lane, 0);
                  break;
                default:
                  break;
                }

              if (region)
                {
                  track_add_region (
                    track, region, NULL, lane_pos,
                    F_GEN_NAME, F_PUBLISH_EVENTS);
                  arranger_object_select (
                    (ArrangerObject *) region,
                    F_SELECT, F_NO_APPEND,
                    F_NO_PUBLISH_EVENTS);

                  GError * err = NULL;
                  bool     ret =
                    arranger_selections_action_perform_create (
                      TL_SELECTIONS, &err);
                  if (!ret)
                    {
                      HANDLE_ERROR (
                        err, "%s",
                        _ ("Failed to create arranger "
                           "objects"));
                    }
                }
              else
                {
                  g_warn_if_reached ();
                }

              goto free_file_array_and_return;
            }
          else /* else if no track given */
            {
              GError * err = NULL;
              bool ret = track_create_with_action (
                track_type, NULL, file, pos,
                self->num_tracks, 1, &err);
              if (ret)
                {
                  UndoableAction * ua =
                    undo_manager_get_last_action (
                      UNDO_MANAGER);
                  ua->num_actions = i + 1;
                }
              else
                {
                  HANDLE_ERROR (
                    err, "%s",
                    _ ("Failed to create track"));
                  goto free_file_array_and_return;
                }
            }
        }
      else
        {
          g_warning ("operation not supported yet");
        }
    } /* foreach file */

free_file_array_and_return:
  g_ptr_array_unref (file_arr);

  return;
}

static void
move_after_copying_or_moving_inside (
  TracklistSelections * after_tls,
  int diff_between_track_below_and_parent)
{
  Track * lowest_cloned_track =
    tracklist_selections_get_lowest_track (
      after_tls);
  int lowest_cloned_track_pos =
    lowest_cloned_track->pos;

  GError * err = NULL;
  bool ret = tracklist_selections_action_perform_move (
    after_tls, PORT_CONNECTIONS_MGR,
    lowest_cloned_track_pos
      + diff_between_track_below_and_parent,
    &err);
  object_free_w_func_and_null (
    tracklist_selections_free, after_tls);
  if (!ret)
    {
      HANDLE_ERROR (
        err, "%s",
        _ ("Failed to move tracks after "
           "copying or moving inside folder"));
      return;
    }
  UndoableAction * ua =
    undo_manager_get_last_action (UNDO_MANAGER);
  ua->num_actions = 2;
}

/**
 * Handles a move or copy action based on a drag.
 *
 * @param this_track The track at the cursor (where
 *   the selection was dropped to.
 * @param location Location relative to @ref
 *   this_track.
 */
void
tracklist_handle_move_or_copy (
  Tracklist *          self,
  Track *              this_track,
  TrackWidgetHighlight location,
  GdkDragAction        action)
{
  g_debug (
    "%s: this track '%s' - location %s - "
    "action %s",
    __func__, this_track->name,
    track_widget_highlight_to_str (location),
    action == GDK_ACTION_COPY ? "copy" : "move");

  int pos = -1;
  if (location == TRACK_WIDGET_HIGHLIGHT_TOP)
    {
      pos = this_track->pos;
    }
  else
    {
      Track * next =
        tracklist_get_next_visible_track (
          TRACKLIST, this_track);
      if (next)
        pos = next->pos;
      /* else if last track, move to end */
      else if (
        this_track->pos
        == TRACKLIST->num_tracks - 1)
        pos = TRACKLIST->num_tracks;
      /* else if last visible track but not last
       * track */
      else
        pos = this_track->pos + 1;
    }

  if (pos == -1)
    return;

  tracklist_selections_select_foldable_children (
    TRACKLIST_SELECTIONS);

  if (action == GDK_ACTION_COPY)
    {
      if (tracklist_selections_contains_uncopyable_track (
            TRACKLIST_SELECTIONS))
        {
          g_message (
            "cannot copy - track selection "
            "contains uncopyable track");
          return;
        }

      if (location == TRACK_WIDGET_HIGHLIGHT_INSIDE)
        {
          GError * err = NULL;
          bool     ret =
            tracklist_selections_action_perform_copy_inside (
              TRACKLIST_SELECTIONS,
              PORT_CONNECTIONS_MGR,
              this_track->pos, &err);
          if (!ret)
            {
              HANDLE_ERROR (
                err, "%s",
                _ ("Failed to copy tracks inside"));
              return;
            }
        }
      /* else if not highlighted inside */
      else
        {
          TracklistSelections * tls =
            TRACKLIST_SELECTIONS;
          int num_tls = tls->num_tracks;
          TracklistSelections * after_tls = NULL;
          int diff_between_track_below_and_parent =
            0;
          bool copied_inside = false;
          if (pos < TRACKLIST->num_tracks)
            {
              Track * track_below =
                TRACKLIST->tracks[pos];
              Track * track_below_parent =
                track_get_direct_folder_parent (
                  track_below);
              tracklist_selections_sort (
                TRACKLIST_SELECTIONS, true);
              Track * cur_parent =
                TRACKLIST_SELECTIONS->tracks[0];

              if (track_below_parent)
                {
                  diff_between_track_below_and_parent =
                    track_below->pos
                    - track_below_parent->pos;
                }

              /* first copy inside new parent */
              if (
                track_below_parent
                && track_below_parent != cur_parent)
                {
                  GError * err = NULL;
                  bool     ret =
                    tracklist_selections_action_perform_copy_inside (
                      tls, PORT_CONNECTIONS_MGR,
                      track_below_parent->pos, &err);
                  if (!ret)
                    {
                      HANDLE_ERROR (
                        err, "%s",
                        _ ("Failed to copy track "
                           "inside"));
                      return;
                    }

                  after_tls =
                    tracklist_selections_new (
                      F_NOT_PROJECT);
                  for (int j = 1; j <= num_tls; j++)
                    {
                      err = NULL;
                      Track * clone_tr = track_clone (
                        TRACKLIST->tracks
                          [track_below_parent->pos
                           + j],
                        &err);
                      if (!clone_tr)
                        {
                          HANDLE_ERROR (
                            err, "%s",
                            _ ("Failed to clone "
                               "track"));
                          return;
                        }
                      tracklist_selections_add_track (
                        after_tls, clone_tr,
                        F_NO_PUBLISH_EVENTS);
                    }

                  copied_inside = true;
                }
            }

          /* if not copied inside, copy normally */
          if (!copied_inside)
            {
              GError * err = NULL;
              bool     ret =
                tracklist_selections_action_perform_copy (
                  tls, PORT_CONNECTIONS_MGR, pos,
                  &err);
              if (!ret)
                {
                  HANDLE_ERROR (
                    err, "%s",
                    _ ("Failed to copy tracks"));
                  return;
                }
            }
          /* else if copied inside and there is
           * a track difference, also move */
          else if (
            diff_between_track_below_and_parent
            != 0)
            {
              move_after_copying_or_moving_inside (
                after_tls,
                diff_between_track_below_and_parent);
            }
        }
    }
  else if (action == GDK_ACTION_MOVE)
    {
      if (location == TRACK_WIDGET_HIGHLIGHT_INSIDE)
        {
          if (tracklist_selections_contains_track (
                TRACKLIST_SELECTIONS, this_track))
            {
              if (!ZRYTHM_TESTING)
                {
                  ui_show_error_message (
                    MAIN_WINDOW, true,
                    _ ("Cannot drag folder into "
                       "itself"));
                }
              return;
            }
          /* else if selections do not contain the
           * track dragged into */
          else
            {
              GError * err = NULL;
              bool     ret =
                tracklist_selections_action_perform_move_inside (
                  TRACKLIST_SELECTIONS,
                  PORT_CONNECTIONS_MGR,
                  this_track->pos, &err);
              if (!ret)
                {
                  HANDLE_ERROR (
                    err, "%s",
                    _ ("Failed to move track "
                       "inside folder"));
                  return;
                }
            }
        }
      /* else if not highlighted inside */
      else
        {
          TracklistSelections * tls =
            TRACKLIST_SELECTIONS;
          int num_tls = tls->num_tracks;
          TracklistSelections * after_tls = NULL;
          int diff_between_track_below_and_parent =
            0;
          bool moved_inside = false;
          if (pos < TRACKLIST->num_tracks)
            {
              Track * track_below =
                TRACKLIST->tracks[pos];
              Track * track_below_parent =
                track_get_direct_folder_parent (
                  track_below);
              tracklist_selections_sort (
                TRACKLIST_SELECTIONS, true);
              Track * cur_parent =
                TRACKLIST_SELECTIONS->tracks[0];

              if (track_below_parent)
                {
                  diff_between_track_below_and_parent =
                    track_below->pos
                    - track_below_parent->pos;
                }

              /* first move inside new parent */
              if (
                track_below_parent
                && track_below_parent != cur_parent)
                {
                  GError * err = NULL;
                  bool     ret =
                    tracklist_selections_action_perform_move_inside (
                      tls, PORT_CONNECTIONS_MGR,
                      track_below_parent->pos, &err);
                  if (!ret)
                    {
                      HANDLE_ERROR (
                        err, "%s",
                        _ ("Failed to move track "
                           "inside folder"));
                      return;
                    }

                  after_tls =
                    tracklist_selections_new (
                      F_NOT_PROJECT);
                  for (int j = 1; j <= num_tls; j++)
                    {
                      err = NULL;
                      Track * clone_tr = track_clone (
                        TRACKLIST->tracks
                          [track_below_parent->pos
                           + j],
                        &err);
                      if (!clone_tr)
                        {
                          HANDLE_ERROR (
                            err, "%s",
                            _ ("Failed to clone "
                               "track"));
                          return;
                        }
                      tracklist_selections_add_track (
                        after_tls, clone_tr,
                        F_NO_PUBLISH_EVENTS);
                    }

                  moved_inside = true;
                }
            } /* endif moved to an existing track */

          /* if not moved inside, move normally */
          if (!moved_inside)
            {
              GError * err = NULL;
              bool     ret =
                tracklist_selections_action_perform_move (
                  tls, PORT_CONNECTIONS_MGR, pos,
                  &err);
              if (!ret)
                {
                  HANDLE_ERROR (
                    err, "%s",
                    _ ("Failed to move tracks"));
                  return;
                }
            }
          /* else if moved inside and there is
           * a track difference, also move */
          else if (
            diff_between_track_below_and_parent
            != 0)
            {
              move_after_copying_or_moving_inside (
                after_tls,
                diff_between_track_below_and_parent);
            }
        }
    } /* endif action is MOVE */
}

/**
 * Marks or unmarks all tracks for bounce.
 */
void
tracklist_mark_all_tracks_for_bounce (
  Tracklist * self,
  bool        bounce)
{
  for (int i = 0; i < self->num_tracks; i++)
    {
      Track * track = self->tracks[i];
      track_mark_for_bounce (
        track, bounce, F_MARK_REGIONS,
        F_NO_MARK_CHILDREN, F_NO_MARK_PARENTS);
    }
}

void
tracklist_get_total_bars (
  Tracklist * self,
  int *       total_bars)
{
  for (int i = 0; i < self->num_tracks; i++)
    {
      Track * track = self->tracks[i];
      track_get_total_bars (track, total_bars);
    }
}

void
tracklist_set_caches (Tracklist * self)
{
  for (int i = 0; i < self->num_tracks; i++)
    {
      Track * track = self->tracks[i];
      track_set_caches (track);
    }
}

/**
 * Only clones what is needed for project save.
 *
 * @param src Source tracklist. Must be the
 *   tracklist of the project in use.
 */
Tracklist *
tracklist_clone (Tracklist * src)
{
  Tracklist * self = object_new (Tracklist);
  self->schema_version = TRACKLIST_SCHEMA_VERSION;

  self->pinned_tracks_cutoff =
    src->pinned_tracks_cutoff;

  self->num_tracks = src->num_tracks;
  for (int i = 0; i < src->num_tracks; i++)
    {
      Track *  track = src->tracks[i];
      GError * err = NULL;
      self->tracks[i] = track_clone (track, &err);
      g_return_val_if_fail (self->tracks[i], NULL);
    }

  return self;
}

Tracklist *
tracklist_new (
  Project *         project,
  SampleProcessor * sample_processor)
{
  Tracklist * self = object_new (Tracklist);
  self->schema_version = TRACKLIST_SCHEMA_VERSION;
  self->project = project;
  self->sample_processor = sample_processor;

  if (project)
    {
      project->tracklist = self;
    }

  return self;
}

void
tracklist_free (Tracklist * self)
{
  g_message ("%s: freeing...", __func__);

  int num_tracks = self->num_tracks;

  for (int i = num_tracks - 1; i >= 0; i--)
    {
      Track * track = self->tracks[i];
      g_return_if_fail (
        IS_TRACK_AND_NONNULL (track));
      if (track != self->tempo_track)
        tracklist_remove_track (
          self, track, F_REMOVE_PL, F_FREE,
          F_NO_PUBLISH_EVENTS, F_NO_RECALC_GRAPH);
    }

  /* remove tempo track last (used when printing
   * positions) */
  if (self->tempo_track)
    {
      tracklist_remove_track (
        self, self->tempo_track, F_REMOVE_PL,
        F_FREE, F_NO_PUBLISH_EVENTS,
        F_NO_RECALC_GRAPH);
      self->tempo_track = NULL;
    }

  object_zero_and_free (self);

  g_message ("%s: done", __func__);
}
