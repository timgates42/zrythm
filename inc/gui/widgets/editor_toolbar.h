// SPDX-FileCopyrightText: © 2019-2022 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

/**
 * \file
 */

#ifndef __GUI_WIDGETS_EDITOR_TOOLBAR_H__
#define __GUI_WIDGETS_EDITOR_TOOLBAR_H__

#include <adwaita.h>
#include <gtk/gtk.h>

#define EDITOR_TOOLBAR_WIDGET_TYPE \
  (editor_toolbar_widget_get_type ())
G_DECLARE_FINAL_TYPE (
  EditorToolbarWidget,
  editor_toolbar_widget,
  Z,
  EDITOR_TOOLBAR_WIDGET,
  GtkWidget)

#define MW_EDITOR_TOOLBAR \
  MW_CLIP_EDITOR->editor_toolbar

TYPEDEF_STRUCT_UNDERSCORED (ToolboxWidget);
TYPEDEF_STRUCT_UNDERSCORED (QuantizeMbWidget);
TYPEDEF_STRUCT_UNDERSCORED (QuantizeBoxWidget);
TYPEDEF_STRUCT_UNDERSCORED (SnapBoxWidget);
TYPEDEF_STRUCT_UNDERSCORED (
  PlayheadScrollButtonsWidget);
TYPEDEF_STRUCT_UNDERSCORED (VelocitySettingsWidget);
TYPEDEF_STRUCT_UNDERSCORED (ZoomButtonsWidget);

/**
 * The PianoRoll toolbar in the top.
 */
typedef struct _EditorToolbarWidget
{
  GtkWidget parent_instance;

  GtkScrolledWindow * scroll;

  GtkComboBoxText *   chord_highlighting;
  SnapBoxWidget *     snap_box;
  QuantizeBoxWidget * quantize_box;
  GtkButton *         event_viewer_toggle;
  GtkStack *          functions_btn_stack;
  AdwSplitButton *    midi_functions_btn;
  AdwSplitButton *    audio_functions_btn;
  AdwSplitButton *    automation_functions_btn;

  GtkSeparator * sep_after_chord_highlight;
  GtkBox *       chord_highlight_box;

  VelocitySettingsWidget * velocity_settings;
  GtkSeparator * sep_after_velocity_settings;

  PlayheadScrollButtonsWidget * playhead_scroll;
  ZoomButtonsWidget *           zoom_buttons;

  GMenuModel * midi_functions_menu;
  GMenuModel * automation_functions_menu;
  GMenuModel * audio_functions_menu;
} EditorToolbarWidget;

/**
 * Refreshes relevant widgets.
 */
void
editor_toolbar_widget_refresh (
  EditorToolbarWidget * self);

void
editor_toolbar_widget_setup (
  EditorToolbarWidget * self);

#endif
