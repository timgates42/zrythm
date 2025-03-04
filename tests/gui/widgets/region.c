/*
 * Copyright (C) 2019 Alexandros Theodotou <alex at zrythm dot org>
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

#include "audio/region.h"
#include "gui/widgets/region.h"
#include "project.h"
#include "zrythm.h"

#include "tests/helpers/fishbowl.h"
#include "tests/helpers/fishbowl_window.h"
#include "tests/helpers/zrythm.h"

typedef struct
{
  Region * midi_region;
} RegionFixture;

GtkWidget *
fishbowl_creation_func ()
{
  POSITION_INIT_ON_STACK (start_pos);
  POSITION_INIT_ON_STACK (end_pos);
  position_set_to_bar (&end_pos, 4);
  MidiRegion * r =
    midi_region_new (&start_pos, &end_pos, 1);
  g_return_val_if_fail (r, NULL);

  region_gen_name (r, "Test Region", NULL, NULL);
  ArrangerObject * r_obj = (ArrangerObject *) r;
  arranger_object_gen_widget (r_obj);
  gtk_widget_set_size_request (
    GTK_WIDGET (r_obj->widget), 200, 20);

  return GTK_WIDGET (r_obj->widget);
}

static void
test_midi_region_fishbowl ()
{
  guint region_count = fishbowl_window_widget_run (
    fishbowl_creation_func, DEFAULT_FISHBOWL_TIME);
}

int
main (int argc, char * argv[])
{
  g_test_init (&argc, &argv, NULL);

  test_helper_zrythm_init ();
  test_helper_zrythm_gui_init (argc, argv);

#define TEST_PREFIX "/gui/widgets/midi_region/"

  g_test_add_func (
    TEST_PREFIX "test midi region fishbowl",
    (GTestFunc) test_midi_region_fishbowl);

  return g_test_run ();
}
