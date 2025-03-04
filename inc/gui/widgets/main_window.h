// SPDX-FileCopyrightText: © 2018-2022 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#ifndef __GUI_WIDGETS_MAIN_WINDOW_H__
#define __GUI_WIDGETS_MAIN_WINDOW_H__

#include "zrythm.h"

#include <adwaita.h>
#include <gtk/gtk.h>
#include <libpanel.h>

#define MAIN_WINDOW_WIDGET_TYPE \
  (main_window_widget_get_type ())
G_DECLARE_FINAL_TYPE (
  MainWindowWidget,
  main_window_widget,
  Z,
  MAIN_WINDOW_WIDGET,
  AdwApplicationWindow)

typedef struct _HeaderWidget     HeaderWidget;
typedef struct _CenterDockWidget CenterDockWidget;
typedef struct _BotBarWidget     BotBarWidget;
typedef struct _TopBarWidget     TopBarWidget;
typedef struct _ZrythmApp        ZrythmApp;
typedef struct ArrangerSelections ArrangerSelections;

/**
 * @addtogroup widgets
 *
 * @{
 */

#define MAIN_WINDOW zrythm_app->main_window
#define MW MAIN_WINDOW

/**
 * The main window of Zrythm.
 *
 * Inherits from GtkApplicationWindow, meaning that
 * it is the parent of all other sub-windows of
 * Zrythm.
 */
typedef struct _MainWindowWidget
{
  AdwApplicationWindow parent_instance;

  GtkHeaderBar *      header_bar;
  PanelDockSwitcher * start_dock_switcher;
  AdwWindowTitle *    window_title;
  PanelDockSwitcher * end_dock_switcher;

  AdwViewSwitcher * view_switcher;

  GtkBox *    header_start_box;
  GtkBox *    header_end_box;
  GtkButton * z_icon;
  GtkButton * preferences;
  GtkButton * log_viewer;
  GtkButton * scripting_interface;

  GtkBox *           main_box;
  HeaderWidget *     header;
  TopBarWidget *     top_bar;
  GtkBox *           center_box;
  CenterDockWidget * center_dock;
  BotBarWidget *     bot_bar;
  int                is_fullscreen;
  int                height;
  int                width;
  AdwToastOverlay *  toast_overlay;

  /** Whether preferences window is opened. */
  bool preferences_opened;

  /** Whether log has pending warnings (if true,
   * the log viewer button will have an emblem until
   * clicked). */
  bool log_has_pending_warnings;

  /** Whether set up already or not. */
  bool setup;
} MainWindowWidget;

/**
 * Creates a main_window widget using the given
 * app data.
 */
MainWindowWidget *
main_window_widget_new (ZrythmApp * app);

/**
 * Refreshes the state of the main window.
 */
void
main_window_widget_setup (MainWindowWidget * self);

void
main_window_widget_set_project_title (
  MainWindowWidget * self,
  Project *          prj);

/**
 * Prepare for finalization.
 */
void
main_window_widget_tear_down (
  MainWindowWidget * self);

/**
 * TODO
 */
void
main_window_widget_open (
  MainWindowWidget * win,
  GFile *            file);

void
main_window_widget_quit (MainWindowWidget * self);

/**
 * @}
 */

#endif
