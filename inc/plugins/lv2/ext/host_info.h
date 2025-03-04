/*
  Copyright 2021 Alexandros Theodotou <alex at zrythm dot org>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

/* TODO move custom extensions to separate repo. */

/**
 * @file
 *
 * LV2 extension to allow plugins to receive
 * information about the host.
 *
 * Hosts supporting this extension should pass an
 * LV2_Options_Option for each piece of information
 * when instantiating a plugin. For example:
 *
 * context: LV2_OPTIONS_INSTANCE
 * subject: 0
 * key: Z_LV2_HOST_INFO__name
 * size: sizeof (const char *)
 * type: <URID of LV2_ATOM__String>
 * value: Pointer to string
 *
 * @note This feature MUST ONLY be used by plugins to
 * display the host's information somewhere. Plugins
 * MUST NOT behave differently in different hosts.
 */

#ifndef Z_LV2_HOST_INFO_H
#define Z_LV2_HOST_INFO_H

/**
 * @addtogroup lv2_ext
 *
 * @{
 */

#define Z_LV2_EXT_URI \
  "https://lv2.zrythm.org/ns/ext"
#define Z_LV2_HOST_INFO_URI \
  Z_LV2_EXT_URI "/host-info"
#define Z_LV2_HOST_INFO_PREFIX \
  Z_LV2_HOST_INFO_URI "#"
#define Z_LV2_HOST_INFO__name \
  Z_LV2_HOST_INFO_PREFIX "name"
#define Z_LV2_HOST_INFO__version \
  Z_LV2_HOST_INFO_PREFIX "version"

/**
 * @}
 */

#endif /* Z_LV2_HOST_INFO_H */
