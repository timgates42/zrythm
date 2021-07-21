/*
 * Copyright (C) 2018-2021 Alexandros Theodotou <alex at zrythm dot org>
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
 * Application settings.
 */

#include "zrythm-config.h"

#include <stdio.h>
#include <stdlib.h>

#include "settings/plugin_settings.h"
#include "settings/settings.h"
#include "settings/user_shortcuts.h"
#include "utils/mem.h"
#include "utils/objects.h"
#include "utils/string.h"
#include "utils/terminal.h"
#include "zrythm.h"

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#define G_SETTINGS_ENABLE_BACKEND
#include <gio/gsettingsbackend.h>

/**
 * Initializes settings.
 */
Settings *
settings_new (void)
{
  Settings * self = object_new (Settings);

  if (ZRYTHM_TESTING)
    {
      return self;
    }

  self->general =
    g_settings_new (
      GSETTINGS_ZRYTHM_PREFIX ".general");
  self->export =
    g_settings_new (
      GSETTINGS_ZRYTHM_PREFIX ".export");
  self->monitor =
    g_settings_new (
      GSETTINGS_ZRYTHM_PREFIX ".monitor");
  self->ui =
    g_settings_new (
      GSETTINGS_ZRYTHM_PREFIX ".ui");
  self->ui_inspector =
    g_settings_new (
      GSETTINGS_ZRYTHM_PREFIX ".ui.inspector");
  self->ui_mixer =
    g_settings_new (
      GSETTINGS_ZRYTHM_PREFIX ".ui.mixer");
  self->ui_plugin_browser =
    g_settings_new (
      GSETTINGS_ZRYTHM_PREFIX ".ui.plugin-browser");
  self->ui_panels =
    g_settings_new (
      GSETTINGS_ZRYTHM_PREFIX ".ui.panels");
  self->ui_file_browser =
    g_settings_new (
      GSETTINGS_ZRYTHM_PREFIX ".ui.file-browser");
  self->transport =
    g_settings_new (
      GSETTINGS_ZRYTHM_PREFIX ".transport");

#define PREFERENCES_PREFIX \
  GSETTINGS_ZRYTHM_PREFIX ".preferences."
#define NEW_PREFERENCES_SETTINGS(a,b) \
  self->preferences_##a##_##b = \
    g_settings_new ( \
      PREFERENCES_PREFIX #a "." #b); \
  g_return_val_if_fail ( \
    self->preferences_##a##_##b, NULL)

  NEW_PREFERENCES_SETTINGS (dsp, pan);
  NEW_PREFERENCES_SETTINGS (editing, audio);
  NEW_PREFERENCES_SETTINGS (editing, automation);
  NEW_PREFERENCES_SETTINGS (editing, undo);
  NEW_PREFERENCES_SETTINGS (general, engine);
  NEW_PREFERENCES_SETTINGS (general, paths);
  NEW_PREFERENCES_SETTINGS (plugins, uis);
  NEW_PREFERENCES_SETTINGS (plugins, paths);
  NEW_PREFERENCES_SETTINGS (projects, general);
  NEW_PREFERENCES_SETTINGS (ui, general);
  NEW_PREFERENCES_SETTINGS (scripting, general);

#undef PREFERENCES_PREFIX

  g_return_val_if_fail (
    self->general &&
    self->export && self->ui &&
    self->ui_inspector, NULL);

#if 0
  /* plugin settings */
  GSettingsBackend * psettings_backend =
    g_keyfile_settings_backend_new (
      "/filesystem/path/plugin-settings.ini",
      "/",
      NULL);
  self->plugin_settings =
    g_settings_new_with_backend (
      GSETTINGS_ZRYTHM_PREFIX ".plugin-settings",
      psettings_backend);
  g_settings_set_boolean (
    self->plugin_settings, "loop", 1);
#endif

  self->plugin_settings = plugin_settings_new ();
  g_return_val_if_fail (self->plugin_settings, NULL);

  self->user_shortcuts = user_shortcuts_new ();
  if (!self->user_shortcuts)
    {
      g_warning ("failed to parse user shortcuts");
    }

  return self;
}

static int
print_or_reset_schema (
  GSettingsSchema * schema,
  const char *      schema_str,
  int               print_only,
  int               pretty_print)
{
  GSettings * settings =
    g_settings_new (schema_str);
  char ** keys =
    g_settings_schema_list_keys (schema);

  char tmp[8000];
  if (print_only)
    {
      sprintf (
        tmp, _("Schema %s%s%s"),
        pretty_print ? TERMINAL_BOLD : "",
        schema_str,
        pretty_print ? TERMINAL_RESET : "");
      printf (
        "%s%s\n",
        pretty_print ? TERMINAL_UNDERLINE : "", tmp);
    }
  else
    {
      printf (
        _("Resetting schema %s...\n"), schema_str);
    }

  int i = 0;
  char * key;
  while ((key = keys[i++]))
    {
      GSettingsSchemaKey * schema_key =
        g_settings_schema_get_key (schema, key);
      g_return_val_if_fail (schema_key, -1);
      if (print_only)
        {
          GVariant * val =
            g_settings_get_value (settings, key);
          char * var_str = g_variant_print (val, 0);
          printf (
            "  %s%s%s=%s%s%s\n",
            pretty_print ?
              TERMINAL_COLOR_MAGENTA
                TERMINAL_BOLD : "",
            key,
            pretty_print ? TERMINAL_RESET : "",
            pretty_print ?
              TERMINAL_COLOR_YELLOW : "",
            var_str,
            pretty_print ? TERMINAL_RESET : "");
          if (pretty_print)
            {
              const char * description =
                g_settings_schema_key_get_description (
                  schema_key);
              printf (
                "    %s\n", _(description));
            }
        }
      else if (
        /* don't reset install-dir */
        string_is_equal (key, "install-dir"))
        {
          sprintf (tmp, _("skipping %s"), key);
          printf ("  %s\n", tmp);
        }
      else
        {
          GVariant * val =
            g_settings_schema_key_get_default_value (
              schema_key);
          g_return_val_if_fail (val, -1);
          char * var_str = g_variant_print (val, 0);
          sprintf (
            tmp, _("resetting %s to %s"),
            key, var_str);
          printf ("  %s\n", tmp);
          int ret =
            g_settings_set_value (settings, key, val);
          if (!ret)
            {
              g_warning ("Failed to set value");
              return -1;
            }
        }
    }

  return 0;
}

/**
 * @param print_only Only print the settings without
 *   resetting.
 */
static int
print_or_reset (
  int print_only,
  int pretty_print,
  int exit_on_finish)
{
  GSettingsSchemaSource * source =
    g_settings_schema_source_get_default ();
  char ** non_relocatable;
  g_settings_schema_source_list_schemas (
    source, 1, &non_relocatable, NULL);
  int i = 0;
  char * schema_str;
  while ((schema_str = non_relocatable[i++]))
    {
      if (string_contains_substr (
            schema_str, GSETTINGS_ZRYTHM_PREFIX))
        {
          GSettingsSchema * schema =
            g_settings_schema_source_lookup (
              source, schema_str, 1);
          if (!schema)
            {
              fprintf (
                stderr,
                _("Failed to find schema %s. %s is likely not installed"),
                schema_str, PROGRAM_NAME);
              if (exit_on_finish)
                exit (1);
              else
                return -1;
            }
          if (print_or_reset_schema (
                schema, schema_str, print_only,
                pretty_print))
            {
              if (print_only)
                {
                  fprintf (
                    stderr,
                    _("Failed to view schema %s"),
                    schema_str);
                }
              else
                {
                  fprintf (
                    stderr,
                    _("Failed to reset schema %s"),
                    schema_str);
                }
              if (exit_on_finish)
                exit (1);
              else
                return -1;
            }
        }
    }

  if (!print_only)
    {
      g_settings_sync ();
    }

  return 0;
}

/**
 * Resets settings to defaults.
 *
 * @param exit_on_finish Exit with a code on
 *   finish.
 */
void
settings_reset_to_factory (
  int confirm,
  int exit_on_finish)
{
  if (confirm)
    {
      printf (
        _("This will reset %s to factory settings. "
        "You will lose all your preferences. Type 'y' "
        "to continue: "),
        PROGRAM_NAME);
      char c = getchar ();
      if (c != 'y')
        {
          printf (_("Aborting...\n"));
          if (exit_on_finish)
            exit (0);

          return;
        }
    }

  print_or_reset (0, 0, exit_on_finish);

  printf (
    _("Reset to factory settings successful\n"));
}

/**
 * Prints the current settings.
 */
void
settings_print (
  int pretty_print)
{
  print_or_reset (1, pretty_print, 0);
}

GVariant *
settings_get_range (
  const char * schema,
  const char * key)
{
  GSettingsSchemaSource * source =
    g_settings_schema_source_get_default ();
  GSettingsSchema * g_settings_schema =
    g_settings_schema_source_lookup (
      source, schema, 1);
  GSettingsSchemaKey * schema_key =
    g_settings_schema_get_key (
      g_settings_schema, key);
  GVariant * range =
    g_settings_schema_key_get_range (
      schema_key);

  g_settings_schema_key_unref (schema_key);
  g_settings_schema_unref (g_settings_schema);
  g_settings_schema_source_unref (source);

  return range;
}

GVariant *
settings_get_default_value (
  const char * schema,
  const char * key)
{
  GSettingsSchemaSource * source =
    g_settings_schema_source_get_default ();
  GSettingsSchema * g_settings_schema =
    g_settings_schema_source_lookup (
      source, schema, 1);
  GSettingsSchemaKey * schema_key =
    g_settings_schema_get_key (
      g_settings_schema, key);
  GVariant * def_val =
    g_settings_schema_key_get_default_value (
      schema_key);

  g_settings_schema_key_unref (schema_key);
  g_settings_schema_unref (g_settings_schema);
  g_settings_schema_source_unref (source);

  return def_val;
}

double
settings_get_default_value_double (
  const char * schema,
  const char * key)
{
  GVariant * def_val =
    settings_get_default_value (schema, key);

  double ret = g_variant_get_double (def_val);

  g_variant_unref (def_val);

  return ret;
}

void
settings_get_range_double (
  const char * schema,
  const char * key,
  double *     lower,
  double *     upper)
{
  GVariant * range =
    settings_get_range (schema, key);

  GVariant * range_vals =
    g_variant_get_child_value (range, 1);
  range_vals =
    g_variant_get_child_value (range_vals, 0);
  GVariant * lower_var =
    g_variant_get_child_value (range_vals, 0);
  GVariant * upper_var =
    g_variant_get_child_value (range_vals, 1);

  *lower = g_variant_get_double (lower_var);
  *upper = g_variant_get_double (upper_var);

  g_variant_unref (range_vals);
  g_variant_unref (lower_var);
  g_variant_unref (upper_var);
}

/**
 * Returns whether the "as" key contains the given
 * string.
 */
bool
settings_strv_contains_str (
  GSettings *  settings,
  const char * key,
  const char * val)
{
  char ** strv =
    g_settings_get_strv (settings, key);

  for (int i = 0;; i++)
    {
      char * str = strv[i];
      if (!str)
        break;

      if (string_is_equal (str, val))
        {
          g_strfreev (strv);
          return true;
        }
    }

  g_strfreev (strv);

  return false;
}

/**
 * Appends the given string to a key of type "as".
 */
void
settings_append_to_strv (
  GSettings * settings,
  const char * key,
  const char * val,
  bool         ignore_if_duplicate)
{
  if (ignore_if_duplicate &&
      settings_strv_contains_str (
        settings, key, val))
    {
      return;
    }

  char ** strv =
    g_settings_get_strv (settings, key);

  size_t i;
  for (i = 0;; i++)
    {
      if (!strv[i])
        break;
    }

  strv =
    object_realloc_n (strv, i + 1, i + 2, char *);

  strv[i] = g_strdup (val);

  g_settings_set_strv (
    settings, key, (const char * const *) strv);

  g_strfreev (strv);
}

/**
 * Frees settings.
 */
void
settings_free (
  Settings * self)
{
  g_object_unref_and_null (self->general);
  g_object_unref_and_null (
    self->preferences_dsp_pan);
  g_object_unref_and_null (
    self->preferences_editing_audio);
  g_object_unref_and_null (
    self->preferences_editing_automation);
  g_object_unref_and_null (
    self->preferences_editing_undo);
  g_object_unref_and_null (self->export);
  g_object_unref_and_null (self->ui);
  g_object_unref_and_null (self->ui_inspector);
  g_object_unref_and_null (self->ui_mixer);
  g_object_unref_and_null (self->ui_panels);
  g_object_unref_and_null (self->ui_plugin_browser);
  g_object_unref_and_null (self->ui_file_browser);
  g_object_unref_and_null (self->monitor);

  /* TODO more */

  object_zero_and_free (self);
}
