/*
 * main.c - Command line interface, parameter loading and saving, and other such glue
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

#include <gtk/gtk.h>
#include <stdlib.h>
#include <time.h>
#include <getopt.h>
#include "de-jong.h"

static void usage(char **argv);
static void render_main(const char *filename);
static gchar* describe_color(GdkColor *c);
static void set_size_from_string(const char *s);

struct computation_params params;
struct render_params render;


int main(int argc, char ** argv) {
  enum {INTERACTIVE, RENDER} mode = INTERACTIVE;
  const char *outputFile;
  int c, option_index=0;

  srand(time(NULL));
  g_type_init();
  set_defaults();

  while (1) {
    static struct option long_options[] = {
      {"help",        0, NULL, 'h'},
      {"read",        1, NULL, 'i'},
      {"output",      1, NULL, 'o'},
      {"zoom",        1, NULL, 'z'},
      {"rotation",    1, NULL, 'r'},
      {"blur-radius", 1, NULL, 1000},
      {"blur-ratio",  1, NULL, 1001},
      {"exposure",    1, NULL, 'e'},
      {"gamma",       1, NULL, 'g'},
      {"foreground",  1, NULL, 1002},
      {"background",  1, NULL, 1003},
      {"size",        1, NULL, 's'},
      {"density",     1, NULL, 'd'},
      {"clamped",     0, NULL, 1004},
      {"oversample",  1, NULL, 1005},
      {"tileable",    0, NULL, 1006},
      {"fg-alpha",    1, NULL, 1007},
      {"bg-alpha",    1, NULL, 1008},
      NULL,
    };
    c = getopt_long(argc, argv, "hi:o:a:b:c:d:x:y:z:r:e:g:s:t:",
		    long_options, &option_index);
    if (c == -1)
      break;

    switch (c) {

    case 'i':  load_parameters_from_file(optarg);       break;
    case 'o':  mode = RENDER; outputFile = optarg;      break;
    case 'a':  set_parameter("a",              optarg); break;
    case 'b':  set_parameter("b",              optarg); break;
    case 'c':  set_parameter("c",              optarg); break;
    case 'd':  set_parameter("d",              optarg); break;
    case 'x':  set_parameter("xoffset",        optarg); break;
    case 'y':  set_parameter("yoffset",        optarg); break;
    case 'z':  set_parameter("zoom",           optarg); break;
    case 'r':  set_parameter("rotation",       optarg); break;
    case 'e':  set_parameter("exposure",       optarg); break;
    case 'g':  set_parameter("gamma",          optarg); break;
    case 's':  set_parameter("size" ,          optarg); break;
    case 't':  set_parameter("target_density", optarg); break;
    case 1000: set_parameter("blur_radius",    optarg); break;
    case 1001: set_parameter("blur_ratio",     optarg); break;
    case 1002: set_parameter("fgcolor",        optarg); break;
    case 1003: set_parameter("bgcolor",        optarg); break;
    case 1004: set_parameter("clamped",        "1");    break;
    case 1005: set_parameter("oversample",     optarg); break;
    case 1006: set_parameter("tileable",       "1");    break;
    case 1007: set_parameter("fgalpha",        optarg); break;
    case 1008: set_parameter("bgalpha",        optarg); break;

    case 'h':
    default:
      usage(argv);
      return 1;
    }
  }

  if (optind < argc) {
    usage(argv);
    return 1;
  }

  resize(render.width, render.height, render.oversample);

  switch (mode) {

  case INTERACTIVE:
    interactive_main(argc, argv);
    break;

  case RENDER:
    render_main(outputFile);
    break;
  }

  return 0;
}

static void usage(char **argv) {
  gchar *fg = describe_color(&render.fgcolor);
  gchar *bg = describe_color(&render.bgcolor);

  printf("Usage: %s [options]\n"
	 "Interactive exploration of the Peter de Jong attractor\n"
	 "\n"
	 "Actions:\n"
	 "  -i, --read FILE       Load all parameters from the tEXt chunk of any\n"
	 "                          .png image file generated by this program.\n"
	 "  -o, --output FILE     Instead of presenting an interactive GUI, render\n"
	 "                          an image with the provided settings and write it\n"
	 "                          in PNG format to FILE.\n"
	 "\n"
	 "Parameters:\n"
	 "  -a VALUE              Set the 'a' parameter [%f]\n"
	 "  -b VALUE              Set the 'b' parameter [%f]\n"
	 "  -c VALUE              Set the 'c' parameter [%f]\n"
	 "  -d VALUE              Set the 'd' parameter [%f]\n"
	 "  -x OFFSET             Set the X offest [%f]\n"
	 "  -y OFFSET             Set the Y offset [%f]\n"
	 "  -z, --zoom ZOOM       Set the zoom factor [%f]\n"
         "  -r, --rotation RADS   Set the rotation, in radians [%f]\n"
	 "  --blur-radius RADIUS  Set the blur radius [%f]\n"
	 "  --blur-ratio RATIO    Set the blur ratio [%f]\n"
	 "  --tileable            Generate a tileable image by wrapping at the edges\n"
	 "\n"
	 "Rendering:\n"
	 "  -e, --exposure EXP    Set the image exposure [%f]\n"
	 "  -g, --gamma GAMMA     Set the image gamma correction [%f]\n"
	 "  --foreground COLOR    Set the foreground color, specified as a color name\n"
	 "                          or in #RRGGBB hexadecimal format [%s]\n"
	 "  --background COLOR    Set the background color, specified as a color name\n"
	 "                          or in #RRGGBB hexadecimal format [%s]\n"
	 "  --fg-alpha ALPHA      Set the foreground alpha, between 0 (transparent)\n"
	 "                          and 65535 (completely opaque)\n"
	 "  --bg-alpha ALPHA      Set the background alpha, between 0 (transparent)\n"
	 "                          and 65535 (completely opaque)\n"
	 "  --clamped             Clamp the image to the foreground color, rather than\n"
	 "                          allowing more intense pixels to have other values\n"
	 "\n"
	 "Quality:\n"
	 "  -s, --size X[xY]      Set the image size in pixels. If only one value is\n"
	 "                          given, a square image is produced [%d]\n"
	 "  --oversample SCALE    Calculate the image at some integer multiple of the\n"
	 "                          output resolution, downsampling when generating the\n"
	 "                          final image. This improves the quality of sharp\n"
	 "                          edges on most images, but will increase memory usage\n"
	 "                          quadratically. Recommended values are between 1\n"
	 "                          (no oversampling) and 4 (heavy oversampling) [%d]\n"
	 "  -t, --density DENSITY In noninteractive rendering, set the peak density\n"
	 "                          to stop rendering at. Larger numbers give smoother\n"
	 "                          and more detailed results, but increase running time\n"
	 "                          linearly [%d]\n",
	 argv[0],
	 params.a, params.b, params.c, params.d, params.xoffset, params.yoffset, params.zoom,
	 params.rotation, params.blur_radius, params.blur_ratio,
	 render.exposure, render.gamma, fg, bg,
	 render.width, render.oversample, render.target_density);

  g_free(fg);
  g_free(bg);
}

static void render_main(const char *filename) {
  /* Main function for noninteractive rendering. This renders an image with the
   * current settings until render.current_density reaches target_density. We show helpful
   * progress doodads on stdout while the poor user has to wait.
   */
  time_t start_time, now, elapsed, remaining;
  start_time = time(NULL);

  while (render.current_density < render.target_density) {
    run_iterations(1000000);

    /* This should be a fairly accurate time estimate, since (asymptotically at least)
     * current_density increases linearly with the number of iterations performed.
     */
    now = time(NULL);
    elapsed = now - start_time;
    remaining = ((float)elapsed) * render.target_density / render.current_density - elapsed;

    /* After each batch of iterations, show the percent completion, number
     * of iterations (in scientific notation), iterations per second,
     * density / target density, and elapsed time / remaining time.
     */
    if (elapsed > 0) {
      printf("%6.02f%%   %.3e   %.2e/sec   %6d / %d   %02d:%02d:%02d / %02d:%02d:%02d\n",
	     100.0 * render.current_density / render.target_density,
	     render.iterations, render.iterations / elapsed,
	     render.current_density, render.target_density,
	     elapsed / (60*60), (elapsed / 60) % 60, elapsed % 60,
	     remaining / (60*60), (remaining / 60) % 60, remaining % 60);
    }
  }

  printf("Creating image...\n");
  save_to_file(filename);
}

void set_defaults() {
  params.a = 1.41914;
  params.b = -2.28413;
  params.c = 2.42754;
  params.d = -2.17719;
  params.zoom = 1;
  params.xoffset = 0;
  params.yoffset = 0;
  params.rotation = 0;
  params.blur_radius = 0;
  params.blur_ratio = 1;
  params.tileable = FALSE;

  render.exposure = 0.05;
  render.gamma = 1;
  render.clamped = FALSE;
  gdk_color_parse("white", &render.bgcolor);
  gdk_color_parse("black", &render.fgcolor);
  render.fgalpha = 0xFFFF;
  render.bgalpha = 0xFFFF;

  render.width = 600;
  render.height = 600;
  render.oversample = 1;
  render.target_density = 10000;
}

static gchar* describe_color(GdkColor *c) {
  /* Convert a GdkColor back to a gdk_color_parse compatible hex value.
   * Returns a freshly allocated buffer that should be freed.
   */
  return g_strdup_printf("#%02X%02X%02X", c->red >> 8, c->green >> 8, c->blue >> 8);
}

gchar* save_parameters() {
  /* Save the current parameters to a freshly allocated human and machine readable string */
  gchar *fg = describe_color(&render.fgcolor);
  gchar *bg = describe_color(&render.bgcolor);
  gchar *result;

  result = g_strdup_printf("a = %f\n"
			   "b = %f\n"
			   "c = %f\n"
			   "d = %f\n"
			   "zoom = %f\n"
			   "xoffset = %f\n"
			   "yoffset = %f\n"
			   "rotation = %f\n"
			   "blur_radius = %f\n"
			   "blur_ratio = %f\n"
			   "exposure = %f\n"
			   "gamma = %f\n"
			   "bgcolor = %s\n"
			   "fgcolor = %s\n"
			   "clamped = %d\n"
			   "tileable = %d\n"
			   "bgalpha = %d\n"
			   "fgalpha = %d\n",
			   params.a, params.b, params.c, params.d,
			   params.zoom, params.xoffset, params.yoffset, params.rotation,
			   params.blur_radius, params.blur_ratio,
			   render.exposure, render.gamma,
			   bg, fg, render.clamped, params.tileable,
			   render.bgalpha, render.fgalpha);

  g_free(fg);
  g_free(bg);
  return result;
}

static void set_size_from_string(const char *s) {
  /* Set the current width and height from a WIDTH or WIDTHxHEIGHT in the given string */
  char *cptr;
  render.width = strtol(optarg, &cptr, 10);
  if (*cptr == 'x')
    render.height = atoi(cptr+1);
  else
    render.height = render.width;
}

gboolean set_parameter(const char *key, const char *value) {
  /* Set a single parameter in key-value form, using the same key and value format
   * as save_parameters(). Returns TRUE if the key wasn't recognized.
   */

  if (!strcmp(key, "a"))
    params.a = atof(value);

  else if (!strcmp(key, "b"))
    params.b = atof(value);

  else if (!strcmp(key, "c"))
    params.c = atof(value);

  else if (!strcmp(key, "d"))
    params.d = atof(value);

  else if (!strcmp(key, "zoom"))
    params.zoom = atof(value);

  else if (!strcmp(key, "xoffset"))
    params.xoffset = atof(value);

  else if (!strcmp(key, "yoffset"))
    params.yoffset = atof(value);

  else if (!strcmp(key, "rotation"))
    params.rotation = atof(value);

  else if (!strcmp(key, "blur_radius"))
    params.blur_radius = atof(value);

  else if (!strcmp(key, "blur_ratio"))
    params.blur_ratio = atof(value);

  else if (!strcmp(key, "exposure"))
    render.exposure = atof(value);

  else if (!strcmp(key, "gamma"))
    render.gamma = atof(value);

  else if (!strcmp(key, "fgcolor"))
    gdk_color_parse(value, &render.fgcolor);

  else if (!strcmp(key, "bgcolor"))
    gdk_color_parse(value, &render.bgcolor);

  else if (!strcmp(key, "size"))
    set_size_from_string(value);

  else if (!strcmp(key, "target_density"))
    render.target_density = atol(value);

  else if (!strcmp(key, "clamped"))
    render.clamped = atol(value) != 0;

  else if (!strcmp(key, "tileable"))
    params.tileable = atol(value) != 0;

  else if (!strcmp(key, "oversample")) {
    render.oversample = atol(value);
    if (render.oversample < 1)
      render.oversample = 1;
  }

  else if (!strcmp(key, "fgalpha"))
    render.fgalpha = atol(value);

  else if (!strcmp(key, "bgalpha"))
    render.bgalpha = atol(value);

  else
    return TRUE;
  return FALSE;
}

void load_parameters(const gchar *paramstring) {
  /* Load all recognized parameters from a string given in the same
   * format as the one produced by save_parameters()
   */
  gchar *copy, *line, *nextline;
  gchar *key, *value;

  /* Make a copy of the parameters, since we'll be modifying it */
  copy = g_strdup(paramstring);

  /* Iterate over lines... */
  line = copy;
  while (line) {
    nextline = strchr(line, '\n');
    if (nextline) {
      *nextline = '\0';
      nextline++;
    }

    /* Separate it into key and value */
    key = g_malloc(strlen(line)+1);
    value = g_malloc(strlen(line)+1);
    if (sscanf(line, " %s = %s", key, value) == 2) {
      set_parameter(key, value);
    }
    g_free(key);
    line = nextline;
  }
  g_free(copy);
}

void load_parameters_from_file(const char *name) {
  /* Try to open the given PNG file and load parameters from it */
  const gchar *params;
  GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(name, NULL);
  params = gdk_pixbuf_get_option(pixbuf, "tEXt::de_jong_params");
  if (params)
    load_parameters(params);
  else
    printf("No parameters chunk found\n");
  gdk_pixbuf_unref(pixbuf);
}

void save_to_file(const char *name) {
  /* Save our current image to a .PNG file */
  gchar *params;

  /* Get a higher quality rendering */
  update_pixels();

  /* Save our current parameters in a tEXt chunk, using a format that
   * is both human-readable and easy to load parameters from automatically.
   */
  params = save_parameters();
  gdk_pixbuf_save(render.pixbuf, name, "png", NULL, "tEXt::de_jong_params", params, NULL);
  g_free(params);
}

/* The End */
