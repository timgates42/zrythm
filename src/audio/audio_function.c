// SPDX-FileCopyrightText: © 2020-2022 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include <inttypes.h>

#include "audio/audio_function.h"
#include "audio/audio_region.h"
#include "audio/engine.h"
#include "gui/backend/arranger_selections.h"
#include "gui/backend/event.h"
#include "gui/backend/event_manager.h"
#include "gui/widgets/main_window.h"
#include "plugins/lv2/lv2_ui.h"
#include "plugins/lv2_plugin.h"
#include "plugins/plugin_gtk.h"
#include "plugins/plugin_manager.h"
#include "project.h"
#include "settings/settings.h"
#include "utils/debug.h"
#include "utils/dsp.h"
#include "utils/error.h"
#include "utils/flags.h"
#include "utils/gtk.h"
#include "utils/string.h"
#include "zrythm_app.h"

#include <glib/gi18n.h>

typedef enum
{
  Z_AUDIO_AUDIO_FUNCTION_ERROR_INVALID_POSITIONS,
} ZAudioAudioFunctionError;

#define Z_AUDIO_AUDIO_FUNCTION_ERROR \
  z_audio_audio_function_error_quark ()
GQuark
z_audio_audio_function_error_quark (void);
G_DEFINE_QUARK (
  z-audio-audio-function-error-quark, z_audio_audio_function_error)

char *
audio_function_get_action_target_for_type (
  AudioFunctionType type)
{
  const char * type_str =
    audio_function_type_to_string (type);
  char * type_str_lower = g_strdup (type_str);
  string_to_lower (type_str, type_str_lower);
  char * substituted =
    string_replace (type_str_lower, " ", "-");
  g_free (type_str_lower);

  return substituted;
}

/**
 * Returns a detailed action name to be used for
 * actionable widgets or menus.
 *
 * @param base_action Base action to use.
 */
char *
audio_function_get_detailed_action_for_type (
  AudioFunctionType type,
  const char *      base_action)
{
  char * target =
    audio_function_get_action_target_for_type (type);
  char * ret = g_strdup_printf (
    "%s::%s", base_action, target);
  g_free (target);

  return ret;
}

const char *
audio_function_get_icon_name_for_type (
  AudioFunctionType type)
{
  switch (type)
    {
    case AUDIO_FUNCTION_INVERT:
      return "edit-select-invert";
    case AUDIO_FUNCTION_REVERSE:
      return "path-reverse";
    case AUDIO_FUNCTION_NORMALIZE_PEAK:
      return "kt-set-max-upload-speed";
    case AUDIO_FUNCTION_LINEAR_FADE_IN:
      return "arena-fade-in";
    case AUDIO_FUNCTION_LINEAR_FADE_OUT:
      return "arena-fade-out";
    case AUDIO_FUNCTION_NUDGE_LEFT:
      return "arrow-left";
    case AUDIO_FUNCTION_NUDGE_RIGHT:
      return "arrow-right";
    case AUDIO_FUNCTION_NORMALIZE_RMS:
    case AUDIO_FUNCTION_NORMALIZE_LUFS:
    default:
      return "modulator";
      break;
    }

  g_return_val_if_reached (NULL);
}

/**
 * @param frames Interleaved frames.
 * @param num_frames Number of frames per channel.
 * @param channels Number of channels.
 *
 * @return Non-zero if fail
 */
static int
apply_plugin (
  const char * uri,
  float *      frames,
  size_t       num_frames,
  channels_t   channels,
  GError **    error)
{
  PluginDescriptor * descr =
    plugin_manager_find_plugin_from_uri (
      PLUGIN_MANAGER, uri);
  g_return_val_if_fail (descr, -1);
  PluginSetting * setting =
    plugin_setting_new_default (descr);
  g_return_val_if_fail (setting, -1);
  setting->force_generic_ui = true;
  GError * err = NULL;
  Plugin * pl = plugin_new_from_setting (
    setting, 0, PLUGIN_SLOT_INSERT, 0, &err);
  if (!IS_PLUGIN_AND_NONNULL (pl))
    {
      PROPAGATE_PREFIXED_ERROR (
        error, err, "%s",
        _ ("Failed to create plugin"));
      return -1;
    }
  pl->is_function = true;
  int ret = plugin_instantiate (pl, NULL, &err);
  if (ret != 0)
    {
      PROPAGATE_PREFIXED_ERROR (
        error, err, "%s",
        _ ("Failed to instantiate plugin"));
      return -1;
    }
  ret = plugin_activate (pl, true);
  g_return_val_if_fail (ret == 0, -1);
  plugin_setting_free (setting);

  /* create window */
  pl->window = GTK_WINDOW (gtk_dialog_new ());
  gtk_window_set_title (pl->window, descr->name);
  gtk_window_set_icon_name (pl->window, "zrythm");
  /*gtk_window_set_role (*/
  /*pl->window, "plugin_ui");*/
  gtk_window_set_modal (pl->window, true);
  /*gtk_window_set_attached_to (*/
  /*pl->window, GTK_WIDGET (MAIN_WINDOW));*/

  /* add vbox for stacking elements */
  pl->vbox = GTK_BOX (
    gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
  GtkWidget * container =
    gtk_dialog_get_content_area (
      GTK_DIALOG (pl->window));
  gtk_box_append (
    GTK_BOX (container), GTK_WIDGET (pl->vbox));

#if 0
  /* add menu bar */
  plugin_gtk_build_menu (
    pl, GTK_WIDGET (pl->window),
    GTK_WIDGET (pl->vbox));
#endif

  /* Create/show alignment to contain UI (whether
   * custom or generic) */
  pl->ev_box = GTK_BOX (
    gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 0));
  gtk_box_append (
    pl->vbox, GTK_WIDGET (pl->ev_box));
  gtk_widget_set_vexpand (
    GTK_WIDGET (pl->ev_box), true);

  /* open */
  plugin_gtk_open_generic_ui (
    pl, F_NO_PUBLISH_EVENTS);

  ret = z_gtk_dialog_run (
    GTK_DIALOG (pl->window), false);

  Port * l_in = NULL;
  Port * r_in = NULL;
  Port * l_out = NULL;
  Port * r_out = NULL;
  for (int i = 0; i < pl->num_out_ports; i++)
    {
      Port * port = pl->out_ports[i];
      if (port->id.type == TYPE_AUDIO)
        {
          if (l_out)
            {
              r_out = port;
              break;
            }
          else
            {
              l_out = port;
            }
        }
    }
  int num_plugin_channels = r_out ? 2 : 1;
  for (int i = 0; i < pl->num_in_ports; i++)
    {
      Port * port = pl->in_ports[i];
      if (port->id.type == TYPE_AUDIO)
        {
          if (l_in)
            {
              r_in = port;
              break;
            }
          else
            {
              l_in = port;
              if (num_plugin_channels == 1)
                break;
            }
        }
    }
  if (num_plugin_channels == 1)
    {
      r_in = l_in;
      r_out = l_out;
    }

  size_t step =
    MIN (AUDIO_ENGINE->block_length, num_frames);
  size_t i = 0; /* frames processed */
  plugin_update_latency (pl);
  nframes_t latency = pl->latency;
  while (i < num_frames)
    {
      for (size_t j = 0; j < step; j++)
        {
          g_return_val_if_fail (l_in, -1);
          g_return_val_if_fail (r_in, -1);
          l_in->buf[j] = frames[(i + j) * channels];
          r_in->buf[j] =
            frames[(i + j) * channels + 1];
        }
      lv2_ui_read_and_apply_events (pl->lv2, step);
      lilv_instance_run (pl->lv2->instance, step);
      for (size_t j = 0; j < step; j++)
        {
          signed_frame_t actual_j =
            (signed_frame_t) (i + j)
            - (signed_frame_t) latency;
          if (actual_j < 0)
            continue;
#if 0
          g_message (
            "%ld %f",
            actual_j,
            fabsf (l_out->buf[j]));
#endif
          g_return_val_if_fail (l_out, -1);
          g_return_val_if_fail (r_out, -1);
          frames
            [actual_j * (signed_frame_t) channels] =
              l_out->buf[j];
          frames
            [actual_j * (signed_frame_t) channels
             + 1] = r_out->buf[j];
        }
      if (i > latency)
        {
          plugin_update_latency (pl);
          /*g_message ("end latency %d", pl->latency);*/
        }
      i += step;
      step = MIN (step, num_frames - i);
    }

  /* handle latency */
  i = 0;
  step = MIN (AUDIO_ENGINE->block_length, latency);
  while (i < latency)
    {
      for (size_t j = 0; j < step; j++)
        {
          g_return_val_if_fail (l_in, -1);
          g_return_val_if_fail (r_in, -1);
          l_in->buf[j] = 0.f;
          r_in->buf[j] = 0.f;
        }
      lv2_ui_read_and_apply_events (pl->lv2, step);
      lilv_instance_run (pl->lv2->instance, step);
      for (size_t j = 0; j < step; j++)
        {
          signed_frame_t actual_j =
            (signed_frame_t) (i + j + num_frames)
            - (signed_frame_t) latency;
          g_return_val_if_fail (actual_j >= 0, -1);
#if 0
          g_message (
            "%ld %f",
            actual_j,
            fabsf (l_out->buf[j]));
#endif
          g_return_val_if_fail (l_out, -1);
          g_return_val_if_fail (r_out, -1);
          frames
            [actual_j * (signed_frame_t) channels] =
              l_out->buf[j];
          frames
            [actual_j * (signed_frame_t) channels
             + 1] = r_out->buf[j];
        }
      i += step;
      step = MIN (step, latency - i);
    }
  plugin_update_latency (pl);
  g_message ("end latency %d", pl->latency);

  plugin_gtk_close_ui (pl);
  plugin_free (pl);

  return 0;
}

/**
 * Applies the given action to the given selections.
 *
 * This will save a file in the pool and store the
 * pool ID in the selections.
 *
 * @param sel Selections to edit.
 * @param type Function type. If invalid is passed,
 *   this will simply add the audio file in the pool
 *   for the unchanged audio material (used in
 *   audio selection actions for the selections
 *   before the change).
 *
 * @return Non-zero if error.
 */
int
audio_function_apply (
  ArrangerSelections * sel,
  AudioFunctionType    type,
  const char *         uri,
  GError **            error)
{
  g_message (
    "applying %s...",
    audio_function_type_to_string (type));

  AudioSelections * audio_sel =
    (AudioSelections *) sel;

  ZRegion * r = region_find (&audio_sel->region_id);
  g_return_val_if_fail (r, -1);
  Track * tr = arranger_object_get_track (
    (ArrangerObject *) r);
  g_return_val_if_fail (tr, -1);
  AudioClip * orig_clip = audio_region_get_clip (r);
  g_return_val_if_fail (orig_clip, -1);

  Position init_pos;
  position_init (&init_pos);
  if (
    position_is_before (
      &audio_sel->sel_start, &r->base.pos)
    || position_is_after (
      &audio_sel->sel_end, &r->base.end_pos))
    {
      position_print (&audio_sel->sel_start);
      position_print (&audio_sel->sel_end);
      g_set_error_literal (
        error, Z_AUDIO_AUDIO_FUNCTION_ERROR,
        Z_AUDIO_AUDIO_FUNCTION_ERROR_INVALID_POSITIONS,
        _ ("Invalid positions - skipping function"));
      return -1;
    }

  /* adjust the positions */
  Position start, end;
  position_set_to_pos (
    &start, &audio_sel->sel_start);
  position_set_to_pos (&end, &audio_sel->sel_end);
  position_add_frames (&start, -r->base.pos.frames);
  position_add_frames (&end, -r->base.pos.frames);

  /* create a copy of the frames to be replaced */
  unsigned_frame_t num_frames =
    (unsigned_frame_t) (end.frames - start.frames);

  /* interleaved frames */
  channels_t channels = orig_clip->channels;
  float      src_frames[num_frames * channels];
  float      frames[num_frames * channels];
  dsp_copy (
    &frames[0],
    &orig_clip
       ->frames[start.frames * (long) channels],
    num_frames * channels);
  dsp_copy (
    &src_frames[0], &frames[0],
    num_frames * channels);

  unsigned_frame_t nudge_frames = (unsigned_frame_t)
    position_get_frames_from_ticks (
      ARRANGER_SELECTIONS_DEFAULT_NUDGE_TICKS);
  unsigned_frame_t nudge_frames_all_channels =
    channels * nudge_frames;
  unsigned_frame_t num_frames_excl_nudge;

  g_debug (
    "num frames %" PRIu64
    ", "
    "nudge_frames %" PRIu64,
    num_frames, nudge_frames);
  z_return_val_if_fail_cmp (nudge_frames, >, 0, -1);

  switch (type)
    {
    case AUDIO_FUNCTION_INVERT:
      dsp_mul_k2 (
        &frames[0], -1.f, num_frames * channels);
      break;
    case AUDIO_FUNCTION_NORMALIZE_PEAK:
      /* peak-normalize */
      {
        float abs_peak = dsp_abs_max (
          &frames[0], num_frames * channels);
        dsp_mul_k2 (
          &frames[0], 1.f / abs_peak,
          num_frames * channels);
      }
      break;
    case AUDIO_FUNCTION_NORMALIZE_RMS:
      /* TODO rms-normalize */
      break;
    case AUDIO_FUNCTION_NORMALIZE_LUFS:
      /* TODO lufs-normalize */
      break;
    case AUDIO_FUNCTION_LINEAR_FADE_IN:
      dsp_linear_fade_in (
        &frames[0], num_frames * channels);
      break;
    case AUDIO_FUNCTION_LINEAR_FADE_OUT:
      dsp_linear_fade_out (
        &frames[0], num_frames * channels);
      break;
    case AUDIO_FUNCTION_NUDGE_LEFT:
      g_return_val_if_fail (
        num_frames > nudge_frames, -1);
      num_frames_excl_nudge =
        num_frames - (size_t) nudge_frames;
      dsp_copy (
        &frames[0],
        &src_frames[nudge_frames_all_channels],
        channels * num_frames_excl_nudge);
      dsp_fill (
        &frames[channels * num_frames_excl_nudge],
        0.f, nudge_frames_all_channels);
      break;
    case AUDIO_FUNCTION_NUDGE_RIGHT:
      g_return_val_if_fail (
        num_frames > nudge_frames, -1);
      num_frames_excl_nudge =
        num_frames - (size_t) nudge_frames;
      dsp_copy (
        &frames[nudge_frames], &src_frames[0],
        channels * num_frames_excl_nudge);
      dsp_fill (
        &frames[0], 0.f, nudge_frames_all_channels);
      break;
    case AUDIO_FUNCTION_REVERSE:
      for (size_t i = 0; i < num_frames; i++)
        {
          for (size_t j = 0; j < channels; j++)
            {
              frames[i * channels + j] =
                orig_clip->frames
                  [((size_t) start.frames
                    + ((num_frames - i) - 1))
                     * channels
                   + j];
            }
        }
      break;
    case AUDIO_FUNCTION_EXT_PROGRAM:
      {
        AudioClip * tmp_clip =
          audio_clip_new_from_float_array (
            src_frames, num_frames, channels,
            BIT_DEPTH_32, "tmp-clip");
        tmp_clip = audio_clip_edit_in_ext_program (
          tmp_clip);
        if (!tmp_clip)
          return -1;
        dsp_copy (
          &frames[0], &tmp_clip->frames[0],
          MIN (
            num_frames,
            (size_t) tmp_clip->num_frames)
            * channels);
        if ((size_t) tmp_clip->num_frames < num_frames)
          {
            dsp_fill (
              &frames[0], 0.f,
              (num_frames
               - (size_t) tmp_clip->num_frames)
                * channels);
          }
      }
      break;
    case AUDIO_FUNCTION_CUSTOM_PLUGIN:
      {
        g_return_val_if_fail (uri, -1);
        GError * err = NULL;
        int      ret = apply_plugin (
               uri, frames, num_frames, channels, &err);
        if (ret != 0)
          {
            PROPAGATE_PREFIXED_ERROR (
              error, err, "%s",
              _ ("Failed to apply plugin"));
            return ret;
          }
      }
      break;
    case AUDIO_FUNCTION_INVALID:
      /* do nothing */
      break;
    default:
      g_warning ("not implemented");
      break;
    }

#if 0
  char * tmp =
    g_strdup_printf (
      "%s - %s - %s",
      tr->name, r->name,
      audio_function_type_to_string (type));

  /* remove dots from name */
  char * name = string_replace (tmp, ".", "_");
  g_free (tmp);
#endif

  AudioClip * clip = audio_clip_new_from_float_array (
    &frames[0], num_frames, channels, BIT_DEPTH_32,
    orig_clip->name);
  audio_pool_add_clip (AUDIO_POOL, clip);
  g_message (
    "writing %s to pool (id %d)", clip->name,
    clip->pool_id);
  audio_clip_write_to_pool (
    clip, false, F_NOT_BACKUP);

  audio_sel->pool_id = clip->pool_id;

  if (type != AUDIO_FUNCTION_INVALID)
    {
      /* replace the frames in the region */
      audio_region_replace_frames (
        r, frames, (size_t) start.frames,
        num_frames, F_NO_DUPLICATE_CLIP);
    }

  if (
    !ZRYTHM_TESTING && type != AUDIO_FUNCTION_INVALID
    && type != AUDIO_FUNCTION_CUSTOM_PLUGIN)
    {
      /* set last action */
      g_settings_set_int (
        S_UI, "audio-function", type);
    }

  EVENTS_PUSH (ET_EDITOR_FUNCTION_APPLIED, NULL);

  return 0;
}
