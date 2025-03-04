/*
  Copyright 2007-2017 David Robillard <http://drobilla.net>

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

#include "zrythm-config.h"

#ifndef HAVE_SUIL

#  include <gtk/gtk.h>

#  include <suil/suil.h>

unsigned
suil_ui_supported (
  const char * host_type_uri,
  const char * ui_type_uri)
{
  enum
  {
    SUIL_WRAPPING_UNSUPPORTED = 0,
    SUIL_WRAPPING_NATIVE = 1,
    SUIL_WRAPPING_EMBEDDED = 2
  };

  /* not supported */
  /*return SUIL_WRAPPING_UNSUPPORTED;*/

  if (!strcmp (GTK4_UI_URI, ui_type_uri))
    return SUIL_WRAPPING_NATIVE;
#  ifdef _WOE32
  else if (!strcmp (ui_type_uri, WIN_UI_URI))
    return SUIL_WRAPPING_EMBEDDED;
#  endif
#  ifdef HAVE_X11
  else if (!strcmp (ui_type_uri, X11_UI_URI))
    return SUIL_WRAPPING_EMBEDDED;
#  endif
  else
    return SUIL_WRAPPING_UNSUPPORTED;
}

static SuilWrapper *
open_wrapper (
  SuilHost *      host,
  const char *    container_type_uri,
  const char *    ui_type_uri,
  LV2_Feature *** features,
  unsigned        n_features)
{
  SuilWrapper * wrapper = NULL;

#  if 0
#    ifdef HAVE_X11
	if (!strcmp(ui_type_uri, X11_UI_URI))
    {
      wrapper =
        suil_wrapper_new_x11 (
          host, container_type_uri,
          ui_type_uri, features, n_features);
    }
#    endif

#    ifdef _WOE32
  if (!strcmp(ui_type_uri, WIN_UI_URI))
    {
      wrapper =
        suil_wrapper_new_woe (
          host, container_type_uri,
          ui_type_uri, features, n_features);
    }
#    endif

#    ifdef __APPLE__
#      if 0
  if (!strcmp(ui_type_uri, COCOA_UI_URI))
    {
      wrapper =
        suil_wrapper_new_cocoa (
          host, container_type_uri,
          ui_type_uri, features, n_features);
    }
#      endif
#    endif
#  endif

  if (!wrapper)
    {
      SUIL_ERRORF (
        "Unable to wrap UI type <%s> as type <%s>\n",
        ui_type_uri, container_type_uri);
      return NULL;
    }

  return wrapper;
}

SuilInstance *
suil_instance_new (
  SuilHost *                  host,
  SuilController              controller,
  const char *                container_type_uri,
  const char *                plugin_uri,
  const char *                ui_uri,
  const char *                ui_type_uri,
  const char *                ui_bundle_path,
  const char *                ui_binary_path,
  const LV2_Feature * const * features)
{
  // Open UI library
  dlerror ();
  void * lib = dlopen (ui_binary_path, RTLD_NOW);
  if (!lib)
    {
      SUIL_ERRORF (
        "Unable to open UI library %s (%s)\n",
        ui_binary_path, dlerror ());
      return NULL;
    }

  // Get discovery function
  LV2UI_DescriptorFunction df =
    (LV2UI_DescriptorFunction) suil_dlfunc (
      lib, "lv2ui_descriptor");
  if (!df)
    {
      SUIL_ERRORF (
        "Broken LV2 UI %s (no lv2ui_descriptor symbol found)\n",
        ui_binary_path);
      dlclose (lib);
      return NULL;
    }

  // Get UI descriptor
  const LV2UI_Descriptor * descriptor = NULL;
  for (uint32_t i = 0; true; ++i)
    {
      const LV2UI_Descriptor * ld = df (i);
      if (!ld)
        {
          break;
        }
      else if (!strcmp (ld->URI, ui_uri))
        {
          descriptor = ld;
          break;
        }
    }
  if (!descriptor)
    {
      SUIL_ERRORF (
        "Failed to find descriptor for <%s> in %s\n",
        ui_uri, ui_binary_path);
      dlclose (lib);
      return NULL;
    }

  // Create SuilInstance
  SuilInstance * instance = (SuilInstance *)
    calloc (1, sizeof (SuilInstance));
  if (!instance)
    {
      SUIL_ERRORF (
        "Failed to allocate memory for <%s> instance\n",
        ui_uri);
      dlclose (lib);
      return NULL;
    }

  instance->lib_handle = lib;
  instance->descriptor = descriptor;

  // Make UI features array
  instance->features = (LV2_Feature **) malloc (
    sizeof (LV2_Feature *));
  instance->features[0] = NULL;

  // Copy user provided features
  const LV2_Feature * const * fi = features;
  unsigned                    n_features = 0;
  while (fi && *fi)
    {
      const LV2_Feature * f = *fi++;
      suil_add_feature (
        &instance->features, &n_features, f->URI,
        f->data);
    }

  // Add additional features implemented by SuilHost functions
  if (host->index_func)
    {
      instance->port_map.handle = controller;
      instance->port_map.port_index =
        host->index_func;
      suil_add_feature (
        &instance->features, &n_features,
        LV2_UI__portMap, &instance->port_map);
    }
  if (host->subscribe_func && host->unsubscribe_func)
    {
      instance->port_subscribe.handle = controller;
      instance->port_subscribe.subscribe =
        host->subscribe_func;
      instance->port_subscribe.unsubscribe =
        host->unsubscribe_func;
      suil_add_feature (
        &instance->features, &n_features,
        LV2_UI__portSubscribe,
        &instance->port_subscribe);
    }
  if (host->touch_func)
    {
      instance->touch.handle = controller;
      instance->touch.touch = host->touch_func;
      suil_add_feature (
        &instance->features, &n_features,
        LV2_UI__touch, &instance->touch);
    }

  // Open wrapper (this may add additional features)
  if (
    container_type_uri
    && strcmp (container_type_uri, ui_type_uri))
    {
      instance->wrapper = open_wrapper (
        host, container_type_uri, ui_type_uri,
        &instance->features, n_features);
      if (!instance->wrapper)
        {
          suil_instance_free (instance);
          return NULL;
        }
    }

  // Instantiate UI
  instance->handle = descriptor->instantiate (
    descriptor, plugin_uri, ui_bundle_path,
    host->write_func, controller,
    &instance->ui_widget,
    (const LV2_Feature * const *)
      instance->features);

  // Failed to instantiate UI
  if (!instance->handle)
    {
      SUIL_ERRORF (
        "Failed to instantiate UI <%s> in %s\n",
        ui_uri, ui_binary_path);
      suil_instance_free (instance);
      return NULL;
    }

  if (instance->wrapper)
    {
      if (instance->wrapper->wrap (
            instance->wrapper, instance))
        {
          SUIL_ERRORF (
            "Failed to wrap UI <%s> in type <%s>\n",
            ui_uri, container_type_uri);
          suil_instance_free (instance);
          return NULL;
        }
    }
  else
    {
      instance->host_widget = instance->ui_widget;
    }

  return instance;
}

void
suil_instance_free (SuilInstance * instance)
{
  g_message ("freeing suil instance %p", instance);
  if (instance)
    {
      for (unsigned i = 0; instance->features[i];
           ++i)
        {
          free (instance->features[i]);
        }
      free (instance->features);

      // Call wrapper free function to destroy widgets and drop references
      if (instance->wrapper && instance->wrapper->free)
        {
          g_message ("freeing wrapper");
          instance->wrapper->free (
            instance->wrapper);
        }

      // Call cleanup to destroy UI (if it still exists at this point)
      if (instance->handle)
        {
          g_message ("cleaning up UI");
          instance->descriptor->cleanup (
            instance->handle);
        }

      dlclose (instance->lib_handle);

      // Close libraries and free everything
      if (instance->wrapper)
        {
#  ifndef _WOE32
          // Never unload modules on windows, causes mysterious segfaults
          /*dlclose(instance->wrapper->lib);*/
#  endif
          free (instance->wrapper);
        }
      free (instance);
    }
}

SuilHandle
suil_instance_get_handle (SuilInstance * instance)
{
  return instance->handle;
}

LV2UI_Widget
suil_instance_get_widget (SuilInstance * instance)
{
  return instance->host_widget;
}

void
suil_instance_port_event (
  SuilInstance * instance,
  uint32_t       port_index,
  uint32_t       buffer_size,
  uint32_t       format,
  const void *   buffer)
{
  if (instance->descriptor->port_event)
    {
      instance->descriptor->port_event (
        instance->handle, port_index, buffer_size,
        format, buffer);
    }
}

const void *
suil_instance_extension_data (
  SuilInstance * instance,
  const char *   uri)
{
  if (instance->descriptor->extension_data)
    {
      return instance->descriptor->extension_data (
        uri);
    }
  return NULL;
}

#endif /* ifndef HAVE_SUIL */
