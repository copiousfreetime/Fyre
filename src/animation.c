/*
 * animation.c - A simple keyframe animation system for DeJong objects
 *
 * de Jong Explorer - interactive exploration of the Peter de Jong attractor
 * Copyright (C) 2004 David Trowbridge and Micah Dowty
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#include "animation.h"
#include "chunked-file.h"
#include "spline.h"

static void animation_class_init(AnimationClass *klass);
static void animation_init(Animation *self);
static void animation_dispose(GObject *gobject);
static void animation_keyframe_append_default(Animation *self, GtkTreeIter *iter);

/* Animations are serialized using chunked-file.
 * These are the chunk types and file signature
 */
#define FILE_SIGNATURE        "de Jong Explorer Animation\n\r\xFF\n"
#define CHUNK_KEYFRAME_START  CHUNK_TYPE('K','f','r','S')   /* Begin a new keyframe definition */
#define CHUNK_KEYFRAME_END    CHUNK_TYPE('K','f','r','E')   /* End a keyframe definition */
#define CHUNK_DE_JONG_PARAMS  CHUNK_TYPE('d','j','P','R')   /* Set de-jong parameters, represented as a string */
#define CHUNK_THUMBNAIL       CHUNK_TYPE('d','j','T','H')   /* Set a thumbnail, represented as a PNG image */
#define CHUNK_SPLINE          CHUNK_TYPE('s','p','l','C')   /* Spline control points */
#define CHUNK_DURATION        CHUNK_TYPE('d','u','r','a')   /* Transition duration, as a double */


/************************************************************************************/
/**************************************************** Initialization / Finalization */
/************************************************************************************/

GType animation_get_type(void) {
  static GType anim_type = 0;

  if (!anim_type) {
    static const GTypeInfo dj_info = {
      sizeof(AnimationClass),
      NULL, /* base_init */
      NULL, /* base_finalize */
      (GClassInitFunc) animation_class_init,
      NULL, /* class_finalize */
      NULL, /* class_data */
      sizeof(Animation),
      0,
      (GInstanceInitFunc) animation_init,
    };

    anim_type = g_type_register_static(G_TYPE_OBJECT, "Animation", &dj_info, 0);
  }

  return anim_type;
}

static void animation_class_init(AnimationClass *klass) {
  GObjectClass *object_class;
  object_class = (GObjectClass*) klass;

  object_class->dispose      = animation_dispose;
}

static void animation_init(Animation *self) {
  self->model = gtk_list_store_new(5,
				   GDK_TYPE_PIXBUF,     /* ANIMATION_MODEL_THUMBNAIL */
				   G_TYPE_STRING,       /* ANIMATION_MODEL_PARAMS    */
				   G_TYPE_DOUBLE,       /* ANIMATION_MODEL_DURATION  */
				   TYPE_SPLINE,         /* ANIMATION_MODEL_SPLINE    */
				   GTK_TYPE_TREE_ITER); /* ANIMATION_MODEL_ITER      */
}

static void animation_dispose(GObject *gobject) {
  Animation *self = ANIMATION(gobject);

  if (self->model) {
    g_object_unref(self->model);
    self->model = NULL;
  }
}

Animation* animation_new() {
  return ANIMATION(g_object_new(animation_get_type(), NULL));
}


/************************************************************************************/
/************************************************************ Keyframe Manipulation */
/************************************************************************************/

void animation_keyframe_store_dejong(Animation *self, GtkTreeIter *iter, DeJong *dejong) {
  /* Save de-jong parameters and a thumbnail to the keyframe at the given iterator
   */
  GdkPixbuf *thumbnail = de_jong_make_thumbnail(dejong, 128, 128);
  gchar *params = de_jong_save_string(dejong);

  gtk_list_store_set(self->model, iter,
		     ANIMATION_MODEL_THUMBNAIL, thumbnail,
		     ANIMATION_MODEL_PARAMS,    params,
		     -1);

  g_free(params);
  gdk_pixbuf_unref(thumbnail);
}

void animation_keyframe_load_dejong(Animation *self, GtkTreeIter *iter, DeJong *dejong) {
  /* Load de-jong parameters from the keyframe at the given iterator
   */
  gchar *params;

  gtk_tree_model_get(GTK_TREE_MODEL(self->model), iter,
		     ANIMATION_MODEL_PARAMS, &params,
		     -1);
  de_jong_load_string(dejong, params);
  g_free(params);
}

void animation_keyframe_append(Animation *self, DeJong *dejong) {
  GtkTreeIter iter;
  animation_keyframe_append_default(self, &iter);
  animation_keyframe_store_dejong(self, &iter, dejong);
}

static void animation_keyframe_append_default(Animation *self, GtkTreeIter *iter) {
  gtk_list_store_append(self->model, iter);
  gtk_list_store_set(self->model, iter,
		     ANIMATION_MODEL_ITER, iter,
		     ANIMATION_MODEL_DURATION, (gdouble) 5.0,
		     ANIMATION_MODEL_SPLINE,   &spline_template_smooth,
		     -1);
}

void animation_clear(Animation *self) {
  gtk_list_store_clear(self->model);
}

gdouble animation_keyframe_get_time(Animation *self, GtkTreeIter *iter) {
  /* Return the absolute time in seconds that the keyframe pointed to by 'iter' begins at.
   */
  GtkTreeModel *model = GTK_TREE_MODEL(self->model);
  GtkTreeIter my_iter;
  gboolean valid;
  gdouble keyframe_duration;
  gdouble total = 0;

  valid = gtk_tree_model_get_iter_first(model, &my_iter);
  while (valid) {

    if (!memcmp(&my_iter, iter, sizeof(GtkTreeIter)))
      break;

    gtk_tree_model_get(model, &my_iter,
		       ANIMATION_MODEL_DURATION, &keyframe_duration,
		       -1);
    total += keyframe_duration;

    valid = gtk_tree_model_iter_next(model, &my_iter);
  }

  return total;
}


/************************************************************************************/
/********************************************************************** Persistence */
/************************************************************************************/

void animation_load_file(Animation *self, const gchar *filename) {
  FILE *f;
  ChunkType type;
  gsize length;
  guchar* data;
  GtkTreeIter iter;
  GdkPixbufLoader *pixbuf_loader;
  gchar *tempstring;
  Spline *spline;

  g_return_if_fail(f = fopen(filename, "rb"));
  g_return_if_fail(chunked_file_read_signature(f, FILE_SIGNATURE));

  animation_clear(self);

  while (chunked_file_read_chunk(f, &type, &length, &data)) {
    switch (type) {

    case CHUNK_KEYFRAME_START:
      /* Start a new keyframe, point iter at it */
      animation_keyframe_append_default(self, &iter);
      break;

    case CHUNK_KEYFRAME_END:
      /* Ending a keyframe. We don't yet need this for anything */
      break;

    case CHUNK_DE_JONG_PARAMS:
      /* Set the de Jong parameters for this keyframe. Note that the
       * data in the file is not null terminated, hence the need to
       * copy it into a string we can null-terminate.
       */
      tempstring = g_malloc(length+1);
      tempstring[length] = '\0';
      memcpy(tempstring, data, length);
      gtk_list_store_set(self->model, &iter,
			 ANIMATION_MODEL_PARAMS, tempstring,
			 -1);
      g_free(tempstring);
      break;

    case CHUNK_THUMBNAIL:
      /* Set the thumbnail for this keyframe */
      pixbuf_loader = gdk_pixbuf_loader_new();
      gdk_pixbuf_loader_write(pixbuf_loader, data, length, NULL);
      gdk_pixbuf_loader_close(pixbuf_loader, NULL);
      gtk_list_store_set(self->model, &iter,
			 ANIMATION_MODEL_THUMBNAIL, gdk_pixbuf_loader_get_pixbuf(pixbuf_loader),
			 -1);
      g_object_unref(pixbuf_loader);
      break;

    case CHUNK_DURATION:
      /* The transition duration, as a double */
      if (length == sizeof(gdouble)) {
	gtk_list_store_set(self->model, &iter,
			   ANIMATION_MODEL_DURATION, *(gdouble*)data,
			   -1);
      }
      else {
	g_log(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING,
	      "Duration chunk is incorrectly sized, %d bytes instead of %d",
	      length, sizeof(gdouble));
      }
      break;

    case CHUNK_SPLINE:
      /* Spline control points */
      spline = spline_unserialize(data, length);
      gtk_list_store_set(self->model, &iter,
			 ANIMATION_MODEL_SPLINE, spline,
			 -1);
      spline_free(spline);
      break;

    default:
      chunked_file_warn_unknown_type(type);
    }
    g_free(data);
  }

  fclose(f);
}

void animation_save_file(Animation *self, const gchar *filename) {
  FILE *f;
  GtkTreeModel *model = GTK_TREE_MODEL(self->model);
  GtkTreeIter iter;
  gboolean valid;
  gchar *params;
  GdkPixbuf *thumb_pixbuf;
  gchar *buffer;
  gsize buffer_len;
  gdouble duration;
  Spline *spline;

  /* Start a new chunked file */
  g_return_if_fail(f = fopen(filename, "wb"));
  chunked_file_write_signature(f, FILE_SIGNATURE);

  /* Iterate over each keyframe in our model */
  valid = gtk_tree_model_get_iter_first(model, &iter);
  while (valid) {

    gtk_tree_model_get(model, &iter,
		       ANIMATION_MODEL_PARAMS,    &params,
		       ANIMATION_MODEL_THUMBNAIL, &thumb_pixbuf,
		       ANIMATION_MODEL_DURATION,  &duration,
		       ANIMATION_MODEL_SPLINE,    &spline,
		       -1);

    chunked_file_write_chunk(f, CHUNK_KEYFRAME_START, 0, NULL);

    if (params) {
      chunked_file_write_chunk(f, CHUNK_DE_JONG_PARAMS, strlen(params), params);
      g_free(params);
    }

    if (thumb_pixbuf) {
      if (gdk_pixbuf_save_to_buffer(thumb_pixbuf, &buffer, &buffer_len, "png", NULL, NULL)) {
	chunked_file_write_chunk(f, CHUNK_THUMBNAIL, buffer_len, buffer);
	g_free(buffer);
	gdk_pixbuf_unref(thumb_pixbuf);
      }
    }

    chunked_file_write_chunk(f, CHUNK_DURATION, sizeof(duration), (guchar*) &duration);

    if (spline) {
      buffer = spline_serialize(spline, &buffer_len);
      chunked_file_write_chunk(f, CHUNK_SPLINE, buffer_len, buffer);
      g_free(buffer);
      spline_free(spline);
    }

    chunked_file_write_chunk(f, CHUNK_KEYFRAME_END, 0, NULL);

    valid = gtk_tree_model_iter_next(model, &iter);
  }

  fclose(f);
}


/************************************************************************************/
/************************************************************** Animation Iterators */
/************************************************************************************/

gdouble animation_get_length(Animation *self) {
  /* Return the animation's total length in seconds. Currently this
   * requires iterating over the keyframes.
   */
  GtkTreeModel *model = GTK_TREE_MODEL(self->model);
  GtkTreeIter iter;
  gboolean valid;
  gdouble keyframe_duration;
  gdouble total = 0;

  valid = gtk_tree_model_get_iter_first(model, &iter);
  while (valid) {

    gtk_tree_model_get(model, &iter,
		       ANIMATION_MODEL_DURATION, &keyframe_duration,
		       -1);
    total += keyframe_duration;

    valid = gtk_tree_model_iter_next(model, &iter);
  }

  return total;
}

void animation_iter_get_first(Animation *self, AnimationIter *iter) {
  /* Initialize an iterator to the beginning of the animation
   */
  GtkTreeModel *model = GTK_TREE_MODEL(self->model);

  iter->valid = gtk_tree_model_get_iter_first(model, &iter->keyframe);
  iter->absolute_time = 0;
  iter->time_after_keyframe = 0;
}

void animation_iter_seek(Animation *self, AnimationIter *iter, gdouble absolute_time) {
  /* Initialize an iterator to an absolute time in seconds
   */
  animation_iter_get_first(self, iter);
  animation_iter_seek_relative(self, iter, absolute_time);
}

void animation_iter_seek_relative(Animation *self, AnimationIter *iter, gdouble delta_time) {
  /* Seek an iterator forward or backwards by a given number of seconds. This works
   * by first adding the delta_time to the time_after_keyframe, then moving the
   * current keyframe until time_after_keyframe is in an acceptable range.
   */
  GtkTreeModel *model = GTK_TREE_MODEL(self->model);
  gdouble keyframe_duration;

  iter->time_after_keyframe += delta_time;

  while (iter->valid) {
    gtk_tree_model_get(model, &iter->keyframe,
		       ANIMATION_MODEL_DURATION, &keyframe_duration,
		       -1);

    if (iter->time_after_keyframe >= keyframe_duration) {
      /* Skip to the next keyframe */
      iter->valid = gtk_tree_model_iter_next(model, &iter->keyframe);
      iter->time_after_keyframe -= keyframe_duration;
    }

    else if (iter->time_after_keyframe < 0) {
      /* Skip to the previous keyframe.
       * Unfortunately, there's no gtk_tree_model_iter_prev, so
       * the best way we can do this from here is to seek back to the beginning.
       */
      animation_iter_get_first(self, iter);
    }

    else
      break;
  }

}

void animation_iter_load_dejong(Animation *self, AnimationIter *iter, DeJong *dejong) {
  /* Load the dejong parameters corresponding to the given iterator into a DeJong object.
   * This finds the keyframes before and after the iterator and applies the proper type
   * of interpolation.
   */
  GtkTreeModel *model = GTK_TREE_MODEL(self->model);
  GtkTreeIter next_keyframe = iter->keyframe;
  gdouble keyframe_duration;
  DeJongPair pair;
  gdouble alpha;
  Spline *spline;

  g_return_if_fail(iter->valid);

  /* We should always be able to load the first keyframe */
  pair.a = de_jong_new();
  animation_keyframe_load_dejong(self, &iter->keyframe, pair.a);

  if (gtk_tree_model_iter_next(model, &next_keyframe)) {
    /* We have a next keyframe, load it */
    pair.b = de_jong_new();
    animation_keyframe_load_dejong(self, &next_keyframe, pair.b);
  }
  else {
    /* No next keyframe, use another copy of the first */
    pair.b = g_object_ref(pair.a);
  }

  gtk_tree_model_get(model, &iter->keyframe,
		     ANIMATION_MODEL_DURATION, &keyframe_duration,
		     ANIMATION_MODEL_SPLINE,   &spline,
		     -1);

  /* Alpha is 0 at the first keyframe and 1 at the second keyframe,
   * increasing linearly. It could be used as-is for linear interpolation.
   */
  alpha = iter->time_after_keyframe / keyframe_duration;

  /* We, however, will pass alpha through a spline to give the user
   * more control over the interpolation.
   */
  alpha = spline_solve_and_eval(spline, alpha);
  spline_free(spline);

  /* Only do linear interpolation for now */
  de_jong_interpolate_linear(dejong, alpha, &pair);

  g_object_unref(pair.a);
  g_object_unref(pair.b);
}

gboolean animation_iter_read_frame(Animation *self, AnimationIter *iter, DeJongPair *frame, double frame_rate) {
  /* Retrieve and step over one frame of the animation.
   * Sets frame->a to the beginning of this frame and frame->b to the end.
   * Returns TRUE if a frame was retrieved successfully, FALSE on end-of-animation.
   */
  if (!iter->valid)
    return FALSE;
  animation_iter_load_dejong(self, iter, frame->a);

  animation_iter_seek_relative(self, iter, 1/frame_rate);

  if (!iter->valid)
    return FALSE;
  animation_iter_load_dejong(self, iter, frame->b);

  return TRUE;
}

/* The End */
