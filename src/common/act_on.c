/*
    This file is part of darktable,
    Copyright (C) 2021 darktable developers.

    darktable is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    darktable is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with darktable.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "common/act_on.h"
#include "common/collection.h"
#include "common/darktable.h"
#include "common/debug.h"
#include "common/image.h"
#include "common/image_cache.h"
#include "common/selection.h"
#include "control/control.h"
#include "dtgtk/thumbtable.h"
#include <glib.h>
#include <sqlite3.h>


static int _find_custom(gconstpointer a, gconstpointer b)
{
  return (GPOINTER_TO_INT(a) != GPOINTER_TO_INT(b));
}
static void _insert_in_list(GList **list, const int imgid, gboolean only_visible)
{
  if(only_visible)
  {
    if(!g_list_find_custom(*list, GINT_TO_POINTER(imgid), _find_custom))
      *list = g_list_append(*list, GINT_TO_POINTER(imgid));
    return;
  }

  const dt_image_t *image = dt_image_cache_get(darktable.image_cache, imgid, 'r');
  if(image)
  {
    const int img_group_id = image->group_id;
    dt_image_cache_read_release(darktable.image_cache, image);

    if(!darktable.gui || !darktable.gui->grouping || darktable.gui->expanded_group_id == img_group_id
       || !dt_selection_get_collection(darktable.selection))
    {
      if(!g_list_find_custom(*list, GINT_TO_POINTER(imgid), _find_custom))
        *list = g_list_append(*list, GINT_TO_POINTER(imgid));
    }
    else
    {
      sqlite3_stmt *stmt;
      // clang-format off
      gchar *query = g_strdup_printf(
          "SELECT id"
          "  FROM main.images"
          "  WHERE group_id = %d AND id IN (%s)",
          img_group_id, dt_collection_get_query_no_group(dt_selection_get_collection(darktable.selection)));
      // clang-format on
      DT_DEBUG_SQLITE3_PREPARE_V2(dt_database_get(darktable.db), query, -1, &stmt, NULL);
      while(sqlite3_step(stmt) == SQLITE_ROW)
      {
        const int imgidg = sqlite3_column_int(stmt, 0);
        if(!g_list_find_custom(*list, GINT_TO_POINTER(imgidg), _find_custom))
          *list = g_list_append(*list, GINT_TO_POINTER(imgidg));
      }
      sqlite3_finalize(stmt);
      g_free(query);
    }
  }
}

// test if the cache is still valid
static gboolean _test_cache(dt_act_on_cache_t *cache)
{
  const int mouseover = dt_control_get_mouse_over_id();

  if(cache->ok && cache->image_over == mouseover
     && cache->inside_table == dt_ui_thumbtable(darktable.gui->ui)->mouse_inside
     && g_slist_length(cache->active_imgs) == g_slist_length(darktable.view_manager->active_images))
  {
    // we test active images if mouse outside table
    gboolean ok = TRUE;
    if(!dt_ui_thumbtable(darktable.gui->ui)->mouse_inside && cache->active_imgs)
    {
      GSList *l1 = cache->active_imgs;
      GSList *l2 = darktable.view_manager->active_images;
      while(l1 && l2)
      {
        if(GPOINTER_TO_INT(l1->data) != GPOINTER_TO_INT(l2->data))
        {
          ok = FALSE;
          break;
        }
        l2 = g_slist_next(l2);
        l1 = g_slist_next(l1);
      }
    }
    if(ok) return TRUE;
  }
  return FALSE;
}

// cache the list of images to act on during global changes (libs, accels)
// return TRUE if the cache is updated, FALSE if it's still up to date
gboolean _cache_update(const gboolean only_visible, const gboolean force, const gboolean ordered)
{
  /** Here's how it works
   *  if only_visible is FALSE, then it will add also not visible images because of grouping
   *  force define if we try to use cache or force a refresh
   *  if ordered is TRUE, we return the list in the gui order. Otherwise the order is undefined (but quicker)
   **/

  dt_act_on_cache_t *cache;
  if(only_visible)
    cache = &darktable.view_manager->act_on_cache_visible;
  else
    cache = &darktable.view_manager->act_on_cache_all;

  // Return the cached list if we don't force-refresh
  if(!force && cache->ordered == ordered && _test_cache(cache))
  {
    return FALSE;
  }

  // Selection are images having been toggled explicitly
  GList *l = dt_selection_get_list(darktable.selection, only_visible, ordered);

  // Active images are the single image being processed in darkroom
  // or the images being culled in culling view. We don't always have them.
  // Treat them as an higher level of selection.
  if(darktable.view_manager->active_images)
  {
    for(GSList *ll = darktable.view_manager->active_images; ll; ll = g_slist_next(ll))
    {
      const int id = GPOINTER_TO_INT(ll->data);
      _insert_in_list(&l, id, only_visible);
      // be absolutely sure we have the id in the list (in darkroom,
      // the active image can be out of collection)
      if(!only_visible) _insert_in_list(&l, id, TRUE);
    }
  }

  // let's register the new list as cached
  cache->ordered = ordered;
  cache->image_over = dt_control_get_mouse_over_id();
  GList *ltmp = cache->images;
  cache->images = l;
  g_list_free(ltmp);
  cache->images_nb = g_list_length(cache->images);
  GSList *sl = cache->active_imgs;
  cache->active_imgs = g_slist_copy(darktable.view_manager->active_images);
  g_slist_free(sl);
  cache->inside_table = dt_ui_thumbtable(darktable.gui->ui)->mouse_inside;
  cache->ok = TRUE;

  // if needed, we show the list of cached images in terminal
  if((darktable.unmuted & DT_DEBUG_ACT_ON) == DT_DEBUG_ACT_ON)
  {
    gchar *tx = dt_util_dstrcat(NULL, "[images to act on] new cache (%s) : ", only_visible ? "visible" : "all");
    for(GList *ll = l; ll; ll = g_list_next(ll)) tx = dt_util_dstrcat(tx, "%d ", GPOINTER_TO_INT(ll->data));
    dt_print(DT_DEBUG_ACT_ON, "%s\n", tx);
    g_free(tx);
  }

  return TRUE;
}

// get the list of images to act on during global changes (libs, accels)
GList *dt_act_on_get_images(const gboolean only_visible, const gboolean force, const gboolean ordered)
{
  // we first update the cache if needed
  _cache_update(only_visible, force, ordered);

  GList *l = NULL;
  if(only_visible && darktable.view_manager->act_on_cache_visible.ok)
    l = g_list_copy((GList *)darktable.view_manager->act_on_cache_visible.images);
  else if(!only_visible && darktable.view_manager->act_on_cache_all.ok)
    l = g_list_copy((GList *)darktable.view_manager->act_on_cache_all.images);

  // and we return a copy of the cached list
  return l;
}

// get the query to retrieve images to act on. this is useful to speedup actions if they already use sqlite queries
gchar *dt_act_on_get_query(const gboolean only_visible)
{
  return dt_selection_get_list_query(darktable.selection, only_visible, FALSE);
}

// get the main image to act on during global changes (libs, accels)
int dt_act_on_get_main_image()
{
  /** Here's how it works -- same as for list, except we don't care about mouse inside selection or table
   *
   *             mouse over| x |   |   |
   *          active images| ? |   | x |
   *                       |   |   |   |
   *                       | O | S | A |
   *  First image of ...
   *  S = selection ; O = mouseover ; A = active images
   **/

  int ret = -1;
  const int mouseover = dt_control_get_mouse_over_id();

  if(mouseover > 0)
  {
    ret = mouseover;
  }
  else
  {
    if(darktable.view_manager->active_images)
    {
      ret = GPOINTER_TO_INT(darktable.view_manager->active_images->data);
    }
    else
    {
      sqlite3_stmt *stmt;
      // clang-format off
      DT_DEBUG_SQLITE3_PREPARE_V2(dt_database_get(darktable.db),
                                  "SELECT s.imgid"
                                  " FROM main.selected_images as s, memory.collected_images as c"
                                  " WHERE s.imgid=c.imgid"
                                  " ORDER BY c.rowid LIMIT 1",
                                  -1, &stmt, NULL);
      // clang-format on
      if(stmt != NULL && sqlite3_step(stmt) == SQLITE_ROW)
      {
        ret = sqlite3_column_int(stmt, 0);
      }
      if(stmt) sqlite3_finalize(stmt);
    }
  }

  if((darktable.unmuted & DT_DEBUG_ACT_ON) == DT_DEBUG_ACT_ON)
    dt_print(DT_DEBUG_ACT_ON, "[images to act on] single image : %d\n", ret);

  return ret;
}

// get only the number of images to act on
int dt_act_on_get_images_nb(const gboolean only_visible, const gboolean force)
{
  // if the cache is valid (whatever the ordering) we return its value
  if(!force)
  {
    dt_act_on_cache_t *cache;
    if(only_visible)
      cache = &darktable.view_manager->act_on_cache_visible;
    else
      cache = &darktable.view_manager->act_on_cache_all;

    if(_test_cache(cache)) return cache->images_nb;
  }


  // otherwise we update the cache
  _cache_update(only_visible, force, FALSE);

  // and we return the number of images in cache
  if(only_visible && darktable.view_manager->act_on_cache_visible.ok)
    return darktable.view_manager->act_on_cache_visible.images_nb;
  else if(!only_visible && darktable.view_manager->act_on_cache_all.ok)
    return darktable.view_manager->act_on_cache_all.images_nb;
  else
    return 0;
}

// reset the cache
void dt_act_on_reset_cache(const gboolean only_visible)
{
  if(only_visible)
    darktable.view_manager->act_on_cache_visible.ok = FALSE;
  else
    darktable.view_manager->act_on_cache_all.ok = FALSE;
}
// clang-format off
// modelines: These editor modelines have been set for all relevant files by tools/update_modelines.py
// vim: shiftwidth=2 expandtab tabstop=2 cindent
// kate: tab-indents: off; indent-width 2; replace-tabs on; indent-mode cstyle; remove-trailing-spaces modified;
// clang-format on
