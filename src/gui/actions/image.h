#include "gui/actions/menu.h"
#include "common/grouping.h"
#include "common/colorlabels.h"
#include "common/ratings.h"


void rotate_counterclockwise_callback()
{
  dt_control_flip_images(1);
}

void rotate_clockwise_callback()
{
  dt_control_flip_images(0);
}

void reset_rotation_callback()
{
  dt_control_flip_images(2);
}

/** merges all the selected images into a single group.
 * if there is an expanded group, then they will be joined there, otherwise a new one will be created. */
void group_images_callback()
{
  int new_group_id = darktable.gui->expanded_group_id;
  GList *imgs = NULL;
  sqlite3_stmt *stmt;
  DT_DEBUG_SQLITE3_PREPARE_V2(dt_database_get(darktable.db), "SELECT imgid FROM main.selected_images", -1,
                              &stmt, NULL);
  while(sqlite3_step(stmt) == SQLITE_ROW)
  {
    int id = sqlite3_column_int(stmt, 0);
    if(new_group_id == -1) new_group_id = id;
    dt_grouping_add_to_group(new_group_id, id);
    imgs = g_list_prepend(imgs, GINT_TO_POINTER(id));
  }
  imgs = g_list_reverse(imgs); // list was built in reverse order, so un-reverse it
  sqlite3_finalize(stmt);
  if(darktable.gui->grouping)
    darktable.gui->expanded_group_id = new_group_id;
  else
    darktable.gui->expanded_group_id = -1;
  dt_collection_update_query(darktable.collection, DT_COLLECTION_CHANGE_RELOAD, DT_COLLECTION_PROP_GROUPING, imgs);
  dt_control_queue_redraw_center();
}

/** removes the selected images from their current group. */
void ungroup_images_callback()
{
  GList *imgs = NULL;
  sqlite3_stmt *stmt;
  DT_DEBUG_SQLITE3_PREPARE_V2(dt_database_get(darktable.db), "SELECT imgid FROM main.selected_images", -1,
                              &stmt, NULL);
  while(sqlite3_step(stmt) == SQLITE_ROW)
  {
    const int id = sqlite3_column_int(stmt, 0);
    const int new_group_id = dt_grouping_remove_from_group(id);
    if(new_group_id != -1)
    {
      // new_group_id == -1 if image to be ungrouped was a single image and no change to any group was made
      imgs = g_list_prepend(imgs, GINT_TO_POINTER(id));
    }
  }
  sqlite3_finalize(stmt);
  if(imgs != NULL)
  {
    darktable.gui->expanded_group_id = -1;
    dt_collection_update_query(darktable.collection, DT_COLLECTION_CHANGE_RELOAD, DT_COLLECTION_PROP_GROUPING,
                               g_list_reverse(imgs));
    dt_control_queue_redraw_center();
  }
}

static void _colorlabels_callback(int color)
{
  GList *imgs = dt_act_on_get_images(FALSE, TRUE, FALSE);
  dt_colorlabels_toggle_label_on_list(imgs, color, TRUE);
  dt_collection_update_query(darktable.collection, DT_COLLECTION_CHANGE_RELOAD, DT_COLLECTION_PROP_COLORLABEL, imgs);
}

static void _rating_callback(int value)
{
  GList *imgs = dt_act_on_get_images(FALSE, TRUE, FALSE);
  dt_ratings_apply_on_list(imgs, value, TRUE);
  dt_collection_update_query(darktable.collection, DT_COLLECTION_CHANGE_RELOAD, DT_COLLECTION_PROP_RATING, imgs);
}

void red_label_callback()
{
  _colorlabels_callback(0);
}

void yellow_label_callback()
{
  _colorlabels_callback(1);
}

void green_label_callback()
{
  _colorlabels_callback(2);
}

void blue_label_callback()
{
  _colorlabels_callback(3);
}

void magenta_label_callback()
{
  _colorlabels_callback(4);
}

void reset_label_callback()
{
  _colorlabels_callback(5);
}

void rating_one_callback()
{
  _rating_callback(1);
}

void rating_two_callback()
{
  _rating_callback(2);
}

void rating_three_callback()
{
  _rating_callback(3);
}

void rating_four_callback()
{
  _rating_callback(4);
}

void rating_five_callback()
{
  _rating_callback(5);
}

void rating_reset_callback()
{
  _rating_callback(0);
}

void rating_reject_callback()
{
  _rating_callback(6);
}


void append_image(GtkWidget **menus, GList **lists, const dt_menus_t index)
{
  /* Rotation */
  add_top_submenu_entry(menus, lists, _("Rotate"), index);
  GtkWidget *parent = get_last_widget(lists);

  add_sub_sub_menu_entry(parent, lists, _("90\302\260 counter-clockwise"), index, NULL,
                         rotate_counterclockwise_callback, NULL, NULL, sensitive_if_selected, 0, 0);

  add_sub_sub_menu_entry(parent, lists, _("90\302\260 clockwise"), index, NULL,
                         rotate_clockwise_callback, NULL, NULL, sensitive_if_selected, 0, 0);

  add_sub_menu_separator(parent);

  add_sub_sub_menu_entry(parent, lists, _("Reset rotation"), index, NULL,
                         reset_rotation_callback, NULL, NULL, sensitive_if_selected, 0, 0);

  /* Color labels */
  add_top_submenu_entry(menus, lists, _("Color labels"), index);
  parent = get_last_widget(lists);

  add_sub_sub_menu_entry(parent, lists, _("<span foreground='#BB2222'>\342\254\244</span> Red"), index, NULL,
                         red_label_callback, NULL, NULL, sensitive_if_selected, GDK_KEY_F1, 0);

  add_sub_sub_menu_entry(parent, lists, _("<span foreground='#BBBB22'>\342\254\244</span> Yellow"), index, NULL,
                         yellow_label_callback, NULL, NULL, sensitive_if_selected, GDK_KEY_F2, 0);

  add_sub_sub_menu_entry(parent, lists, _("<span foreground='#22BB22'>\342\254\244</span> Green"), index, NULL,
                         green_label_callback, NULL, NULL, sensitive_if_selected, GDK_KEY_F3, 0);

  add_sub_sub_menu_entry(parent, lists, _("<span foreground='#2222BB'>\342\254\244</span> Blue"), index, NULL,
                         blue_label_callback, NULL, NULL, sensitive_if_selected, GDK_KEY_F4, 0);

  add_sub_sub_menu_entry(parent, lists, _("<span foreground='#BB22BB'>\342\254\244</span> Magenta"), index, NULL,
                         magenta_label_callback, NULL, NULL, sensitive_if_selected, GDK_KEY_F5, 0);

  add_sub_menu_separator(parent);

  add_sub_sub_menu_entry(parent, lists, _("<span foreground='#BBBBBB'>\342\254\244</span> Clear labels"), index, NULL,
                         reset_label_callback, NULL, NULL, sensitive_if_selected, GDK_KEY_F6, 0);

  /* Ratings */
  add_top_submenu_entry(menus, lists, _("Ratings"), index);
  parent = get_last_widget(lists);

  add_sub_sub_menu_entry(parent, lists, _("Reject"), index, NULL,
                         rating_reject_callback, NULL, NULL, sensitive_if_selected, GDK_KEY_r, GDK_CONTROL_MASK);

  add_sub_sub_menu_entry(parent, lists, _("\342\230\205"), index, NULL,
                         rating_one_callback, NULL, NULL, sensitive_if_selected, GDK_KEY_1, GDK_CONTROL_MASK);

  add_sub_sub_menu_entry(parent, lists, _("\342\230\205\342\230\205"), index, NULL,
                         rating_two_callback, NULL, NULL, sensitive_if_selected, GDK_KEY_2, GDK_CONTROL_MASK);

  add_sub_sub_menu_entry(parent, lists, _("\342\230\205\342\230\205\342\230\205"), index, NULL,
                         rating_three_callback, NULL, NULL, sensitive_if_selected, GDK_KEY_3, GDK_CONTROL_MASK);

  add_sub_sub_menu_entry(parent, lists, _("\342\230\205\342\230\205\342\230\205\342\230\205"), index, NULL,
                         rating_four_callback, NULL, NULL, sensitive_if_selected, GDK_KEY_4, GDK_CONTROL_MASK);

  add_sub_sub_menu_entry(parent, lists, _("\342\230\205\342\230\205\342\230\205\342\230\205\342\230\205"), index, NULL,
                         rating_five_callback, NULL, NULL, sensitive_if_selected, GDK_KEY_5, GDK_CONTROL_MASK);

  add_sub_menu_separator(parent);

  add_sub_sub_menu_entry(parent, lists, _("Clear rating"), index, NULL,
                         rating_reset_callback, NULL, NULL, sensitive_if_selected, GDK_KEY_0, GDK_CONTROL_MASK);

  add_menu_separator(menus[index]);

  /* Reload EXIF */
  add_sub_menu_entry(menus, lists, _("Reload EXIF from file"), index, NULL, dt_control_refresh_exif, NULL, NULL,
                     sensitive_if_selected, 0, 0);

  add_menu_separator(menus[index]);

  /* Group/Ungroup */
  add_sub_menu_entry(menus, lists, _("Group images"), index, NULL, group_images_callback, NULL, NULL,
                     sensitive_if_selected, GDK_KEY_g, GDK_CONTROL_MASK);

  add_sub_menu_entry(menus, lists, _("Ungroup images"), index, NULL, ungroup_images_callback, NULL, NULL,
                     sensitive_if_selected, GDK_KEY_g, GDK_CONTROL_MASK | GDK_SHIFT_MASK);
}
