// SPDX-FileCopyrightText: © 2019-2022 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "zrythm-config.h"

#include "gui/accel.h"
#include "gui/widgets/gtk_flipper.h"
#include "gui/widgets/header.h"
#include "gui/widgets/help_toolbar.h"
#include "gui/widgets/home_toolbar.h"
#include "gui/widgets/live_waveform.h"
#include "gui/widgets/midi_activity_bar.h"
#include "gui/widgets/project_toolbar.h"
#include "gui/widgets/snap_box.h"
#include "gui/widgets/snap_grid.h"
#include "gui/widgets/toolbox.h"
#include "gui/widgets/view_toolbar.h"
#include "utils/gtk.h"
#include "utils/resources.h"

#include <glib/gi18n.h>

G_DEFINE_TYPE (
  HeaderWidget,
  header_widget,
  GTK_TYPE_WIDGET)

void
header_widget_refresh (HeaderWidget * self)
{
  /* TODO move to main window */
#if 0
  g_debug (
    "refreshing header widget: "
    "has pending warnings: %d",
    self->log_has_pending_warnings);
  GtkButton * btn = self->log_viewer;
  z_gtk_button_set_emblem (
    btn,
    self->log_has_pending_warnings ?
      "media-record" : NULL);
#endif
}

void
header_widget_setup (
  HeaderWidget * self,
  const char *   title)
{
  home_toolbar_widget_setup (self->home_toolbar);
}

static void
dispose (HeaderWidget * self)
{
  /* unparenting the stack throws a CRITICAL on
   * close */
  /*gtk_widget_unparent (GTK_WIDGET (self->stack));*/

  gtk_widget_unparent (GTK_WIDGET (self->end_box));

  G_OBJECT_CLASS (header_widget_parent_class)
    ->dispose (G_OBJECT (self));
}

static void
header_widget_init (HeaderWidget * self)
{
  g_type_ensure (HOME_TOOLBAR_WIDGET_TYPE);
  g_type_ensure (TOOLBOX_WIDGET_TYPE);
  g_type_ensure (SNAP_BOX_WIDGET_TYPE);
  g_type_ensure (SNAP_GRID_WIDGET_TYPE);
  g_type_ensure (HELP_TOOLBAR_WIDGET_TYPE);
  g_type_ensure (VIEW_TOOLBAR_WIDGET_TYPE);
  g_type_ensure (PROJECT_TOOLBAR_WIDGET_TYPE);
  g_type_ensure (LIVE_WAVEFORM_WIDGET_TYPE);
  g_type_ensure (MIDI_ACTIVITY_BAR_WIDGET_TYPE);
  g_type_ensure (GTK_TYPE_FLIPPER);

  gtk_widget_init_template (GTK_WIDGET (self));

  GtkStyleContext * context;
  context = gtk_widget_get_style_context (
    GTK_WIDGET (self));
  gtk_style_context_add_class (context, "header");

  live_waveform_widget_setup_engine (
    self->live_waveform);
  midi_activity_bar_widget_setup_engine (
    self->midi_activity);
  midi_activity_bar_widget_set_animation (
    self->midi_activity, MAB_ANIMATION_FLASH);

  /* set tooltips */
  char about_tooltip[500];
  sprintf (
    about_tooltip, _ ("About %s"), PROGRAM_NAME);
}

static void
header_widget_class_init (
  HeaderWidgetClass * _klass)
{
  GtkWidgetClass * klass =
    GTK_WIDGET_CLASS (_klass);

  resources_set_class_template (klass, "header.ui");

#define BIND_CHILD(x) \
  gtk_widget_class_bind_template_child ( \
    klass, HeaderWidget, x)

  BIND_CHILD (stack);
  BIND_CHILD (end_box);
  BIND_CHILD (home_toolbar);
  BIND_CHILD (project_toolbar);
  BIND_CHILD (view_toolbar);
  BIND_CHILD (help_toolbar);
  BIND_CHILD (live_waveform);
  BIND_CHILD (midi_activity);

#undef BIND_CHILD

  gtk_widget_class_set_layout_manager_type (
    klass, GTK_TYPE_BOX_LAYOUT);

  GObjectClass * oklass = G_OBJECT_CLASS (_klass);
  oklass->dispose = (GObjectFinalizeFunc) dispose;
}
