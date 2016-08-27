/* Buzztrax
 * Copyright (C) 2006-2008 Buzztrax team <buzztrax-devel@buzztrax.org>
 *
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
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include "pt-waveslider.h"

enum
{
	PROP_0,
	PROP_PLAYBACK_CURSOR,
	N_PROPERTIES
};

#define MARKER_BOX_W 6
#define MARKER_BOX_H 5
#define MIN_W 24
#define MIN_H 16

#define DEF_PEAK_SIZE 1000

//-- the class

G_DEFINE_TYPE (PtWaveslider, pt_waveslider, GTK_TYPE_WIDGET);


static void
pt_waveslider_realize (GtkWidget *widget)
{
	PtWaveslider *self = PT_WAVESLIDER (widget);
	GdkWindow *window;
	GtkAllocation allocation;
	GdkWindowAttr attributes;
	gint attributes_mask;

	gtk_widget_set_realized (widget, TRUE);

	window = gtk_widget_get_parent_window (widget);
	gtk_widget_set_window (widget, window);
	g_object_ref (window);

	gtk_widget_get_allocation (widget, &allocation);

	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.x = allocation.x;
	attributes.y = allocation.y;
	attributes.width = allocation.width;
	attributes.height = allocation.height;
	attributes.wclass = GDK_INPUT_ONLY;
	attributes.event_mask = gtk_widget_get_events (widget);
	attributes.event_mask |= (GDK_EXPOSURE_MASK |
		GDK_BUTTON_PRESS_MASK |
		GDK_BUTTON_RELEASE_MASK |
		GDK_BUTTON_MOTION_MASK |
		GDK_ENTER_NOTIFY_MASK |
		GDK_LEAVE_NOTIFY_MASK |
		GDK_POINTER_MOTION_MASK | GDK_POINTER_MOTION_HINT_MASK);
	attributes_mask = GDK_WA_X | GDK_WA_Y;

	self->window = gdk_window_new (window, &attributes, attributes_mask);
	gtk_widget_register_window (widget, self->window);
}

static void
pt_waveslider_unrealize (GtkWidget *widget)
{
	PtWaveslider *self = PT_WAVESLIDER (widget);

	gtk_widget_unregister_window (widget, self->window);
	gdk_window_destroy (self->window);
	self->window = NULL;
	GTK_WIDGET_CLASS (pt_waveslider_parent_class)->unrealize (widget);
}

static void
pt_waveslider_map (GtkWidget *widget)
{
	PtWaveslider *self = PT_WAVESLIDER (widget);

	gdk_window_show (self->window);

	GTK_WIDGET_CLASS (pt_waveslider_parent_class)->map (widget);
}

static void
pt_waveslider_unmap (GtkWidget *widget)
{
	PtWaveslider *self = PT_WAVESLIDER (widget);

	gdk_window_hide (self->window);

	GTK_WIDGET_CLASS (pt_waveslider_parent_class)->unmap (widget);
}

static gboolean
pt_waveslider_draw (GtkWidget *widget,
			 cairo_t   *cr)
{
	PtWaveslider *self = PT_WAVESLIDER (widget);
	GtkStyleContext *style_ctx;
	gint width, height, left, top, middle, half;
	gint i, x;
	gfloat *peaks = self->peaks;
	gdouble min, max;
	gint offset;

	width = gtk_widget_get_allocated_width (widget);
	height = gtk_widget_get_allocated_height (widget);
	style_ctx = gtk_widget_get_style_context (widget);

	/* draw border */
	gtk_render_background (style_ctx, cr, 0, 0, width, height);
	gtk_render_frame (style_ctx, cr, 0, 0, width, height);

	if (!peaks) {
		g_debug ("draw, no peaks");
		return FALSE;
	}

	left = self->border.left;
	top = self->border.top;
	width -= self->border.left + self->border.right;
	height -= self->border.top + self->border.bottom;
	middle = top + height / 2;
	half = height / 2 - 1;

	cairo_set_line_join (cr, CAIRO_LINE_JOIN_ROUND);
	cairo_set_line_width (cr, 1.0);

	GdkRGBA wave_color, peak_color, line_color;
	gtk_style_context_lookup_color (style_ctx, "wave_color", &wave_color);
	gtk_style_context_lookup_color (style_ctx, "peak_color", &peak_color);
	gtk_style_context_lookup_color (style_ctx, "line_color", &line_color);

	offset = self->playback_cursor * 2 - width;

	/* before waveform */
	gdk_cairo_set_source_rgba (cr, &peak_color);
	if (offset < 0) {
		cairo_rectangle (cr, left, top, offset /2 * -1, height);
		cairo_fill (cr);
	}

	/* beyond waveform */
	gint diff = offset + width * 2 - self->peaks_size;
	if (diff > 0) {
		cairo_rectangle (cr, left + width - diff / 2, top, diff / 2, height);
		cairo_fill (cr);
	}

	/* waveform */
	for (i = 0; i < 2 * width; i += 2) {
		if (offset + i < 0)
			continue;
		if (offset + i > self->peaks_size)
			break;
		gint x = left + i / 2;
		min = (middle + half * peaks[offset + i + 1] * -1);
		max = (middle - half * peaks[offset + i + 2]);
		cairo_move_to (cr, x, min);
		cairo_line_to (cr, x, max);
	}

	gdk_cairo_set_source_rgba (cr, &wave_color);
	cairo_stroke (cr);

	/* cursor */
	if (self->playback_cursor != -1) {
		gdk_cairo_set_source_rgba (cr, &line_color);
		x = (gint) (left + width / 2) - 1;
		cairo_move_to (cr, x, top + height);
		cairo_line_to (cr, x, top);
		cairo_stroke (cr);
		cairo_move_to (cr, x, top + height / 2 - MARKER_BOX_H);
		cairo_line_to (cr, x, top + height / 2 + MARKER_BOX_H);
		cairo_line_to (cr, x + MARKER_BOX_W, top + height / 2);
		cairo_line_to (cr, x, top + height / 2 - MARKER_BOX_H);
		cairo_fill (cr);
	}

	return FALSE;
}

static void
pt_waveslider_size_allocate (GtkWidget	*widget,
				  GtkAllocation *allocation)
{
	PtWaveslider *self = PT_WAVESLIDER (widget);
	GtkStyleContext *context;
	GtkStateFlags state;

	gtk_widget_set_allocation (widget, allocation);

	if (gtk_widget_get_realized (widget))
		gdk_window_move_resize (self->window,
			allocation->x, allocation->y, allocation->width, allocation->height);

	context = gtk_widget_get_style_context (widget);
	state = gtk_widget_get_state_flags (widget);
	gtk_style_context_get_border (context, state, &self->border);
}

static void
pt_waveslider_get_preferred_width (GtkWidget *widget,
					gint	  *minimal_width,
					gint	  *natural_width)
{
	PtWaveslider *self = PT_WAVESLIDER (widget);
	gint border_padding = self->border.left + self->border.right;

	*minimal_width = MIN_W + border_padding;
	*natural_width = (MIN_W * 6) + border_padding;
}

static void
pt_waveslider_get_preferred_height (GtkWidget *widget,
					 gint	   *minimal_height,
					 gint	   *natural_height)
{
	PtWaveslider *self = PT_WAVESLIDER (widget);
	gint border_padding = self->border.top + self->border.bottom;

	*minimal_height = MIN_H + border_padding;
	*natural_height = (MIN_H * 4) + border_padding;
}

static gboolean
pt_waveslider_button_press (GtkWidget	*widget,
				 GdkEventButton *event)
{
	PtWaveslider *self = PT_WAVESLIDER (widget);

	const gint width = gtk_widget_get_allocated_width (widget) - 2;

	gint64 left;	/* the first sample in the view */
	gint64 clicked;	/* the sample clicked on */
	gint64 pos;	/* clicked sample's position in milliseconds */

	left = self->playback_cursor - width * 0.5;
	clicked = left + (gint) event->x;
	pos = clicked * 10;

	if (clicked < 0 || clicked > self->peaks_size / 2) {
		g_debug ("click outside");
		return FALSE;
	}

	/* Single left click */
	if (event->type == GDK_BUTTON_PRESS && event->button == GDK_BUTTON_PRIMARY) {
		g_signal_emit_by_name (widget, "cursor-changed", pos);
		g_debug ("click, jump to: %d ms", pos);
		return TRUE;
	}

	return FALSE;
}

static gboolean
pt_waveslider_button_release (GtkWidget	  *widget,
				   GdkEventButton *event)
{
	PtWaveslider *self = PT_WAVESLIDER (widget);

	return FALSE;
}

static gboolean
pt_waveslider_motion_notify (GtkWidget	 *widget,
				  GdkEventMotion *event)
{
	PtWaveslider *self = PT_WAVESLIDER (widget);
	const gint ox = 1;
	const gint sx = gtk_widget_get_allocated_width (widget) - 2;
	gint64 pos = (event->x - ox) * (gdouble) self->wave_length / sx;

	pos = CLAMP (pos, 0, self->wave_length);

	return FALSE;
}

static void
pt_waveslider_finalize (GObject *object)
{
	PtWaveslider *self = PT_WAVESLIDER (object);

	g_free (self->peaks);

	G_OBJECT_CLASS (pt_waveslider_parent_class)->finalize (object);
}

static void
pt_waveslider_get_property (GObject    *object,
				 guint	     property_id,
				 GValue     *value,
				 GParamSpec *pspec)
{
	PtWaveslider *self = PT_WAVESLIDER (object);

	switch (property_id) {
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
  }
}

static void
pt_waveslider_set_property (GObject      *object,
				 guint	       property_id,
				 const GValue *value,
				 GParamSpec   *pspec)
{
	PtWaveslider *self = PT_WAVESLIDER (object);

	switch (property_id) {
	case PROP_PLAYBACK_CURSOR:
		self->playback_cursor = g_value_get_int64 (value) / 441;
		if (gtk_widget_get_realized (GTK_WIDGET (self))) {
			gtk_widget_queue_draw (GTK_WIDGET (self));
		}
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
pt_waveslider_class_init (PtWavesliderClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	widget_class->realize = pt_waveslider_realize;
	widget_class->unrealize = pt_waveslider_unrealize;
	widget_class->map = pt_waveslider_map;
	widget_class->unmap = pt_waveslider_unmap;
	widget_class->draw = pt_waveslider_draw;
	widget_class->get_preferred_width = pt_waveslider_get_preferred_width;
	widget_class->get_preferred_height = pt_waveslider_get_preferred_height;
	widget_class->size_allocate = pt_waveslider_size_allocate;
	widget_class->button_press_event = pt_waveslider_button_press;
	widget_class->button_release_event = pt_waveslider_button_release;
	widget_class->motion_notify_event = pt_waveslider_motion_notify;

	gobject_class->set_property = pt_waveslider_set_property;
	gobject_class->get_property = pt_waveslider_get_property;
	gobject_class->finalize = pt_waveslider_finalize;

	/**
	* PtWaveslider::cursor-changed:
	* @ws: the waveslider emitting the signal
	* @position: the position in stream in milliseconds
	*
	* Some description.
	*/
	g_signal_new ("cursor-changed",
		      G_TYPE_OBJECT,
		      G_SIGNAL_RUN_FIRST,
		      0,
		      NULL,
		      NULL,
		      g_cclosure_marshal_VOID__INT,
		      G_TYPE_NONE,
		      1, G_TYPE_INT64);

	g_object_class_install_property (gobject_class, PROP_PLAYBACK_CURSOR,
		g_param_spec_int64 ("playback-cursor",
			"playback cursor position",
			"Current playback position within a waveform or -1 if sample is not played",
			-1, G_MAXINT64, -1, G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS));
}

static void
pt_waveslider_init (PtWaveslider *self)
{
	GtkStyleContext *context;
	GtkCssProvider  *provider;
	GtkSettings     *settings;
	gboolean	 dark;
	GFile		*file;

	self->channels = 2;
	self->peaks_size = DEF_PEAK_SIZE;
	self->peaks = g_malloc (sizeof (gfloat) * self->channels * self->peaks_size);
	self->wave_length = 0;
	self->playback_cursor = -1;

	gtk_widget_set_has_window (GTK_WIDGET (self), FALSE);

	settings = gtk_settings_get_default ();
	g_object_get (settings, "gtk-application-prefer-dark-theme", &dark, NULL);

	if (dark)
		file = g_file_new_for_uri ("resource:///org/gnome/parlatype/pt-waveslider-dark.css");
	else
		file = g_file_new_for_uri ("resource:///org/gnome/parlatype/pt-waveslider.css");

	provider = gtk_css_provider_new ();
	gtk_css_provider_load_from_file (provider, file, NULL);
	g_object_unref (file);

	context = gtk_widget_get_style_context (GTK_WIDGET (self));
	gtk_style_context_add_class (context, GTK_STYLE_CLASS_FRAME);
	gtk_style_context_add_class (context, GTK_STYLE_CLASS_VIEW);
	gtk_style_context_add_provider (context,
					GTK_STYLE_PROVIDER (provider),
					GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	/* Remove and unref provider? Where? */
}

/**
 * pt_waveslider_set_wave:
 * @self: the widget
 * @data: memory block of samples (interleaved for channels>1)
 * @channels: number channels
 * @length: number samples per channel
 *
 * Set wave data to show in the widget.
 */
void
pt_waveslider_set_wave (PtWaveslider *self,
			     gint16	      *data,
			     gint	       channels,
			     gint	       length)
{
	/* Create 100 data pairs (minimum and maximum value) per second
	   from raw data. Input must be mono, at a bit rate of 44100.
	   Move this later to the waveloader. */

	gint i, p;
	gint64 len = length; /* number of samples */
	gint rate = 44100;   /* for reference */

	self->wave_length = length;

	g_free (self->peaks);
	self->peaks = NULL;

	if (!data || !length) {
		gtk_widget_queue_draw (GTK_WIDGET (self));
		return;
	}

	/* calculate peak data */
	self->peaks_size = length / 441 * 2;
	self->peaks = g_malloc (sizeof (gfloat) * self->peaks_size - 2);

	for (i = 0; i < self->peaks_size -2 ; i += 2) {
		gint p1 = len * i / self->peaks_size;
		gint p2 = len * (i + 1) / self->peaks_size;

		/* get min max for peak slot */
		gfloat vmin = data[p1 + 1], vmax = data[p1 + 1];
		for (p = p1 + 1; p < p2; p++) {
			gfloat d = data[p + 1];
			if (d < vmin)
				vmin = d;
			if (d > vmax)
				vmax = d;
		}
		if (vmin > 0 && vmax > 0)
			vmin = 0;
		else if (vmin < 0 && vmax < 0)
			vmax = 0;
		self->peaks[i + 1] = vmin / 32768.0;
		self->peaks[i + 2] = vmax / 32768.0;
	}

	gtk_widget_queue_draw (GTK_WIDGET (self));
}

/**
 * pt_waveslider_new:
 *
 * Create a new waveform viewer widget. Use pt_waveslider_set_wave() to
 * pass wave data.
 *
 * Returns: the widget
 */
GtkWidget *
pt_waveslider_new (void)
{
	return GTK_WIDGET (g_object_new (PT_TYPE_WAVESLIDER, NULL));
}
