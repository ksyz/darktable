/*
    This file is part of darktable,
    copyright (c) 2009--2011 johannes hanika.
    copyright (c) 2012 henrik andersson.
    copyright (c) 2012 tobias ellinghaus.

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
#include "views/capture.h"
#include "bauhaus/bauhaus.h"
#include "common/darktable.h"
#include "common/camera_control.h"
#include "control/jobs.h"
#include "control/control.h"
#include "control/conf.h"
#include "libs/lib.h"
#include "gui/accelerators.h"
#include "gui/gtk.h"
#include "gui/draw.h"
#include "gui/guides.h"
#include "dtgtk/label.h"
#include <gdk/gdkkeysyms.h>
#include "dtgtk/button.h"

#define GUIDE_NONE 0
#define GUIDE_GRID 1
#define GUIDE_THIRD 2
#define GUIDE_DIAGONAL 3
#define GUIDE_TRIANGL 4
#define GUIDE_GOLDEN 5

DT_MODULE(1)

typedef struct dt_lib_live_view_t
{
  GtkWidget *live_view, *live_view_zoom, *rotate_ccw, *rotate_cw;
  GtkWidget *focus_out_small, *focus_out_big, *focus_in_small, *focus_in_big;
  GtkWidget *guide_selector, *flip_guides, *golden_extras;
}
dt_lib_live_view_t;

static void
guides_presets_changed (GtkWidget *combo, dt_lib_live_view_t *lib)
{
  int which = dt_bauhaus_combobox_get(combo);
  if (which == GUIDE_TRIANGL || which == GUIDE_GOLDEN )
    gtk_widget_set_visible(GTK_WIDGET(lib->flip_guides), TRUE);
  else
    gtk_widget_set_visible(GTK_WIDGET(lib->flip_guides), FALSE);

  if (which == GUIDE_GOLDEN)
    gtk_widget_set_visible(GTK_WIDGET(lib->golden_extras), TRUE);
  else
    gtk_widget_set_visible(GTK_WIDGET(lib->golden_extras), FALSE);
}


const char*
name ()
{
  return _("live view");
}

uint32_t views()
{
  return DT_VIEW_TETHERING;
}

uint32_t container()
{
  return DT_UI_CONTAINER_PANEL_RIGHT_CENTER;
}


void
gui_reset (dt_lib_module_t *self)
{
}

int
position ()
{
  return 998;
}

void init_key_accels(dt_lib_module_t *self)
{
  dt_accel_register_lib(self, NC_("accel", "toggle live view"), GDK_v, 0);
  dt_accel_register_lib(self, NC_("accel", "rotate 90 degrees ccw"), 0, 0);
  dt_accel_register_lib(self, NC_("accel", "rotate 90 degrees cw"), 0, 0);
  dt_accel_register_lib(self, NC_("accel", "move focus point in (big steps)"), 0, 0);
  dt_accel_register_lib(self, NC_("accel", "move focus point in (small steps)"), 0, 0);
  dt_accel_register_lib(self, NC_("accel", "move focus point out (small steps)"), 0, 0);
  dt_accel_register_lib(self, NC_("accel", "move focus point out (big steps)"), 0, 0);
}

void connect_key_accels(dt_lib_module_t *self)
{
  dt_lib_live_view_t *lib = (dt_lib_live_view_t*)self->data;

  dt_accel_connect_button_lib(self, "toggle live view", GTK_WIDGET(lib->live_view));
  dt_accel_connect_button_lib(self, "rotate 90 degrees ccw", GTK_WIDGET(lib->rotate_ccw));
  dt_accel_connect_button_lib(self, "rotate 90 degrees cw", GTK_WIDGET(lib->rotate_cw));
  dt_accel_connect_button_lib(self, "move focus point in (big steps)", GTK_WIDGET(lib->focus_in_big));
  dt_accel_connect_button_lib(self, "move focus point in (small steps)", GTK_WIDGET(lib->focus_in_small));
  dt_accel_connect_button_lib(self, "move focus point out (small steps)", GTK_WIDGET(lib->focus_out_small));
  dt_accel_connect_button_lib(self, "move focus point out (big steps)", GTK_WIDGET(lib->focus_out_big));
}

static void _rotate_ccw(GtkWidget *widget, gpointer user_data)
{
  dt_camera_t *cam = (dt_camera_t*)darktable.camctl->active_camera;
  cam->live_view_rotation = (cam->live_view_rotation + 1) % 4; // 0 -> 1 -> 2 -> 3 -> 0 -> ...
}

static void _rotate_cw(GtkWidget *widget, gpointer user_data)
{
  dt_camera_t *cam = (dt_camera_t*)darktable.camctl->active_camera;
  cam->live_view_rotation = (cam->live_view_rotation + 3) % 4; // 0 -> 3 -> 2 -> 1 -> 0 -> ...
}

// Congratulations to Simon for being the first one recognizing live view in a screen shot ^^
static void _toggle_live_view_clicked(GtkWidget *widget, gpointer user_data)
{
  if(gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(widget) ) == TRUE)
  {
    if(dt_camctl_camera_start_live_view(darktable.camctl) == FALSE)
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(widget), FALSE);
  }
  else
  {
    dt_camctl_camera_stop_live_view(darktable.camctl);
  }
}

// TODO: using a toggle button would be better, but this setting can also be chhanged by right clicking on the canvas (src/views/capture.c).
//       maybe using a signal would work? i have no idea.
static void _zoom_live_view_clicked(GtkWidget *widget, gpointer user_data)
{
  dt_camera_t *cam = (dt_camera_t*)darktable.camctl->active_camera;
  if(cam->is_live_viewing)
  {
    cam->live_view_zoom = !cam->live_view_zoom;
    if(cam->live_view_zoom == TRUE)
      dt_camctl_camera_set_property(darktable.camctl, NULL, "eoszoom", "5");
    else
      dt_camctl_camera_set_property(darktable.camctl, NULL, "eoszoom", "1");
  }
}

static const gchar *focus_array[] = {"Near 3", "Near 2", "Near 1", "Far 1", "Far 2", "Far 3"};
static void _focus_button_clicked(GtkWidget *widget, gpointer user_data)
{
  long int focus = (long int) user_data;
  if(focus >= 0 && focus <= 5)
    dt_camctl_camera_set_property(darktable.camctl, NULL, "manualfocusdrive", g_dgettext("libgphoto2-2", focus_array[focus]));
}

void
gui_init (dt_lib_module_t *self)
{
  self->data = malloc(sizeof(dt_lib_live_view_t));
  memset(self->data,0,sizeof(dt_lib_live_view_t));

  // Setup lib data
  dt_lib_live_view_t *lib=self->data;

  // Setup gui
  self->widget = gtk_vbox_new(FALSE, 5);
  GtkWidget *box;

  box = gtk_hbox_new(FALSE, 5);
  gtk_box_pack_start(GTK_BOX(self->widget), box, TRUE, TRUE, 0);
  lib->live_view       = dtgtk_togglebutton_new(dtgtk_cairo_paint_eye, CPF_STYLE_FLAT|CPF_DO_NOT_USE_BORDER);
  lib->live_view_zoom  = dtgtk_button_new(dtgtk_cairo_paint_zoom, CPF_STYLE_FLAT|CPF_DO_NOT_USE_BORDER); // TODO: see _zoom_live_view_clicked
  lib->rotate_ccw      = dtgtk_button_new(dtgtk_cairo_paint_refresh, CPF_STYLE_FLAT|CPF_DO_NOT_USE_BORDER);
  lib->rotate_cw       = dtgtk_button_new(dtgtk_cairo_paint_refresh, CPF_STYLE_FLAT|CPF_DO_NOT_USE_BORDER|1);

  gtk_box_pack_start(GTK_BOX(box), lib->live_view, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(box), lib->live_view_zoom, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(box), lib->rotate_ccw, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(box), lib->rotate_cw, TRUE, TRUE, 0);

  g_object_set(G_OBJECT( lib->live_view), "tooltip-text", _("toggle live view"), (char *)NULL);
  g_object_set(G_OBJECT( lib->live_view_zoom), "tooltip-text", _("zoom live view"), (char *)NULL);
  g_object_set(G_OBJECT( lib->rotate_ccw), "tooltip-text", _("rotate 90 degrees ccw"), (char *)NULL);
  g_object_set(G_OBJECT( lib->rotate_cw), "tooltip-text", _("rotate 90 degrees cw"), (char *)NULL);

  g_signal_connect(G_OBJECT(lib->live_view), "clicked", G_CALLBACK(_toggle_live_view_clicked), lib);
  g_signal_connect(G_OBJECT(lib->live_view_zoom), "clicked", G_CALLBACK(_zoom_live_view_clicked), lib);
  g_signal_connect(G_OBJECT(lib->rotate_ccw), "clicked", G_CALLBACK(_rotate_ccw), lib);
  g_signal_connect(G_OBJECT(lib->rotate_cw), "clicked", G_CALLBACK(_rotate_cw), lib);

  // focus buttons
  box = gtk_hbox_new(FALSE, 5);
  gtk_box_pack_start(GTK_BOX(self->widget), box, TRUE, TRUE, 0);
  lib->focus_in_big    = dtgtk_button_new(dtgtk_cairo_paint_solid_triangle, CPF_STYLE_FLAT|CPF_DO_NOT_USE_BORDER|CPF_DIRECTION_LEFT);
  lib->focus_in_small  = dtgtk_button_new(dtgtk_cairo_paint_arrow, CPF_STYLE_FLAT|CPF_DO_NOT_USE_BORDER|CPF_DIRECTION_LEFT); // TODO icon not centered
  lib->focus_out_small = dtgtk_button_new(dtgtk_cairo_paint_arrow, CPF_STYLE_FLAT|CPF_DO_NOT_USE_BORDER|CPF_DIRECTION_RIGHT); // TODO same here
  lib->focus_out_big   = dtgtk_button_new(dtgtk_cairo_paint_solid_triangle, CPF_STYLE_FLAT|CPF_DO_NOT_USE_BORDER|CPF_DIRECTION_RIGHT);

  gtk_box_pack_start(GTK_BOX(box), lib->focus_in_big, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(box), lib->focus_in_small, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(box), lib->focus_out_small, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(box), lib->focus_out_big, TRUE, TRUE, 0);

  g_object_set(G_OBJECT( lib->focus_in_big), "tooltip-text", _("move focus point in (big steps)"), (char *)NULL);
  g_object_set(G_OBJECT( lib->focus_in_small), "tooltip-text", _("move focus point in (small steps)"), (char *)NULL);
  g_object_set(G_OBJECT( lib->focus_out_small), "tooltip-text", _("move focus point out (small steps)"), (char *)NULL);
  g_object_set(G_OBJECT( lib->focus_out_big), "tooltip-text", _("move focus point out (big steps)"), (char *)NULL);

  // 1 and 4 would be medium steps, not in ui right now ...
  g_signal_connect(G_OBJECT(lib->focus_in_big), "clicked", G_CALLBACK(_focus_button_clicked), (gpointer)0);
  g_signal_connect(G_OBJECT(lib->focus_in_small), "clicked", G_CALLBACK(_focus_button_clicked), (gpointer)2);
  g_signal_connect(G_OBJECT(lib->focus_out_small), "clicked", G_CALLBACK(_focus_button_clicked), (gpointer)3);
  g_signal_connect(G_OBJECT(lib->focus_out_big), "clicked", G_CALLBACK(_focus_button_clicked), (gpointer)5);

  // Guides
  lib->guide_selector = dt_bauhaus_combobox_new(NULL);
  dt_bauhaus_widget_set_label(lib->guide_selector, _("guides"));
  dt_bauhaus_combobox_add(lib->guide_selector, _("none"));
  dt_bauhaus_combobox_add(lib->guide_selector, _("grid"));
  dt_bauhaus_combobox_add(lib->guide_selector, _("rules of thirds"));
  dt_bauhaus_combobox_add(lib->guide_selector, _("diagonal method"));
  dt_bauhaus_combobox_add(lib->guide_selector, _("harmonious triangles"));
  dt_bauhaus_combobox_add(lib->guide_selector, _("golden mean"));
  g_object_set(G_OBJECT(lib->guide_selector), "tooltip-text", _("display guide lines to help compose your photograph"), (char *)NULL);
  g_signal_connect (G_OBJECT (lib->guide_selector), "value-changed", G_CALLBACK (guides_presets_changed), lib);
  gtk_box_pack_start(GTK_BOX(self->widget), lib->guide_selector, TRUE, TRUE, 0);

  lib->flip_guides = dt_bauhaus_combobox_new(NULL);
  dt_bauhaus_widget_set_label(lib->flip_guides, _("flip"));
  dt_bauhaus_combobox_add(lib->flip_guides, _("none"));
  dt_bauhaus_combobox_add(lib->flip_guides, _("horizontally"));
  dt_bauhaus_combobox_add(lib->flip_guides, _("vertically"));
  dt_bauhaus_combobox_add(lib->flip_guides, _("both"));
  g_object_set(G_OBJECT(lib->flip_guides), "tooltip-text", _("flip guides"), (char *)NULL);
  gtk_box_pack_start(GTK_BOX(self->widget), lib->flip_guides, TRUE, TRUE, 0);

  lib->golden_extras = dt_bauhaus_combobox_new(NULL);
  dt_bauhaus_widget_set_label(lib->golden_extras, _("extra"));
  dt_bauhaus_combobox_add(lib->golden_extras, _("golden sections"));
  dt_bauhaus_combobox_add(lib->golden_extras, _("golden spiral sections"));
  dt_bauhaus_combobox_add(lib->golden_extras, _("golden spiral"));
  dt_bauhaus_combobox_add(lib->golden_extras, _("all"));
  g_object_set(G_OBJECT(lib->golden_extras), "tooltip-text", _("show some extra guides"), (char *)NULL);
  gtk_box_pack_start(GTK_BOX(self->widget), lib->golden_extras, TRUE, TRUE, 0);

  gtk_widget_set_visible(GTK_WIDGET(lib->flip_guides), FALSE);
  gtk_widget_set_visible(GTK_WIDGET(lib->golden_extras), FALSE);

  gtk_widget_set_no_show_all(GTK_WIDGET(lib->flip_guides), TRUE);
  gtk_widget_set_no_show_all(GTK_WIDGET(lib->golden_extras), TRUE);

  // disable buttons that won't work with this camera
  // TODO: initialize tethering mode outside of libs/camera.s so we can use darktable.camctl->active_camera here
  const dt_camera_t *cam = darktable.camctl->active_camera;
  if(cam == NULL)
    cam = darktable.camctl->wanted_camera;
  if(cam != NULL && cam->can_live_view_advanced == FALSE)
  {
    gtk_widget_set_sensitive(lib->live_view_zoom, FALSE);
    gtk_widget_set_sensitive(lib->focus_in_big, FALSE);
    gtk_widget_set_sensitive(lib->focus_in_small, FALSE);
    gtk_widget_set_sensitive(lib->focus_out_big, FALSE);
    gtk_widget_set_sensitive(lib->focus_out_small, FALSE);
  }
}

void
gui_cleanup (dt_lib_module_t *self)
{
}

#define MARGIN  20
void
gui_post_expose(dt_lib_module_t *self, cairo_t *cr, int32_t width, int32_t height, int32_t pointerx, int32_t pointery)
{
  dt_camera_t *cam = (dt_camera_t*)darktable.camctl->active_camera;
  dt_lib_live_view_t *lib = self->data;

  if(cam->is_live_viewing == FALSE || cam->live_view_zoom == TRUE)
    return;

  dt_pthread_mutex_lock(&cam->live_view_pixbuf_mutex);
  if(GDK_IS_PIXBUF(cam->live_view_pixbuf))
  {
    gint pw = gdk_pixbuf_get_width(cam->live_view_pixbuf);
    gint ph = gdk_pixbuf_get_height(cam->live_view_pixbuf);
    if(cam->live_view_rotation%2 == 1)
    {
      gint tmp = pw; pw = ph; ph = tmp;
    }
    float w = width-(MARGIN*2.0f);
    float h = height-(MARGIN*2.0f);
    float scale = 1.0;
//     if(cam->live_view_zoom == FALSE)
//     {
      if(pw > w) scale = w/pw;
      if(ph > h) scale = MIN(scale, h/ph);
//     }
    float sw = scale*pw;
    float sh = scale*ph;

    // draw guides
    float left = (width - scale*pw)*0.5;
    float right = left + scale*pw;
    float top = (height - scale*ph)*0.5;
    float bottom = top + scale*ph;

    double dashes = 5.0;

    cairo_save(cr);
    cairo_set_dash(cr, &dashes, 1, 0);

    int guide_flip = dt_bauhaus_combobox_get(lib->flip_guides);
    int which = dt_bauhaus_combobox_get(lib->guide_selector);
    switch(which)
    {
      case GUIDE_GRID:
        dt_guides_draw_simple_grid(cr, left, top, right, bottom, 1.0);
        break;

      case GUIDE_DIAGONAL:
        dt_guides_draw_diagonal_method(cr, left, top, sw, sh);
        cairo_stroke (cr);
        cairo_set_dash (cr, &dashes, 0, 0);
        cairo_set_source_rgba(cr, .3, .3, .3, .8);
        dt_guides_draw_diagonal_method(cr, left, top, sw, sh);
        cairo_stroke (cr);
        break;
      case GUIDE_THIRD:
        dt_guides_draw_rules_of_thirds(cr, left, top,  right, bottom, sw/3.0, sh/3.0);
        cairo_stroke (cr);
        cairo_set_dash (cr, &dashes, 0, 0);
        cairo_set_source_rgba(cr, .3, .3, .3, .8);
        dt_guides_draw_rules_of_thirds(cr, left, top,  right, bottom, sw/3.0, sh/3.0);
        cairo_stroke (cr);
        break;
      case GUIDE_TRIANGL:
      {
        int dst = (int)((sh*cos(atan(sw/sh)) / (cos(atan(sh/sw)))));
        // Move coordinates to local center selection.
        cairo_translate(cr, ((right - left)/2+left), ((bottom - top)/2+top));

        // Flip horizontal.
        if (guide_flip & 1)
          cairo_scale(cr, -1, 1);
        // Flip vertical.
        if (guide_flip & 2)
          cairo_scale(cr, 1, -1);

        dt_guides_draw_harmonious_triangles(cr, left, top,  right, bottom, dst);
        cairo_stroke (cr);
        //p.setPen(QPen(d->guideColor, d->guideSize, Qt::DotLine));
        cairo_set_dash (cr, &dashes, 0, 0);
        cairo_set_source_rgba(cr, .3, .3, .3, .8);
        dt_guides_draw_harmonious_triangles(cr, left, top,  right, bottom, dst);
        cairo_stroke (cr);
      }
      break;
      case GUIDE_GOLDEN:
      {
        // Move coordinates to local center selection.
        cairo_translate(cr, ((right - left)/2+left), ((bottom - top)/2+top));

        // Flip horizontal.
        if (guide_flip & 1)
          cairo_scale(cr, -1, 1);
        // Flip vertical.
        if (guide_flip & 2)
          cairo_scale(cr, 1, -1);

        float w = sw;
        float h = sh;

        // lengths for the golden mean and half the sizes of the region:
        float w_g = w*INVPHI;
        float h_g = h*INVPHI;
        float w_2 = w/2;
        float h_2 = h/2;

        dt_QRect_t R1, R2, R3, R4, R5, R6, R7;
        dt_guides_q_rect (&R1, -w_2, -h_2, w_g, h);

        // w - 2*w_2 corrects for one-pixel difference
        // so that R2.right() is really at the right end of the region
        dt_guides_q_rect (&R2, w_g-w_2, h_2-h_g, w-w_g+1-(w - 2*w_2), h_g);
        dt_guides_q_rect (&R3, w_2 - R2.width*INVPHI, -h_2, R2.width*INVPHI, h - R2.height);
        dt_guides_q_rect (&R4, R2.left, R1.top, R3.left - R2.left, R3.height*INVPHI);
        dt_guides_q_rect (&R5, R4.left, R4.bottom, R4.width*INVPHI, R3.height - R4.height);
        dt_guides_q_rect (&R6, R5.left + R5.width, R5.bottom - R5.height*INVPHI, R3.left - R5.right, R5.height*INVPHI);
        dt_guides_q_rect (&R7, R6.right - R6.width*INVPHI, R4.bottom, R6.width*INVPHI, R5.height - R6.height);

        const int extras = dt_bauhaus_combobox_get(lib->golden_extras);
        dt_guides_draw_golden_mean(cr, &R1, &R2, &R3, &R4, &R5, &R6, &R7,
                                   extras == 0 || extras == 3,
                                   0,
                                   extras == 1 || extras == 3,
                                   extras == 2 || extras == 3);
        cairo_stroke (cr);

        cairo_set_dash (cr, &dashes, 0, 0);
        cairo_set_source_rgba(cr, .3, .3, .3, .8);
        dt_guides_draw_golden_mean(cr, &R1, &R2, &R3, &R4, &R5, &R6, &R7,
                                   extras == 0 || extras == 3,
                                   0,
                                   extras == 1 || extras == 3,
                                   extras == 2 || extras == 3);
        cairo_stroke (cr);
      }
      break;
    }
    cairo_restore(cr);
  }
  dt_pthread_mutex_unlock(&cam->live_view_pixbuf_mutex);
}

// modelines: These editor modelines have been set for all relevant files by tools/update_modelines.sh
// vim: shiftwidth=2 expandtab tabstop=2 cindent
// kate: tab-indents: off; indent-width 2; replace-tabs on; indent-mode cstyle; remove-trailing-space on;