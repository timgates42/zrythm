// SPDX-FileCopyrightText: © 2020-2021 Alexandros Theodotou <alex@zrythm.org>
// SPDX-License-Identifier: LicenseRef-ZrythmLicense

#include "audio/fader.h"
#include "audio/graph.h"
#include "audio/graph_export.h"
#include "audio/graph_node.h"
#include "audio/port.h"
#include "audio/router.h"
#include "audio/track.h"
#include "plugins/plugin.h"
#include "project.h"
#include "utils/flags.h"
#include "utils/objects.h"

#ifdef HAVE_CGRAPH
#  include <graphviz/cgraph.h>
#  include <graphviz/gvc.h>
#endif

#ifdef HAVE_CGRAPH

typedef struct ANode
{
  GraphNode * node;

  /** Subgraph the node is a part of. */
  Agraph_t * graph;
} ANode;

static ANode *
anode_new (void)
{
  return object_new (ANode);
}

static void
anode_free (ANode * anode)
{
  object_zero_and_free (anode);
}

static Agraph_t *
get_graph_from_node (
  GHashTable * anodes,
  GraphNode *  node)
{
  ANode * anode =
    (ANode *) g_hash_table_lookup (anodes, node);
  if (anode && anode->node->id == node->id)
    return anode->graph;
  g_warning (
    "%p %s", node, graph_node_get_name (node));
  g_return_val_if_reached (NULL);
}

static Agraph_t *
get_parent_graph (
  GHashTable * anodes,
  GraphNode *  node)
{
  GraphNode * parent_node = NULL;
  switch (node->type)
    {
    case ROUTE_NODE_TYPE_TRACK:
    case ROUTE_NODE_TYPE_PLUGIN:
    case ROUTE_NODE_TYPE_FADER:
    case ROUTE_NODE_TYPE_PREFADER:
    case ROUTE_NODE_TYPE_MODULATOR_MACRO_PROCESOR:
    case ROUTE_NODE_TYPE_CHANNEL_SEND:
      parent_node = node;
      break;
    case ROUTE_NODE_TYPE_PORT:
      {
        switch (node->port->id.owner_type)
          {
          case PORT_OWNER_TYPE_PLUGIN:
            {
              Plugin * pl =
                port_get_plugin (node->port, true);
              parent_node =
                graph_find_node_from_plugin (
                  node->graph, pl);
            }
            break;
          case PORT_OWNER_TYPE_TRACK:
            {
              Track * tr =
                port_get_track (node->port, true);
              if (
                node->port->id.flags
                & PORT_FLAG_MODULATOR_MACRO)
                {
                  parent_node =
                    graph_find_node_from_modulator_macro_processor (
                      node->graph,
                      tr->modulator_macros
                        [node->port->id.port_index]);
                }
              else
                {
                  parent_node =
                    graph_find_node_from_track (
                      node->graph, tr, true);
                }
            }
            break;
          case PORT_OWNER_TYPE_CHANNEL_SEND:
            {
              Track * tr =
                port_get_track (node->port, true);
              g_return_val_if_fail (
                IS_TRACK_AND_NONNULL (tr), NULL);
              g_return_val_if_fail (
                tr->channel, NULL);
              ChannelSend * send =
                tr->channel->sends
                  [node->port->id.port_index];
              g_return_val_if_fail (send, NULL);
              parent_node =
                graph_find_node_from_channel_send (
                  node->graph, send);
            }
            break;
          case PORT_OWNER_TYPE_FADER:
            {
              if (
                node->port->id.flags2
                & PORT_FLAG2_MONITOR_FADER)
                {
                  parent_node =
                    graph_find_node_from_fader (
                      node->graph, MONITOR_FADER);
                }
              else if (
                node->port->id.flags2
                & PORT_FLAG2_SAMPLE_PROCESSOR_FADER)
                {
                  parent_node =
                    graph_find_node_from_fader (
                      node->graph,
                      SAMPLE_PROCESSOR->fader);
                }
              else
                {
                  Track * tr = port_get_track (
                    node->port, true);
                  if (
                    node->port->id.flags2
                    & PORT_FLAG2_PREFADER)
                    parent_node =
                      graph_find_node_from_prefader (
                        node->graph,
                        tr->channel->prefader);
                  else
                    parent_node =
                      graph_find_node_from_fader (
                        node->graph,
                        tr->channel->fader);
                }
            }
            break;
          case PORT_OWNER_TYPE_TRACK_PROCESSOR:
            {
              Track * tr =
                port_get_track (node->port, true);
              parent_node =
                graph_find_node_from_track (
                  node->graph, tr, true);
            }
            break;
          default:
            break;
          }
      }
      break;
    default:
      break;
    }
  if (!parent_node)
    return NULL;

  return get_graph_from_node (anodes, parent_node);
}

static Agnode_t *
create_anode (
  Agraph_t *   aroot_graph,
  GraphNode *  node,
  GHashTable * anodes)
{
  Agraph_t * aparent_graph =
    get_parent_graph (anodes, node);
  if (!aparent_graph)
    aparent_graph = aroot_graph;

  char * plain_node_name =
    graph_node_get_name (node);
  char * node_name = g_strdup_printf (
    "%s\np:%d (%d) c:%d", plain_node_name,
    node->playback_latency,
    node->route_playback_latency, 0);
  /*g_strdup_printf (*/
  /*"%s i:%d t:%d init refcount: %d",*/
  /*plain_node_name,*/
  /*node->initial, node->terminal,*/
  /*node->init_refcount);*/
  g_free (plain_node_name);
  Agnode_t * anode =
    agnode (aparent_graph, node_name, true);
  switch (node->type)
    {
    case ROUTE_NODE_TYPE_PORT:
      switch (node->port->id.type)
        {
        case TYPE_AUDIO:
          agsafeset (
            anode, (char *) "color",
            (char *) "crimson", (char *) "black");
          break;
        case TYPE_EVENT:
          agsafeset (
            anode, (char *) "color",
            (char *) "navy", (char *) "black");
          break;
        case TYPE_CONTROL:
          agsafeset (
            anode, (char *) "color",
            (char *) "darkviolet",
            (char *) "black");
          break;
        case TYPE_CV:
          agsafeset (
            anode, (char *) "color",
            (char *) "darkgreen", (char *) "black");
          break;
        default:
          break;
        }
      break;
    default:
      agsafeset (
        anode, (char *) "shape", (char *) "record",
        (char *) "ellipse");
      break;
    }
  g_free (node_name);

  return anode;
}

static void
fill_anodes (
  Graph *      graph,
  Agraph_t *   aroot_graph,
  GHashTable * anodes)
{
  char cluster_name[600];

  /* fill nodes */
  GHashTableIter iter;
  gpointer       key, value;
  g_hash_table_iter_init (
    &iter, graph->setup_graph_nodes);
  while (
    g_hash_table_iter_next (&iter, &key, &value))
    {
      GraphNode * node = (GraphNode *) value;
      ANode *     anode = anode_new ();
      anode->node = node;
      g_hash_table_insert (anodes, node, anode);
      get_graph_from_node (anodes, node);
    }

  /* create top clusters (tracks, sample processor,
   * monitor fader) */
  g_hash_table_iter_init (&iter, anodes);
  while (
    g_hash_table_iter_next (&iter, &key, &value))
    {
      GraphNode * node = (GraphNode *) key;
      ANode *     anode = (ANode *) value;

      if (
        node->type != ROUTE_NODE_TYPE_TRACK
        && node->type != ROUTE_NODE_TYPE_SAMPLE_PROCESSOR
        && node->type
             != ROUTE_NODE_TYPE_MONITOR_FADER)
        {
          continue;
        }

      char * node_name = graph_node_get_name (node);
      sprintf (
        cluster_name, "cluster_%s", node_name);
      anode->graph =
        agsubg (aroot_graph, cluster_name, true);
      agsafeset (
        anode->graph, (char *) "label", node_name,
        (char *) "");
    }

  /* create track subclusters */
  g_hash_table_iter_init (&iter, anodes);
  while (
    g_hash_table_iter_next (&iter, &key, &value))
    {
      GraphNode * node = (GraphNode *) key;
      ANode *     anode = (ANode *) value;

      if (
        node->type != ROUTE_NODE_TYPE_PLUGIN
        && node->type != ROUTE_NODE_TYPE_FADER
        && node->type != ROUTE_NODE_TYPE_PREFADER
        && node->type != ROUTE_NODE_TYPE_CHANNEL_SEND
        && node->type
             != ROUTE_NODE_TYPE_MODULATOR_MACRO_PROCESOR)
        continue;

      GraphNode * parent_node;
      switch (node->type)
        {
        case ROUTE_NODE_TYPE_PLUGIN:
          {
            Plugin * pl = node->pl;
            Track *  tr = plugin_get_track (pl);
            parent_node =
              graph_find_node_from_track (
                node->graph, tr, true);
          }
          break;
        case ROUTE_NODE_TYPE_FADER:
          {
            Fader * fader = node->fader;
            Track * tr = fader_get_track (fader);
            parent_node =
              graph_find_node_from_track (
                node->graph, tr, true);
          }
          break;
        case ROUTE_NODE_TYPE_PREFADER:
          {
            Fader * prefader = node->prefader;
            Track * tr = fader_get_track (prefader);
            parent_node =
              graph_find_node_from_track (
                node->graph, tr, true);
          }
          break;
        case ROUTE_NODE_TYPE_CHANNEL_SEND:
          {
            ChannelSend * send = node->send;
            g_return_if_fail (send);
            Track * tr =
              channel_send_get_track (send);
            g_return_if_fail (
              IS_TRACK_AND_NONNULL (tr));
            parent_node =
              graph_find_node_from_track (
                node->graph, tr, true);
          }
          break;
        case ROUTE_NODE_TYPE_MODULATOR_MACRO_PROCESOR:
          {
            ModulatorMacroProcessor * mmp =
              node->modulator_macro_processor;
            Track * tr =
              modulator_macro_processor_get_track (
                mmp);
            parent_node =
              graph_find_node_from_track (
                node->graph, tr, true);
          }
          break;
        default:
          continue;
        }

      Agraph_t * aparent_graph =
        get_parent_graph (anodes, parent_node);
      g_warn_if_fail (aparent_graph);
      char * node_name = graph_node_get_name (node);
      sprintf (
        cluster_name, "cluster_%s", node_name);
      anode->graph =
        agsubg (aparent_graph, cluster_name, true);
      agsafeset (
        anode->graph, (char *) "label", node_name,
        (char *) "");
    }
}

static void
export_as_graphviz_type (
  Graph *      graph,
  const char * export_path,
  const char * type)
{
  GVC_t *    gvc = gvContext ();
  Agraph_t * agraph = agopen (
    (char *) "routing_graph", Agstrictdirected,
    NULL);

  /* fill anodes with subgraphs */
  /* Hash table of
   * key: (GraphNode *), value: (ANode *) */
  GHashTable * anodes = g_hash_table_new_full (
    g_direct_hash, g_direct_equal, NULL,
    (GDestroyNotify) anode_free);
  fill_anodes (graph, agraph, anodes);

  /* create graph */
  GHashTableIter iter;
  gpointer       key, value;
  g_hash_table_iter_init (&iter, anodes);
  while (
    g_hash_table_iter_next (&iter, &key, &value))
    {
      GraphNode * node = (GraphNode *) key;

      Agnode_t * anode =
        create_anode (agraph, node, anodes);
      for (int j = 0; j < node->n_childnodes; j++)
        {
          GraphNode * child = node->childnodes[j];
          Agnode_t *  achildnode =
            create_anode (agraph, child, anodes);

          /* create edge */
          Agedge_t * edge = agedge (
            agraph, anode, achildnode, NULL, true);
          if (node->type == ROUTE_NODE_TYPE_PORT)
            {
              char * color =
                agget (anode, (char *) "color");
              agsafeset (
                edge, (char *) "color", color,
                (char *) "black");
            }
          else if (child->type == ROUTE_NODE_TYPE_PORT)
            {
              char * color = agget (
                achildnode, (char *) "color");
              agsafeset (
                edge, (char *) "color",
                (char *) color, (char *) "black");
            }
        }
    }

  gvLayout (gvc, agraph, "dot");
  gvRenderFilename (gvc, agraph, type, export_path);
  gvFreeLayout (gvc, agraph);
  agclose (agraph);
  gvFreeContext (gvc);
  object_free_w_func_and_null (
    g_hash_table_unref, anodes);
}
#endif

void
graph_export_as_simple (
  GraphExportType type,
  const char *    export_path)
{
  /* pause engine */
  EngineState state;
  engine_wait_for_pause (
    AUDIO_ENGINE, &state, Z_F_FORCE);

  Graph * graph = graph_new (ROUTER);
  graph_setup (graph, false, false);

  graph_export_as (graph, type, export_path);

  graph_free (graph);

  /* continue engine */
  engine_resume (AUDIO_ENGINE, &state);
}

/**
 * Exports the graph at the given path.
 *
 * Engine must be paused before calling this.
 */
void
graph_export_as (
  Graph *         graph,
  GraphExportType type,
  const char *    export_path)
{
  g_message (
    "exporting graph to %s...", export_path);

  switch (type)
    {
#ifdef HAVE_CGRAPH
    case GRAPH_EXPORT_PNG:
      export_as_graphviz_type (
        graph, export_path, "png");
      break;
    case GRAPH_EXPORT_DOT:
      export_as_graphviz_type (
        graph, export_path, "dot");
      break;
    case GRAPH_EXPORT_PS:
      export_as_graphviz_type (
        graph, export_path, "ps");
      break;
    case GRAPH_EXPORT_SVG:
      export_as_graphviz_type (
        graph, export_path, "svg");
      break;
#endif
    default:
      g_warn_if_reached ();
      break;
    }

  g_message ("graph exported");
}
