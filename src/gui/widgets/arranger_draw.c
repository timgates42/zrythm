/*
 * Copyright (C) 2018-2020 Alexandros Theodotou <alex at zrythm dot org>
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
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "audio/control_port.h"
#include "audio/track_lane.h"
#include "audio/tracklist.h"
#include "gui/backend/arranger_object.h"
#include "gui/widgets/arranger_draw.h"
#include "gui/widgets/arranger_object.h"
#include "gui/widgets/bot_dock_edge.h"
#include "gui/widgets/center_dock.h"
#include "gui/widgets/clip_editor.h"
#include "gui/widgets/clip_editor_inner.h"
#include "gui/widgets/editor_ruler.h"
#include "gui/widgets/midi_arranger.h"
#include "gui/widgets/midi_editor_space.h"
#include "gui/widgets/piano_roll_keys.h"
#include "gui/widgets/ruler.h"
#include "gui/widgets/timeline_panel.h"
#include "gui/widgets/timeline_ruler.h"
#include "gui/widgets/track.h"
#include "gui/widgets/tracklist.h"
#include "project.h"
#include "settings/settings.h"
#include "utils/cairo.h"
#include "zrythm_app.h"

#define TYPE(x) ARRANGER_WIDGET_TYPE_##x

static void
draw_selections (
  ArrangerWidget * self,
  cairo_t *        cr,
  GdkRectangle *   rect)
{
  double offset_x, offset_y;
  offset_x =
    self->start_x + self->last_offset_x > 0 ?
    self->last_offset_x :
    1 - self->start_x;
  offset_y =
    self->start_y + self->last_offset_y > 0 ?
    self->last_offset_y :
    1 - self->start_y;

  /* if action is selecting and not selecting range
   * (in the case of timeline */
  switch (self->action)
    {
    case UI_OVERLAY_ACTION_SELECTING:
    case UI_OVERLAY_ACTION_DELETE_SELECTING:
      z_cairo_draw_selection (
        cr, self->start_x - rect->x,
        self->start_y - rect->y,
        offset_x, offset_y);
      break;
    case UI_OVERLAY_ACTION_RAMPING:
      cairo_set_source_rgba (
        cr, 0.9, 0.9, 0.9, 1.0);
      cairo_set_line_width (cr, 2.0);
      cairo_move_to (
        cr, self->start_x -  rect->x,
        self->start_y - rect->y);
      cairo_line_to (
        cr,
        (self->start_x + self->last_offset_x) -
          rect->x,
        (self->start_y + self->last_offset_y) -
          rect->y);
      cairo_stroke (cr);
      break;
    default:
      break;
    }
}

static void
draw_highlight (
  ArrangerWidget * self,
  cairo_t *        cr,
  GdkRectangle *   rect)
{
  if (!self->is_highlighted)
    {
      return;
    }

  z_cairo_draw_selection_with_color (
    cr, &UI_COLORS->bright_orange,
    (self->highlight_rect.x + 1) - rect->x,
    (self->highlight_rect.y + 1) - rect->y,
    self->highlight_rect.width - 1,
    self->highlight_rect.height - 1);
}

/**
 * @param rect Arranger's rectangle.
 */
static void
draw_arranger_object (
  ArrangerWidget * self,
  ArrangerObject * obj,
  cairo_t *        cr,
  GdkRectangle *   rect)
{
  /* loop once or twice (2nd time for transient) */
  for (int i = 0;
       i < 1 +
         (arranger_object_should_orig_be_visible (
           obj) &&
          arranger_object_is_selected (obj));
       i++)
    {
      /* if looping 2nd time (transient) */
      if (i == 1)
        {
          g_return_if_fail (obj->transient);
          obj = obj->transient;
        }

      arranger_object_set_full_rectangle (
        obj, self);

      /* only draw if the object's rectangle is
       * hit by the drawable region */
      if (ui_rectangle_overlap (
            &obj->full_rect, rect))
        {
          arranger_object_draw (
            obj, self, cr, rect);
        }
    }
}

/**
 * @param rect Arranger draw rectangle.
 */
static void
draw_playhead (
  ArrangerWidget * self,
  cairo_t *        cr,
  GdkRectangle *   rect)
{
  int px = arranger_widget_get_playhead_px (self);

  if (px >= rect->x && px <= rect->x + rect->width)
    {
      cairo_set_source_rgba (
        cr, 1, 0, 0, 1);
      switch (ui_get_detail_level ())
        {
        case UI_DETAIL_HIGH:
          cairo_rectangle (
            cr, (px - rect->x) - 1, 0, 2,
            rect->height);
          break;
        case UI_DETAIL_NORMAL:
        case UI_DETAIL_LOW:
          cairo_rectangle (
            cr, (int) (px - rect->x) - 1, 0, 2,
            (int) rect->height);
          break;
        }
      cairo_fill (cr);
      self->last_playhead_px = px;
    }
}

static void
draw_timeline_bg (
  ArrangerWidget * self,
  cairo_t *        cr,
  GdkRectangle *   rect)
{
  /* handle horizontal drawing for tracks */
  GtkWidget * tw_widget;
  gint track_start_offset;
  Track * track;
  TrackWidget * tw;
  int line_y, i, j;
  for (i = 0; i < TRACKLIST->num_tracks; i++)
    {
      track = TRACKLIST->tracks[i];

      /* skip tracks in the other timeline (pinned/
       * non-pinned) */
      if (!track->visible ||
          (!self->is_pinned &&
           track->pinned) ||
          (self->is_pinned &&
           !track->pinned))
        continue;

      /* draw line below track */
      tw = track->widget;
      if (!GTK_IS_WIDGET (tw))
        continue;
      tw_widget = (GtkWidget *) tw;

      int full_track_height =
        track_get_full_visible_height (track);

      gtk_widget_translate_coordinates (
        tw_widget,
        GTK_WIDGET (
          self->is_pinned ?
            MW_TRACKLIST->pinned_box :
            MW_TRACKLIST->unpinned_box),
        0, 0, NULL, &track_start_offset);

      line_y =
        track_start_offset + full_track_height;

      if (line_y >= rect->y &&
          line_y < rect->y + rect->height)
        {
          cairo_set_source_rgb (
            self->cached_cr, 0.3, 0.3, 0.3);
          cairo_rectangle (
            cr, 0, (line_y - rect->y) - 1,
            rect->width, 2);
          cairo_fill (cr);
        }

      int total_height = track->main_height;

#define OFFSET_PLUS_TOTAL_HEIGHT \
  (track_start_offset + total_height)

      /* --- draw lanes --- */

      if (track->lanes_visible)
        {
          for (j = 0; j < track->num_lanes; j++)
            {
              TrackLane * lane = track->lanes[j];

              /* horizontal line above lane */
              if (OFFSET_PLUS_TOTAL_HEIGHT >
                    rect->y &&
                  OFFSET_PLUS_TOTAL_HEIGHT  <
                    rect->y + rect->height)
                {
                  z_cairo_draw_horizontal_line (
                    cr,
                    OFFSET_PLUS_TOTAL_HEIGHT -
                      rect->y,
                    0, rect->width, 0.5, 0.4);
                }

              total_height += lane->height;
            }
        }

      /* --- draw automation related stuff --- */

      /* skip tracks without visible automation */
      if (!track->automation_visible)
        continue;

      AutomationTracklist * atl =
        track_get_automation_tracklist (track);
      if (atl)
        {
          AutomationTrack * at;
          for (j = 0; j < atl->num_ats; j++)
            {
              at = atl->ats[j];

              if (!at->created || !at->visible)
                continue;

              /* horizontal line above automation
               * track */
              if (OFFSET_PLUS_TOTAL_HEIGHT >
                    rect->y &&
                  OFFSET_PLUS_TOTAL_HEIGHT  <
                    rect->y + rect->height)
                {
                  z_cairo_draw_horizontal_line (
                    cr,
                    OFFSET_PLUS_TOTAL_HEIGHT -
                      rect->y,
                    0, rect->width, 0.5, 0.2);
                }

              float normalized_val =
                automation_track_get_val_at_pos (
                  at, PLAYHEAD, true);
              Port * port =
                automation_track_get_port (at);
              AutomationPoint * ap =
                automation_track_get_ap_before_pos (
                  at, PLAYHEAD);
              if (!ap)
                {
                  normalized_val =
                    control_port_real_val_to_normalized (
                      port,
                      control_port_get_val (port));
                }

              int y_px =
                automation_track_get_y_px_from_normalized_val (
                  at,
                  normalized_val);

              /* line at current val */
              cairo_set_source_rgba (
                cr,
                track->color.red,
                track->color.green,
                track->color.blue,
                0.3);
              cairo_rectangle (
                cr, 0,
                (OFFSET_PLUS_TOTAL_HEIGHT + y_px) -
                  rect->y,
                rect->width, 1);
              cairo_fill (cr);

              /* show shade under the line */
              /*cairo_set_source_rgba (*/
                /*cr,*/
                /*track->color.red,*/
                /*track->color.green,*/
                /*track->color.blue,*/
                /*0.06);*/
              /*cairo_rectangle (*/
                /*cr,*/
                /*0,*/
                /*(OFFSET_PLUS_TOTAL_HEIGHT + y_px) -*/
                  /*rect->y,*/
                /*rect->width,*/
                /*at->height - y_px);*/
              /*cairo_fill (cr);*/

              total_height += at->height;
            }
        }
    }
}

static void
draw_borders (
  ArrangerWidget * self,
  cairo_t *        cr,
  int              x_from,
  int              x_to,
  double           y_offset)
{
  cairo_set_source_rgb (cr, 0.7, 0.7, 0.7);
  cairo_rectangle (
    cr, x_from, (int) y_offset, x_to - x_from, 0.5);
  cairo_fill (cr);
}

static void
draw_midi_bg (
  ArrangerWidget * self,
  cairo_t *        cr,
  GdkRectangle *   rect)
{
  /* px per key adjusted for border width */
  double adj_px_per_key =
    MW_PIANO_ROLL_KEYS->px_per_key + 1.0;
  /*double adj_total_key_px =*/
    /*MW_PIANO_ROLL->total_key_px + 126;*/

  /*handle horizontal drawing*/
  double y_offset;
  for (int i = 0; i < 128; i++)
    {
      y_offset =
        adj_px_per_key * i;
      /* if key is visible */
      if (y_offset > rect->y &&
          y_offset < (rect->y + rect->height))
        {
          draw_borders (
            self, cr, 0, rect->width,
            y_offset - rect->y);
          if (piano_roll_is_key_black (
                PIANO_ROLL->piano_descriptors[i].
                  value))
            {
              cairo_set_source_rgba (
                cr, 0, 0, 0, 0.2);
              cairo_rectangle (
                cr, 0,
                /* + 1 since the border is
                 * bottom */
                (int) ((y_offset - rect->y) + 1),
                rect->width, (int) adj_px_per_key);
              cairo_fill (cr);
            }
        }
      if ((PIANO_ROLL->drum_mode &&
          PIANO_ROLL->drum_descriptors[i].value ==
            MW_MIDI_ARRANGER->hovered_note) ||
          (!PIANO_ROLL->drum_mode &&
           PIANO_ROLL->piano_descriptors[i].value ==
             MW_MIDI_ARRANGER->hovered_note))
        {
          cairo_set_source_rgba (
            cr, 1, 1, 1, 0.06);
          cairo_rectangle (
            cr, 0,
            /* + 1 since the border is bottom */
            (y_offset - rect->y) + 1,
            rect->width, adj_px_per_key);
          cairo_fill (cr);
        }
    }
}

static void
draw_audio_bg (
  ArrangerWidget * self,
  cairo_t *        cr,
  GdkRectangle *   rect)
{
  ZRegion * ar =
    clip_editor_get_region (CLIP_EDITOR);
  if (!ar)
    {
      g_message (
        "audio region not found, skipping draw");
      return;
    }
  if (ar->stretching)
    {
      arranger_widget_redraw_whole (self);
      return;
    }
  ArrangerObject * obj = (ArrangerObject *) ar;
  TrackLane * lane = region_get_lane (ar);
  Track * track =
    track_lane_get_track (lane);
  g_return_if_fail (lane);
  GdkRGBA * color = &track->color;
  cairo_set_source_rgba (
    cr, color->red + 0.3, color->green + 0.3,
    color->blue + 0.3, 0.9);
  cairo_set_line_width (cr, 1);

  int height =
    gtk_widget_get_allocated_height (
      GTK_WIDGET (self));

  AudioClip * clip =
    AUDIO_POOL->clips[ar->pool_id];

  double local_start_x =
    (double) rect->x;
  double local_end_x =
    local_start_x +
    (double) rect->width;

  /* frames in the clip to start drawing from */
  long prev_frames =
    MAX (
      ui_px_to_frames_editor (local_start_x, 1) -
        obj->pos.frames,
      0);

  UiDetail detail = ui_get_detail_level ();
  double increment = 1;
  double width = 1;
  switch (detail)
    {
    case UI_DETAIL_HIGH:
      increment = 0.5;
      width = 1;
      break;
    case UI_DETAIL_NORMAL:
      increment = 1;
      width = 1;
      break;
    case UI_DETAIL_LOW:
      increment = 2;
      width = 2;
      break;
    }

  for (double i = local_start_x;
       i < local_end_x; i += increment)
    {
      long curr_frames =
        ui_px_to_frames_editor (i, 1) -
          obj->pos.frames;
      if (curr_frames < 0)
        continue;

      float min = 0.f, max = 0.f;
      for (long j = prev_frames;
           j < curr_frames; j++)
        {
          if (j >= (long) ar->num_frames)
            break;
          for (unsigned int k = 0;
               k < clip->channels; k++)
            {
              long index =
                j * (long) clip->channels + (long) k;
              g_return_if_fail (
                index >= 0 &&
                index <
                  (long)
                  (ar->num_frames *
                     clip->channels));
              float val =
                ar->frames[index];
              if (val > max)
                max = val;
              if (val < min)
                min = val;
            }
        }
#define DRAW_VLINE(cr,x,from_y,_height) \
  switch (detail) \
    { \
    case UI_DETAIL_HIGH: \
      cairo_rectangle ( \
        cr, x, from_y, \
        width, _height); \
      break; \
    case UI_DETAIL_NORMAL: \
    case UI_DETAIL_LOW: \
      cairo_rectangle ( \
        cr, (int) (x), (int) (from_y), \
        width, (int) _height); \
      break; \
    } \
  cairo_fill (cr)

      min = (min + 1.f) / 2.f; /* normallize */
      max = (max + 1.f) / 2.f; /* normalize */
      double from_y =
        MAX (
          (double) min * (double) height, 0.0) - rect->y;
      double draw_height =
        (MIN (
          (double) max * (double) height,
          (double) height) - rect->y) - from_y;
      DRAW_VLINE (
        cr,
        /* x */
        i - rect->x,
        /* from y */
        from_y,
        /* to y */
        draw_height);

      if (curr_frames >= clip->num_frames)
        break;

      prev_frames = curr_frames;
    }
#undef DRAW_VLINE
  cairo_fill (cr);
}

static void
draw_vertical_lines (
  ArrangerWidget * self,
  RulerWidget *    ruler,
  cairo_t *        cr,
  GdkRectangle *   rect)
{
  /* if time display */
  if (g_settings_get_enum (
        S_UI, "ruler-display") ==
          TRANSPORT_DISPLAY_TIME)
    {
      /* get sec interval */
      int sec_interval =
        ruler_widget_get_sec_interval (ruler);

      /* get 10 sec interval */
      int ten_sec_interval =
        ruler_widget_get_10sec_interval (ruler);

      /* get the interval for mins */
      int min_interval =
        (int)
        MAX ((RW_PX_TO_HIDE_BEATS) /
             (double) ruler->px_per_min, 1.0);

      int i = 0;
      double curr_px;
      while (
        (curr_px =
           ruler->px_per_min * (i += min_interval) +
             SPACE_BEFORE_START) <
         rect->x + rect->width)
        {
          if (curr_px < rect->x)
            continue;

          cairo_set_source_rgb (
            cr, 0.3, 0.3, 0.3);
          double x = curr_px - rect->x;
          cairo_rectangle (
            cr, (int) x, 0,
            1, rect->height);
          cairo_fill (cr);
        }
      i = 0;
      if (ten_sec_interval > 0)
        {
          while ((curr_px =
                  ruler->px_per_10sec *
                    (i += ten_sec_interval) +
                  SPACE_BEFORE_START) <
                 rect->x + rect->width)
            {
              if (curr_px < rect->x)
                continue;

              cairo_set_source_rgba (
                cr, 0.25, 0.25, 0.25,
                0.6);
              double x = curr_px - rect->x;
              cairo_rectangle (
                cr, (int) x, 0,
                1, rect->height);
              cairo_fill (cr);
            }
        }
      i = 0;
      if (sec_interval > 0)
        {
          while ((curr_px =
                  ruler->px_per_sec *
                    (i += sec_interval) +
                  SPACE_BEFORE_START) <
                 rect->x + rect->width)
            {
              if (curr_px < rect->x)
                continue;

              cairo_set_source_rgb (
                cr, 0.2, 0.2, 0.2);
              cairo_set_line_width (cr, 0.5);
              double x = curr_px - rect->x;
              cairo_move_to (cr, x, 0);
              cairo_line_to (cr, x, rect->height);
              cairo_stroke (cr);
            }
        }
    }
  /* else if BBT display */
  else
    {
      /* get sixteenth interval */
      int sixteenth_interval =
        ruler_widget_get_sixteenth_interval (
          ruler);

      /* get the beat interval */
      int beat_interval =
        ruler_widget_get_beat_interval (
          ruler);

      /* get the interval for bars */
      int bar_interval =
        (int)
        MAX ((RW_PX_TO_HIDE_BEATS) /
             (double) ruler->px_per_bar, 1.0);

      int i = 0;
      double curr_px;
      while (
        (curr_px =
           ruler->px_per_bar * (i += bar_interval) +
             SPACE_BEFORE_START) <
         rect->x + rect->width)
        {
          if (curr_px < rect->x)
            continue;

          cairo_set_source_rgb (
            cr, 0.3, 0.3, 0.3);
          double x = curr_px - rect->x;
          cairo_rectangle (
            cr, (int) x, 0,
            1, rect->height);
          cairo_fill (cr);
        }
      i = 0;
      if (beat_interval > 0)
        {
          while ((curr_px =
                  ruler->px_per_beat *
                    (i += beat_interval) +
                  SPACE_BEFORE_START) <
                 rect->x + rect->width)
            {
              if (curr_px < rect->x)
                continue;

              cairo_set_source_rgba (
                cr, 0.25, 0.25, 0.25,
                0.6);
              double x = curr_px - rect->x;
              cairo_rectangle (
                cr, (int) x, 0,
                1, rect->height);
              cairo_fill (cr);
            }
        }
      i = 0;
      if (sixteenth_interval > 0)
        {
          while ((curr_px =
                  ruler->px_per_sixteenth *
                    (i += sixteenth_interval) +
                  SPACE_BEFORE_START) <
                 rect->x + rect->width)
            {
              if (curr_px < rect->x)
                continue;

              cairo_set_source_rgb (
                cr, 0.2, 0.2, 0.2);
              cairo_set_line_width (cr, 0.5);
              double x = curr_px - rect->x;
              cairo_move_to (cr, x, 0);
              cairo_line_to (cr, x, rect->height);
              cairo_stroke (cr);
            }
        }
    }
}

static void
draw_range (
  ArrangerWidget * self,
  int              range_first_px,
  int              range_second_px,
  GdkRectangle *   rect,
  cairo_t *        cr)
{
  /* draw range */
  cairo_set_source_rgba (
    cr, 0.3, 0.3, 0.3, 0.3);
  cairo_rectangle (
    cr,
    MAX (0, range_first_px - rect->x), 0,
    range_second_px -
      MAX (rect->x, range_first_px),
    rect->height);
  cairo_fill (cr);
  cairo_set_source_rgba (
    cr, 0.8, 0.8, 0.8, 0.4);
  cairo_set_line_width (cr, 2);

  /* if start is within the screen */
  if (range_first_px > rect->x &&
      range_first_px <= rect->x + rect->width)
    {
      /* draw the start line */
      double x =
        (range_first_px - rect->x) + 1.0;
      cairo_move_to (
        cr, x, 0);
      cairo_line_to (
        cr, x, rect->height);
      cairo_stroke (cr);
    }
  /* if end is within the screen */
  if (range_second_px > rect->x &&
      range_second_px < rect->x + rect->width)
    {
      double x =
        (range_second_px - rect->x) - 1.0;
      cairo_move_to (
        cr, x, 0);
      cairo_line_to (
        cr, x, rect->height);
      cairo_stroke (cr);
    }
}

gboolean
arranger_draw_cb (
  GtkWidget *      widget,
  cairo_t *        cr,
  ArrangerWidget * self)
{
  /*g_message ("drawing arranger %p", self);*/
  RulerWidget * ruler =
    self->type == TYPE (TIMELINE) ?
    MW_RULER : EDITOR_RULER;
  if (ruler->px_per_bar < 2.0)
    return FALSE;

  if (self->first_draw)
    {
      self->first_draw = false;

      GtkScrolledWindow * scroll =
        arranger_widget_get_scrolled_window (self);
      GtkAdjustment * hadj =
        gtk_scrolled_window_get_hadjustment (
          scroll);
      GtkAdjustment * vadj =
        gtk_scrolled_window_get_vadjustment (
          scroll);

      EditorSettings * settings =
        arranger_widget_get_editor_settings (self);

      int new_x = settings->scroll_start_x;
      int new_y = settings->scroll_start_y;
      if (self->type == TYPE (TIMELINE) &&
          self->is_pinned)
        {
          new_y = 0;
        }
      else if (self->type == TYPE (MIDI_MODIFIER))
        {
          new_y = 0;
        }

      g_debug (
        "setting arranger adjustment to %d, %d",
        new_x, new_y);

      gtk_adjustment_set_value (hadj, new_x);
      gtk_adjustment_set_value (vadj, new_y);
    }

  GdkRectangle rect;
  gdk_cairo_get_clip_rectangle (
    cr, &rect);

  if (self->redraw ||
      !gdk_rectangle_equal (
         &rect, &self->last_rect))
    {
      /*g_message (*/
        /*"redrawing arranger in rect: "*/
        /*"(%d, %d) width: %d height %d)",*/
        /*rect.x, rect.y, rect.width, rect.height);*/
      self->last_rect = rect;

      GtkStyleContext *context =
        gtk_widget_get_style_context (widget);

      z_cairo_reset_caches (
        &self->cached_cr,
        &self->cached_surface, rect.width,
        rect.height, cr);

      gtk_render_background (
        context, self->cached_cr, 0, 0,
        rect.width, rect.height);

      /* draw loop background */
      if (TRANSPORT->loop)
        {
          double start_px = 0, end_px = 0;
          if (self->type == TYPE (TIMELINE))
            {
              start_px =
                ui_pos_to_px_timeline (
                  &TRANSPORT->loop_start_pos, 1);
              end_px =
                ui_pos_to_px_timeline (
                  &TRANSPORT->loop_end_pos, 1);
            }
          else
            {
              start_px =
                ui_pos_to_px_editor (
                  &TRANSPORT->loop_start_pos, 1);
              end_px =
                ui_pos_to_px_editor (
                  &TRANSPORT->loop_end_pos, 1);
            }
          cairo_set_source_rgba (
            self->cached_cr, 0, 0.9, 0.7, 0.08);
          cairo_set_line_width (
            self->cached_cr, 2);

          /* if transport loop start is within the
           * screen */
          if (start_px > rect.x &&
              start_px <= rect.x + rect.width)
            {
              /* draw the loop start line */
              double x =
                (start_px - rect.x) + 1.0;
              cairo_rectangle (
                self->cached_cr,
                (int) x, 0, 2, rect.height);
              cairo_fill (self->cached_cr);
            }
          /* if transport loop end is within the
           * screen */
          if (end_px > rect.x &&
              end_px < rect.x + rect.width)
            {
              double x =
                (end_px - rect.x) - 1.0;
              cairo_rectangle (
                self->cached_cr,
                (int) x, 0, 2, rect.height);
              cairo_fill (self->cached_cr);
            }

          /* draw transport loop area */
          cairo_set_source_rgba (
            self->cached_cr, 0, 0.9, 0.7, 0.02);
          double loop_start_local_x =
            MAX (0, start_px - rect.x);
          cairo_rectangle (
            self->cached_cr,
            (int) loop_start_local_x, 0,
            (int) (end_px - MAX (rect.x, start_px)),
            rect.height);
          cairo_fill (self->cached_cr);
        }

      /* --- handle vertical drawing --- */

      draw_vertical_lines (
        self, ruler, self->cached_cr, &rect);

      /* draw range */
      int range_first_px, range_second_px;
      bool have_range = false;
      if (self->type == TYPE (AUDIO) &&
          AUDIO_SELECTIONS->has_selection)
        {
          Position * range_first_pos,
                   * range_second_pos;
          if (position_is_before_or_equal (
                &TRANSPORT->range_1,
                &TRANSPORT->range_2))
            {
              range_first_pos =
                &AUDIO_SELECTIONS->sel_start;
              range_second_pos =
                &AUDIO_SELECTIONS->sel_end;
            }
          else
            {
              range_first_pos =
                &AUDIO_SELECTIONS->sel_end;
              range_second_pos =
                &AUDIO_SELECTIONS->sel_start;
            }

          range_first_px =
            ui_pos_to_px_editor (
              range_first_pos, 1);
          range_second_px =
            ui_pos_to_px_editor (
              range_second_pos, 1);
          have_range = true;
        }
      else if (self->type == TYPE (TIMELINE) &&
          TRANSPORT->has_range)
        {
          /* in order they appear */
          Position * range_first_pos,
                   * range_second_pos;
          if (position_is_before_or_equal (
                &TRANSPORT->range_1,
                &TRANSPORT->range_2))
            {
              range_first_pos = &TRANSPORT->range_1;
              range_second_pos =
                &TRANSPORT->range_2;
            }
          else
            {
              range_first_pos = &TRANSPORT->range_2;
              range_second_pos =
                &TRANSPORT->range_1;
            }

          range_first_px =
            ui_pos_to_px_timeline (
              range_first_pos, 1);
          range_second_px =
            ui_pos_to_px_timeline (
              range_second_pos, 1);
          have_range = true;
        }

      if (have_range)
        {
          draw_range (
            self, range_first_px, range_second_px,
            &rect, self->cached_cr);
        }

      if (self->type == TYPE (TIMELINE))
        {
          draw_timeline_bg (
            self, self->cached_cr, &rect);
        }
      else if (self->type == TYPE (MIDI))
        {
          draw_midi_bg (
            self, self->cached_cr, &rect);
        }
      else if (self->type == TYPE (AUDIO))
        {
          draw_audio_bg (
            self, self->cached_cr, &rect);
        }

      /* draw each arranger object */
      ArrangerObject * objs[2000];
      int num_objs;
      arranger_widget_get_hit_objects_in_rect (
        self, ARRANGER_OBJECT_TYPE_ALL, &rect,
        objs, &num_objs);

      /*g_message (*/
        /*"objects found: %d (is pinned %d)",*/
        /*num_objs, self->is_pinned);*/
      /* note: these are only project objects */
      for (int j = 0; j < num_objs; j++)
        {
          draw_arranger_object (
            self, objs[j], self->cached_cr,
            &rect);
        }

      /* draw dnd highlight */
      draw_highlight (
        self, self->cached_cr, &rect);

      /* draw selections */
      draw_selections (
        self, self->cached_cr, &rect);

      draw_playhead (self, self->cached_cr, &rect);

      self->redraw--;
    }

  cairo_set_source_surface (
    cr, self->cached_surface, rect.x, rect.y);
  cairo_paint (cr);

  return FALSE;
}
