/*
 * Copyright (C) 2021 Alexandros Theodotou <alex at zrythm dot org>
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
 * Modulator macro button processor.
 */

#ifndef __AUDIO_MODULATOR_MACRO_PROCESSOR_H__
#define __AUDIO_MODULATOR_MACRO_PROCESSOR_H__

#include "audio/port.h"

#include "utils/yaml.h"

typedef struct Track Track;

/**
 * @addtogroup audio
 *
 * @{
 */

/**
 * Modulator macro button processor.
 *
 * Has 1 control input, many CV inputs and 1 CV
 * output.
 *
 * Can only belong to modulator track.
 */
typedef struct ModulatorMacroProcessor
{
  /** CV input port for connecting CV signals to. */
  Port  *           cv_in;

  /**
   * CV output after macro is applied.
   *
   * This can be routed to other parameters to apply
   * the macro.
   */
  Port *            cv_out;

  /** Control port controling the amount. */
  Port *            macro;

} ModulatorMacroProcessor;

static const cyaml_schema_field_t
modulator_macro_processor_fields_schema[] =
{
  YAML_FIELD_MAPPING_PTR (
    ModulatorMacroProcessor, cv_in,
    port_fields_schema),
  YAML_FIELD_MAPPING_PTR (
    ModulatorMacroProcessor, cv_out,
    port_fields_schema),
  YAML_FIELD_MAPPING_PTR (
    ModulatorMacroProcessor, macro,
    port_fields_schema),

  CYAML_FIELD_END
};

static const cyaml_schema_value_t
modulator_macro_processor_schema = {
  YAML_VALUE_PTR (
    ModulatorMacroProcessor,
    modulator_macro_processor_fields_schema),
};

Track *
modulator_macro_processor_get_track (
  ModulatorMacroProcessor * self);

/**
 * Process.
 *
 * @param g_start_frames Global frames.
 * @param start_frame The local offset in this
 *   cycle.
 * @param nframes The number of frames to process.
 */
void
modulator_macro_processor_process (
  ModulatorMacroProcessor * self,
  long                      g_start_frames,
  nframes_t                 start_frame,
  const nframes_t           nframes);

ModulatorMacroProcessor *
modulator_macro_processor_new (
  Track * track,
  int     idx);

void
modulator_macro_processor_free (
  ModulatorMacroProcessor * self);

/**
 * @}
 */

#endif
