/*
 * Copyright (C) 2009-2011 Freescale Semiconductor, Inc. All rights reserved.
 *
 */

/*
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <stdio.h>
#include <string.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk/gdkkeysyms.h>
#include "imxplayer.h"
#include "menubar.h"
#include "controlbar.h"
#include "playengine.h"
#include "playlistbox.h"



static void file_open_cb(GtkWidget *widget, gpointer data)
{
  ImxPlayer *player = (ImxPlayer *)data;

  GtkWidget *dialog = gtk_file_chooser_dialog_new ("Open File",
                                        GTK_WINDOW(player->top_window),
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        GTK_STOCK_CANCEL,
                                        GTK_RESPONSE_CANCEL,
                                        GTK_STOCK_OPEN,
                                        GTK_RESPONSE_ACCEPT,
                                        NULL);
  GtkFileFilter *filter = gtk_file_filter_new ();
  //gtk_file_filter_add_pattern (filter, "*");
  gtk_file_filter_add_mime_type (filter, "video/*");
  gtk_file_filter_add_mime_type (filter, "audio/*");
  gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
    GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
    char *filename = gtk_file_chooser_get_filename (chooser);
    g_print("Selected file : %s\n", filename);
    player->playlistbox.add_file(&player->playlistbox, filename, TRUE, TRUE);
    g_free (filename);
  }

  gtk_widget_destroy (dialog);
}

static void dir_open_cb(GtkWidget *widget, gpointer data)
{
  ImxPlayer *player = (ImxPlayer *)data;

  GtkWidget *dialog = gtk_file_chooser_dialog_new ("Open Folder",
                                        GTK_WINDOW(player->top_window),
                                        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                                        GTK_STOCK_CANCEL,
                                        GTK_RESPONSE_CANCEL,
                                        GTK_STOCK_OPEN,
                                        GTK_RESPONSE_ACCEPT,
                                        NULL);
  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
    GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
    char *folder = gtk_file_chooser_get_uri (chooser);
    g_print("Selected folder : %s\n", folder);
    player->playlistbox.add_folder(&player->playlistbox, folder, TRUE, TRUE);
    g_free (folder);
  }

  gtk_widget_destroy (dialog);
}

static void playlist_cb(GtkWidget *widget, gpointer data)
{
  ImxPlayer *player = (ImxPlayer *)data;
  gint x, y, w, h;
  player->show_playlist = !player->show_playlist;

  if (player->show_playlist) {
    x = player->window_w - PLAYLISTBOX_W;
    w = PLAYLISTBOX_W;
    if (player->show_info) {
      y = MENUBAR_H + INFO_BOX_H;
    } else {
      y = MENUBAR_H;
    }
    h = player->window_h - y - CTRLBAR_H;

    playlistbox_resize(player->fixed_ct, &player->playlistbox,
                        x, y, w, h);

    if (player->video_w > x) {
      player->video_w_pre = player->video_w;
      player->video_w = x;
    }
  } else {
    if (player->video_w_pre > 0) {
      player->video_w = player->video_w_pre;
      player->video_w_pre = 0;
    }
  }

  player->playengine->set_render_rect(player->playengine, 0, 0,
                                      player->video_w, player->video_h);
  player->playengine->expose_video(player->playengine);
  playlistbox_show(&player->playlistbox, player->show_playlist);
}

static void rotate_0(GtkWidget *widget, gpointer data)
{
  ImxPlayer *player = (ImxPlayer *)data;
  player->playengine->rotate(player->playengine, 0);
}

static void rotate_90(GtkWidget *widget, gpointer data)
{
  ImxPlayer *player = (ImxPlayer *)data;
  player->playengine->rotate(player->playengine, 90);
}

static void rotate_180(GtkWidget *widget, gpointer data)
{
  ImxPlayer *player = (ImxPlayer *)data;
  player->playengine->rotate(player->playengine, 180);
}

static void rotate_270(GtkWidget *widget, gpointer data)
{
  ImxPlayer *player = (ImxPlayer *)data;
  player->playengine->rotate(player->playengine, 270);
}

static void resize_0(GtkWidget *widget, gpointer data) //full window
{
  ImxPlayer *player = (ImxPlayer *)data;
  player->video_w = player->window_w;
  player->video_h = player->window_h - MENUBAR_H;
  player->video_x = 0;
  player->video_y = MENUBAR_H;
  player->playengine->set_render_rect(player->playengine,
                                      0, 0, player->video_w, player->video_h);
  player->playengine->expose_video(player->playengine);
  gtk_widget_set_size_request(GTK_WIDGET(player->video_win),
                              player->video_w, player->video_h);
  gtk_fixed_move(GTK_FIXED(player->fixed_ct), GTK_WIDGET(player->video_win),
                           player->video_x, player->video_y);
}

static void resize_1(GtkWidget *widget, gpointer data)  //1/2 left top
{
  ImxPlayer *player = (ImxPlayer *)data;
  player->video_w = player->window_w / 2;
  player->video_h = (player->window_h - MENUBAR_H) / 2;
  player->video_x = 0;
  player->video_y = MENUBAR_H;
  gtk_widget_set_size_request(GTK_WIDGET(player->video_win),
                              player->video_w, player->video_h);
  gtk_fixed_move(GTK_FIXED(player->fixed_ct), GTK_WIDGET(player->video_win),
                           player->video_x, player->video_y);
}

static void resize_2(GtkWidget *widget, gpointer data)  //1/4 right top
{
  ImxPlayer *player = (ImxPlayer *)data;
  player->video_w = player->window_w / 4;
  player->video_h = (player->window_h - MENUBAR_H) / 4;
  player->video_x = player->window_w / 2;
  player->video_y = MENUBAR_H;
  gtk_widget_set_size_request(GTK_WIDGET(player->video_win),
                              player->video_w, player->video_h);
  gtk_fixed_move(GTK_FIXED(player->fixed_ct), GTK_WIDGET(player->video_win),
                           player->video_x, player->video_y);
}

static void resize_3(GtkWidget *widget, gpointer data)  //1/2 left bottom
{
  ImxPlayer *player = (ImxPlayer *)data;
  player->video_w = player->window_w / 2;
  player->video_h = (player->window_h - MENUBAR_H) / 2;
  player->video_x = 0;
  player->video_y = (player->window_h - MENUBAR_H) / 2;
  gtk_widget_set_size_request(GTK_WIDGET(player->video_win),
                              player->video_w, player->video_h);
  gtk_fixed_move(GTK_FIXED(player->fixed_ct), GTK_WIDGET(player->video_win),
                           player->video_x, player->video_y);
}

static void resize_4(GtkWidget *widget, gpointer data)  //1/4 right bottom
{
  ImxPlayer *player = (ImxPlayer *)data;
  player->video_w = player->window_w / 4;
  player->video_h = (player->window_h - MENUBAR_H) / 4;
  player->video_x = player->window_w / 2;
  player->video_y = (player->window_h - MENUBAR_H) / 2;
  gtk_widget_set_size_request(GTK_WIDGET(player->video_win),
                              player->video_w, player->video_h);
  gtk_fixed_move(GTK_FIXED(player->fixed_ct), GTK_WIDGET(player->video_win),
                           player->video_x, player->video_y);
}

static void resize_5(GtkWidget *widget, gpointer data)  //1/8 center
{
  ImxPlayer *player = (ImxPlayer *)data;
  player->video_w = player->window_w / 8;
  player->video_h = (player->window_h - MENUBAR_H) / 8;
  player->video_x = (player->window_w - player->window_w / 8) / 2;;
  player->video_y = (player->window_h-MENUBAR_H)-(player->window_h-MENUBAR_H)/8;
  player->video_y /= 2;
  gtk_widget_set_size_request(GTK_WIDGET(player->video_win),
                              player->video_w, player->video_h);
  gtk_fixed_move(GTK_FIXED(player->fixed_ct), GTK_WIDGET(player->video_win),
                           player->video_x, player->video_y);
}

static void resize_6(GtkWidget *widget, gpointer data)  //1/2 render rectangle
{
  ImxPlayer *player = (ImxPlayer *)data;
  player->video_w = player->window_w / 2;
  player->video_h = (player->window_h - MENUBAR_H);
  player->video_x = 0;
  player->video_y = MENUBAR_H;
  player->playengine->set_render_rect(player->playengine,
                                      player->video_w/4, player->video_h/4,
                                      player->video_w/2, player->video_h/2);
  player->playengine->expose_video(player->playengine);
  gtk_widget_set_size_request(GTK_WIDGET(player->video_win),
                              player->video_w, player->video_h);
  gtk_fixed_move(GTK_FIXED(player->fixed_ct), GTK_WIDGET(player->video_win),
                           player->video_x, player->video_y);
}

static void move_left(GtkWidget *widget, gpointer data)
{
  ImxPlayer *player = (ImxPlayer *)data;
  if (player->video_x > WINDOW_MOVE_STEP)
    player->video_x -= WINDOW_MOVE_STEP;
  else
    player->video_x = 0;

  gtk_fixed_move(GTK_FIXED(player->fixed_ct), GTK_WIDGET(data),
                 player->video_x, player->video_y);
}

static void move_right(GtkWidget *widget, gpointer data)
{
  ImxPlayer *player = (ImxPlayer *)data;
  if (player->video_x + player->video_w < (player->window_w - WINDOW_MOVE_STEP))
    player->video_x += WINDOW_MOVE_STEP;
  else
    player->video_x = player->window_w - player->video_w;

  gtk_fixed_move(GTK_FIXED(player->fixed_ct), GTK_WIDGET(data),
                 player->video_x, player->video_y);
}

static void move_up(GtkWidget *widget, gpointer data)
{
  ImxPlayer *player = (ImxPlayer *)data;
  if (player->video_y > WINDOW_MOVE_STEP + MENUBAR_H)
    player->video_y -= WINDOW_MOVE_STEP;
  else
    player->video_y = MENUBAR_H;

  gtk_fixed_move(GTK_FIXED(player->fixed_ct), GTK_WIDGET(data),
                 player->video_x, player->video_y);
}

static void move_down(GtkWidget *widget, gpointer data)
{
  ImxPlayer *player = (ImxPlayer *)data;
  if (player->video_y + player->video_h <
      (player->window_h - MENUBAR_H - WINDOW_MOVE_STEP))
    player->video_y += WINDOW_MOVE_STEP;
  else
    player->video_y = player->window_h - MENUBAR_H - player->video_h;

  gtk_fixed_move(GTK_FIXED(player->fixed_ct), GTK_WIDGET(data),
                 player->video_x, player->video_y);
}

static void video_chose(GtkWidget *widget, gpointer data)
{
  ImxPlayer *player = (ImxPlayer *)data;
  const gchar *label = gtk_menu_item_get_label(GTK_MENU_ITEM(widget));
  GList *list = GTK_MENU_SHELL(player->menubar.videomenu)->children;
  gint index = 0;
  const gchar *item_label;

  for (; list; list = list->next) {
    item_label = gtk_menu_item_get_label(list->data);
    if(!strcmp(item_label, label)) {
      player->playengine->select_video(player->playengine, index);
      break;
    }
    index++;
  }
}

static void audio_chose(GtkWidget *widget, gpointer data)
{
  ImxPlayer *player = (ImxPlayer *)data;
  const gchar *label = gtk_menu_item_get_label(GTK_MENU_ITEM(widget));
  GList *list = GTK_MENU_SHELL(player->menubar.audiomenu)->children;
  gint index = 0;
  const gchar *item_label;

  for (; list; list = list->next) {
    item_label = gtk_menu_item_get_label(list->data);
    if(!strcmp(item_label, label)) {
      player->playengine->select_audio(player->playengine, index);
      break;
    }
    index++;
  }
}

static void subtitle_chose(GtkWidget *widget, gpointer data)
{
  ImxPlayer *player = (ImxPlayer *)data;
  const gchar *label = gtk_menu_item_get_label(GTK_MENU_ITEM(widget));
  GList *list = GTK_MENU_SHELL(player->menubar.subtitlemenu)->children;
  gint index = 0;
  const gchar *item_label;

  for (; list; list = list->next) {
    item_label = gtk_menu_item_get_label(list->data);
    if(!strcmp(item_label, label)) {
      player->playengine->select_subtitle(player->playengine, index);
      break;
    }
    index++;
  }
}

static void load_subtitle(GtkWidget *widget, gpointer data)
{
  ImxPlayer *player = (ImxPlayer *)data;

  GtkWidget *dialog = gtk_file_chooser_dialog_new ("Open Subtitle",
                                        GTK_WINDOW(player->top_window),
                                        GTK_FILE_CHOOSER_ACTION_OPEN,
                                        GTK_STOCK_CANCEL,
                                        GTK_RESPONSE_CANCEL,
                                        GTK_STOCK_OPEN,
                                        GTK_RESPONSE_ACCEPT,
                                        NULL);
  GtkFileFilter *filter = gtk_file_filter_new ();
  gtk_file_filter_add_pattern (filter, "*.srt");
  gtk_file_filter_add_pattern (filter, "*.sub");
  gtk_file_filter_add_pattern (filter, "*.ass");
  gtk_file_filter_add_pattern (filter, "*.SRT");
  gtk_file_filter_add_pattern (filter, "*.SUB");
  gtk_file_filter_add_pattern (filter, "*.ASS");
  gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

  if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
    GtkFileChooser *chooser = GTK_FILE_CHOOSER (dialog);
    char *filename = gtk_file_chooser_get_filename (chooser);
    g_print("Selected subtitle : %s\n", filename);
    //TODO parse subtitle
    g_free (filename);
  }

  gtk_widget_destroy (dialog);
}

static void origin_ratio(GtkWidget *widget, gpointer data)
{
  ImxPlayer *player = (ImxPlayer *)data;
  player->playengine->force_ratio(player->playengine, TRUE);
  player->playengine->set_render_rect(player->playengine,
                                      0, 0, player->video_w, player->video_h);
  player->playengine->expose_video(player->playengine);
}

static void full_fill(GtkWidget *widget, gpointer data)
{
  ImxPlayer *player = (ImxPlayer *)data;
  player->playengine->force_ratio(player->playengine, FALSE);
  player->playengine->set_render_rect(player->playengine,
                                      0, 0, player->video_w, player->video_h);
  player->playengine->expose_video(player->playengine);
}

static void origin_size(GtkWidget *widget, gpointer data)
{
  ImxPlayer *player = (ImxPlayer *)data;
  gint video_w, video_h;
  video_w = player->meta.width;
  video_h = player->meta.height;
  GtkWidget *dialog;

  if (video_w == 0 || video_h == 0) {
    gchar str[256];
    sprintf(str, "Wrong video resolution: %d X %d", video_w, video_h);
    dialog = gtk_message_dialog_new(GTK_WINDOW(player->top_window),
                                      GTK_DIALOG_DESTROY_WITH_PARENT,
                                      GTK_MESSAGE_WARNING,
                                      GTK_BUTTONS_OK,
                                      "%s", str);
    gtk_window_set_title(GTK_WINDOW(dialog), "Warning");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
  } else if (video_w > player->screen_w || video_h > player->screen_h){
    dialog = gtk_message_dialog_new(GTK_WINDOW(player->top_window),
                                      GTK_DIALOG_DESTROY_WITH_PARENT,
                                      GTK_MESSAGE_WARNING,
                                      GTK_BUTTONS_OK,
          "This function only for those video which resolution"
          " less then screen resolution");
    gtk_window_set_title(GTK_WINDOW(dialog), "Warning");
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
  } else {
    gint x,y;
    x = (player->window_w - video_w) / 2;
    y = (player->window_h - video_h) / 2;
    player->playengine->set_render_rect(player->playengine,
                                        x, y, video_w, video_h);
    player->playengine->expose_video(player->playengine);
  }
}

static void about(GtkWidget *widget, gpointer data)
{
  GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file("logo.png", NULL);

  GtkWidget *dialog = gtk_about_dialog_new();
  gtk_about_dialog_set_name(GTK_ABOUT_DIALOG(dialog), "IMXPlayer");
  gtk_about_dialog_set_version(GTK_ABOUT_DIALOG(dialog), "0.1");
  gtk_about_dialog_set_copyright(GTK_ABOUT_DIALOG(dialog),
      "(c) Copyright Freescale 2014, All rights reserved.");
  gtk_about_dialog_set_comments(GTK_ABOUT_DIALOG(dialog),
     "IMXPlayer is a simple media player for Freescale i.MAX SoC Testing.");
  gtk_about_dialog_set_website(GTK_ABOUT_DIALOG(dialog),
      "http://www.freescale.com");
  gtk_about_dialog_set_logo(GTK_ABOUT_DIALOG(dialog), pixbuf);
  g_object_unref(pixbuf), pixbuf = NULL;
  gtk_dialog_run(GTK_DIALOG (dialog));
  gtk_widget_destroy(dialog);
}

static void help(GtkWidget *widget, gpointer data)
{
  ImxPlayer *player = (ImxPlayer *)data;
  gchar helpstr[] = "IMXPlayer Usage: \n"
   "./imxplayer -- start player without a file, use file->open to play a file\n"
   "./imxplayer mediafile -- start player with a specified file\n";

  GtkWidget *dialog, *label, *content_area;
  dialog = gtk_dialog_new_with_buttons ("Help",
                                         GTK_WINDOW(player->top_window),
                                         GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_STOCK_OK,
                                         GTK_RESPONSE_NONE,
                                         NULL);
  content_area = gtk_dialog_get_content_area (GTK_DIALOG (dialog));
  label = gtk_label_new (helpstr);
  /* Ensure that the dialog box is destroyed when the user responds. */
  g_signal_connect_swapped (dialog, "response",
                            G_CALLBACK (gtk_widget_destroy), dialog);

  gtk_container_add (GTK_CONTAINER (content_area), label);
  gtk_widget_show_all (dialog);
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}

void menubar_create(void *imxplayer)
{
  GtkWidget *sep;
  GtkWidget *submenu;
  GtkWidget *item;
  GtkAccelGroup *accel_group = NULL;
  ImxPlayer *player = (ImxPlayer *)imxplayer;
  MenuBar *menubar = &player->menubar;

  menubar->menubar = gtk_menu_bar_new();
  gtk_fixed_put(GTK_FIXED(player->fixed_ct), menubar->menubar, 0, 0);
  gtk_widget_set_size_request(menubar->menubar, player->window_w, MENUBAR_H);
  accel_group = gtk_accel_group_new();
  gtk_window_add_accel_group(GTK_WINDOW(player->top_window), accel_group);
  //gtk_widget_modify_bg(menubar->menubar, GTK_STATE_NORMAL, &player->color_key);

  // File menu
  menubar->filemenu = gtk_menu_new();
  submenu = gtk_menu_item_new_with_mnemonic("_File");
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(submenu), menubar->filemenu);

  item = gtk_image_menu_item_new_from_stock(GTK_STOCK_OPEN, NULL);
  gtk_widget_add_accelerator(item, "activate", accel_group,
       GDK_q, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
  g_signal_connect(G_OBJECT(item), "activate",G_CALLBACK(file_open_cb), player);
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar->filemenu), item);

  item = gtk_menu_item_new_with_label("Open Folder");
  gtk_widget_add_accelerator(item, "activate", accel_group,
       GDK_d, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
  g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(dir_open_cb), player);
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar->filemenu), item);

  item = gtk_menu_item_new_with_label("Playlist");
  gtk_widget_add_accelerator(item, "activate", accel_group,
       GDK_p, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
  g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(playlist_cb), player);
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar->filemenu), item);

  sep = gtk_separator_menu_item_new();
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar->filemenu), sep);

  item = gtk_image_menu_item_new_from_stock(GTK_STOCK_QUIT, accel_group);
  gtk_widget_add_accelerator(item, "activate", accel_group,
       GDK_o, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
  g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(gtk_main_quit), NULL);
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar->filemenu), item);

  gtk_menu_shell_append(GTK_MENU_SHELL(menubar->menubar), submenu);

  //Rotate menu
  menubar->rotatemenu = gtk_menu_new();
  submenu = gtk_menu_item_new_with_mnemonic("_Rotate");
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(submenu), menubar->rotatemenu);

  item = gtk_menu_item_new_with_label("Rotate 0");
  g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(rotate_0), player);
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar->rotatemenu), item);

  item = gtk_menu_item_new_with_label("Rotate 90");
  g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(rotate_90), player);
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar->rotatemenu), item);

  item = gtk_menu_item_new_with_label("Rotate 90");
  g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(rotate_180), player);
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar->rotatemenu), item);

  item = gtk_menu_item_new_with_label("Rotate 90");
  g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(rotate_270), player);
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar->rotatemenu), item);

  gtk_menu_shell_append(GTK_MENU_SHELL(menubar->menubar), submenu);

  //Resize menu
  menubar->resizemenu = gtk_menu_new();
  submenu = gtk_menu_item_new_with_mnemonic("Resi_ze");
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(submenu), menubar->resizemenu);

  item = gtk_menu_item_new_with_label("FULL_WINDOW");
  g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(resize_0), player);
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar->resizemenu), item);

  item = gtk_menu_item_new_with_label("TOP_LEFT_HALF");
  g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(resize_1), player);
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar->resizemenu), item);

  item = gtk_menu_item_new_with_label("TOP_RIGHT_QUARTER");
  g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(resize_2), player);
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar->resizemenu), item);

  item = gtk_menu_item_new_with_label("BOTTOM_LEFT_HALF");
  g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(resize_3), player);
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar->resizemenu), item);

  item = gtk_menu_item_new_with_label("BOTTOM_RIGHT_QUARTER");
  g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(resize_4), player);
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar->resizemenu), item);

  item = gtk_menu_item_new_with_label("CENTER_EIGHTH");
  g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(resize_5), player);
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar->resizemenu), item);

  item = gtk_menu_item_new_with_label("CENTER_HALF_RENDER_RECT");
  g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(resize_6), player);
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar->resizemenu), item);

  gtk_menu_shell_append(GTK_MENU_SHELL(menubar->menubar), submenu);

  //Move menu
  menubar->movemenu = gtk_menu_new();
  submenu = gtk_menu_item_new_with_mnemonic("_Move");
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(submenu), menubar->movemenu);

  item = gtk_menu_item_new_with_label("Move Left");
  g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(move_left), player);
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar->movemenu), item);

  item = gtk_menu_item_new_with_label("Move Right");
  g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(move_right), player);
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar->movemenu), item);

  item = gtk_menu_item_new_with_label("Move Up");
  g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(move_up), player);
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar->movemenu), item);

  item = gtk_menu_item_new_with_label("Move Down");
  g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(move_down), player);
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar->movemenu), item);

  gtk_menu_shell_append(GTK_MENU_SHELL(menubar->menubar), submenu);

  //Video menu, empty menu, updated when got video numbers
  menubar->videomenu = gtk_menu_new();
  submenu = gtk_menu_item_new_with_mnemonic("_Video");
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(submenu), menubar->videomenu);
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar->menubar), submenu);

  //Audio Track menu, empty menu, updated when got audio numbers
  menubar->audiomenu = gtk_menu_new();
  submenu = gtk_menu_item_new_with_mnemonic("_Audio Track");
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(submenu), menubar->audiomenu);
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar->menubar), submenu);

  //Subtitle menu, empty menu, update when got subtitle numbers
  menubar->subtitlemenu = gtk_menu_new();
  submenu = gtk_menu_item_new_with_mnemonic("_Subtitle");
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(submenu), menubar->subtitlemenu);

  item = gtk_menu_item_new_with_label("Load from File...");
  g_signal_connect(G_OBJECT(item), "activate",
      G_CALLBACK(load_subtitle), player);
  gtk_menu_shell_append(GTK_MENU_SHELL(player->menubar.subtitlemenu), item);

  gtk_menu_shell_append(GTK_MENU_SHELL(menubar->menubar), submenu);

  //Aspect menu
  menubar->aspectmenu = gtk_menu_new();
  submenu = gtk_menu_item_new_with_mnemonic("Aspec_t");
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(submenu), menubar->aspectmenu);

  item = gtk_menu_item_new_with_label("Original Ratio");
  g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(origin_ratio),player);
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar->aspectmenu), item);

  item = gtk_menu_item_new_with_label("Full Fill");
  g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(full_fill), player);
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar->aspectmenu), item);

  item = gtk_menu_item_new_with_label("Original Size");
  g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(origin_size), player);
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar->aspectmenu), item);

  gtk_menu_shell_append(GTK_MENU_SHELL(menubar->menubar), submenu);

  //Help menu
  menubar->helpmenu = gtk_menu_new();
  submenu = gtk_menu_item_new_with_mnemonic("_Help");
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(submenu), menubar->helpmenu);

  item = gtk_image_menu_item_new_from_stock(GTK_STOCK_ABOUT, NULL);
  g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(about),player);
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar->helpmenu), item);

  item = gtk_image_menu_item_new_from_stock(GTK_STOCK_HELP, NULL);
  g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(help), player);
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar->helpmenu), item);

  gtk_menu_shell_append(GTK_MENU_SHELL(menubar->menubar), submenu);
}

void menubar_show(MenuBar *menubar, gboolean show)
{
  if (show)
    gtk_widget_show(menubar->menubar);
  else
    gtk_widget_hide(menubar->menubar);
}

void menubar_update(void *imxplayer)
{
  ImxPlayer *player = (ImxPlayer *)imxplayer;

  GtkMenuShell *menu = GTK_MENU_SHELL(player->menubar.videomenu);
  GList *list = NULL;
  for (list = menu->children; list; list = list->next) {
    menu->children = g_list_remove(menu->children, list->data);
  }

  menu = GTK_MENU_SHELL(player->menubar.audiomenu);
  for (list = menu->children; list; list = list->next) {
    menu->children = g_list_remove(menu->children, list->data);
  }

  menu = GTK_MENU_SHELL(player->menubar.subtitlemenu);
  for (list = menu->children->next; list; list = list->next) {
    menu->children = g_list_remove(menu->children, list->data);
  }


  gint video_no = player->playengine->get_video_num(player->playengine);
  gint audio_no = player->playengine->get_audio_num(player->playengine);
  gint text_no = player->playengine->get_subtitle_num(player->playengine);

  GtkWidget *item;
  gint i;
  gchar label[128];

  printf("video,audio,text: %d %d %d\n", video_no, audio_no, text_no);

  for (i = 0; i < video_no; i++) {
    sprintf(label, "Video %d", i+1);
    item = gtk_menu_item_new_with_label(label);
    g_signal_connect(G_OBJECT(item), "activate",
        G_CALLBACK(video_chose), player);
    gtk_menu_shell_append(GTK_MENU_SHELL(player->menubar.videomenu), item);
    gtk_widget_show(item);
  }

  for (i = 0; i < audio_no; i++) {
    sprintf(label, "Audio %d: %s", i+1, player->playengine->audio_lang[i]);
    item = gtk_menu_item_new_with_label(label);
    g_signal_connect(G_OBJECT(item), "activate",
        G_CALLBACK(audio_chose), player);
    gtk_menu_shell_append(GTK_MENU_SHELL(player->menubar.audiomenu), item);
    gtk_widget_show(item);
  }

  for (i = 0; i < text_no; i++) {
    sprintf(label, "Subtitle %d: %s", i+1, player->playengine->text_lang[i]);
    item = gtk_menu_item_new_with_label(label);
    g_signal_connect(G_OBJECT(item), "activate",
        G_CALLBACK(subtitle_chose), player);
    gtk_menu_shell_append(GTK_MENU_SHELL(player->menubar.subtitlemenu), item);
    gtk_widget_show(item);
  }
}
