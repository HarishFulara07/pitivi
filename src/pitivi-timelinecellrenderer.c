/* 
 * PiTiVi
 * Copyright (C) <2004>	 Guillaume Casanova <casano_g@epita.fr>
 *
 * This software has been written in EPITECH <http://www.epitech.net>
 * EPITECH is a computer science school in Paris - FRANCE -
 * under the direction of Flavien Astraud and Jerome Landrieu.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "pitivi.h"
#include "pitivi-timelinewindow.h"
#include "pitivi-timelinecellrenderer.h"
#include "pitivi-timelinemedia.h"
#include "pitivi-dragdrop.h"
#include "pitivi-toolboxwindow.h"
#include "pitivi-toolbox.h"
#include "pitivi-drawing.h"

// Parent Class
static GtkLayoutClass	    *parent_class = NULL;

// Caching Operation  
static GdkPixmap	    *pixmap = NULL;

// default Dashes
static char gdefault_dash [2] = {5, 4};



struct _PitiviTimelineCellRendererPrivate
{
  /* instance private members */
  gboolean	       dispose_has_run;
  
  PitiviTimelineWindow *timewin;
  PitiviTimelineMedia  *draggedWidget;
  GtkSelectionData     *current_selection;
  gboolean	       selected;
  GdkRectangle	       selected_area;
    
  gint		       width;
  gint		       height;
};


/*
 * forward definitions
 */

void  pitivi_timelinecellrenderer_deselection_ontracks (GtkWidget *widget, gboolean self_deselected);
 
// Properties Enumaration

typedef enum {
  
  PITIVI_TML_LAYER_PROPERTY = 1,
  PITIVI_TML_TYPE_LAYER_PROPERTY,
  PITIVI_TML_TRACK_NB_PROPERTY,  
} PitiviLayerProperty;


// Destination Acception mime type for drah 'n drop

static GtkTargetEntry TargetEntries[] =
  {
    { "pitivi/sourcefile", GTK_TARGET_SAME_APP, DND_TARGET_SOURCEFILEWIN },
    { "pitivi/sourceeffect", GTK_TARGET_SAME_APP, DND_TARGET_EFFECTSWIN },
    { "pitivi/sourcetimeline", GTK_TARGET_SAME_APP, DND_TARGET_TIMELINEWIN },
    { "STRING", GTK_TARGET_SAME_APP, DND_TARGET_STRING },
    { "text/plain", GTK_TARGET_SAME_APP, DND_TARGET_URI },
  };

static gint iNbTargetEntries = G_N_ELEMENTS (TargetEntries);

/*
 * Insert "added-value" functions here
 */

GtkWidget *
pitivi_timelinecellrenderer_new (guint track_nb, PitiviLayerType layertype)
{
  PitiviTimelineCellRenderer	*timelinecellrenderer;

  timelinecellrenderer = (PitiviTimelineCellRenderer *) g_object_new (PITIVI_TIMELINECELLRENDERER_TYPE, 
								      "track_nb", 
								      track_nb, 
								      "type", 
								      layertype, 
								      NULL);
  timelinecellrenderer->track_nb = track_nb;
  timelinecellrenderer->track_type = layertype;
  g_assert(timelinecellrenderer != NULL);
  return GTK_WIDGET ( timelinecellrenderer );
}

static GObject *
pitivi_timelinecellrenderer_constructor (GType type,
			     guint n_construct_properties,
			     GObjectConstructParam * construct_properties)
{
  GObject *obj;
  {
    /* Invoke parent constructor. */
    PitiviTimelineCellRendererClass *klass;
    GObjectClass *parent_class;
    klass = PITIVI_TIMELINECELLRENDERER_CLASS (g_type_class_peek (PITIVI_TIMELINECELLRENDERER_TYPE));
    parent_class = G_OBJECT_CLASS (g_type_class_peek_parent (klass));
    obj = parent_class->constructor (type, n_construct_properties,
				     construct_properties);
  }

  /* do stuff. */

  return obj;

}

void 
draw_selection_area (GtkWidget *widget, GdkRectangle *area, int width, char **dash)
{
  GdkGC *style = gdk_gc_new ( widget->window );
  GdkWindow *window;
  
  if (width == 0)
    width = DEFAULT_WIDTH_DASHES;
  if (dash == NULL)
    gdk_gc_set_dashes ( style, 0, (gint8*)gdefault_dash, sizeof (gdefault_dash) / 2);
  else
    gdk_gc_set_dashes ( style, 0, (gint8*)dash, sizeof (dash) / 2); 
  gdk_gc_set_line_attributes ( style, width, GDK_LINE_ON_OFF_DASH, GDK_CAP_BUTT, GDK_JOIN_MITER);
  if (GTK_IS_LAYOUT (widget))
    window = GDK_WINDOW (GTK_LAYOUT (widget)->bin_window);
  else
    window = GDK_WINDOW (widget->window);
  gdk_draw_rectangle ( window, style, 
		       FALSE, 
		       area->x, area->y,
		       area->width, 
		       area->height);
}

void 
draw_selection (GtkWidget *widget, int width, char **dash)
{
  GdkGC *style = gdk_gc_new ( widget->window );
  GdkWindow *window;
  
  if (width == 0)
    width = DEFAULT_WIDTH_DASHES;
  if (dash == NULL)
    gdk_gc_set_dashes ( style, 0, (gint8*)gdefault_dash, sizeof (gdefault_dash) / 2);
  else
    gdk_gc_set_dashes ( style, 0, (gint8*)dash, sizeof (dash) / 2); 
  gdk_gc_set_line_attributes ( style, width, GDK_LINE_ON_OFF_DASH, GDK_CAP_BUTT, GDK_JOIN_MITER);
  if (GTK_IS_LAYOUT (widget))
    window = GDK_WINDOW (GTK_LAYOUT (widget)->bin_window);
  else
    window = GDK_WINDOW (widget->window);
  gdk_draw_rectangle ( window, style, 
		       FALSE, 
		       widget->allocation.x, 0, 
		       widget->allocation.width, widget->allocation.height);
}

static gint
pitivi_timelinecellrenderer_expose (GtkWidget      *widget,
				    GdkEventExpose *event)
{
  PitiviTimelineCellRenderer	*self = PITIVI_TIMELINECELLRENDERER (widget);
  GtkLayout *layout;
  
  g_return_val_if_fail (GTK_IS_LAYOUT (widget), FALSE);
  layout = GTK_LAYOUT (widget);
  gtk_paint_hline (widget->style,
		   layout->bin_window, 
		   GTK_STATE_NORMAL,
		   NULL, widget, "middle-line",
		   0, widget->allocation.width, widget->allocation.height/2);
  
  if (self->private->selected)
    draw_selection_area (widget, &self->private->selected_area, 0, NULL);
  
  if (event->window != layout->bin_window)
    return FALSE;
  
  (* GTK_WIDGET_CLASS (parent_class)->expose_event) (widget, event);
  
  return FALSE;
}


int add_to_layout (GtkWidget *self, GtkWidget *widget, gint x, gint y)
{
  GtkRequisition req;
  PitiviTimelineCellRenderer *cell;  
  cell = PITIVI_TIMELINECELLRENDERER (self);
  gtk_widget_size_request (widget, &req);
  gtk_layout_put (GTK_LAYOUT (self), widget, x, 0);
  x += req.width;
  gtk_layout_set_size (GTK_LAYOUT (self), x, 0);
  
  if (cell->children != NULL)
    {
      g_list_free (cell->children);
      cell->children = NULL;
    }
  
  cell->children = gtk_container_get_children (GTK_CONTAINER (self));
  return TRUE;
}

PitiviLayerType
check_media_type (PitiviSourceFile *sf)
{
  gchar *media;
  
  media = g_strdup (sf->mediatype);
  if (!g_strcasecmp  (sf->mediatype, "video"))
    return (PITIVI_VIDEO_TRACK);
  else if (!g_strcasecmp (sf->mediatype, "audio"))
    return (PITIVI_AUDIO_TRACK);
  else
    return (PITIVI_VIDEO_AUDIO_TRACK);
  return (PITIVI_NO_TRACK);
}


static void 
pitivi_timelinecellrenderer_drag_on_same_widget (PitiviTimelineCellRenderer *self, 
						 GtkSelectionData *selection, 
						 int x,
						 int y)
{
  PitiviTimelineMedia  *current_media;
  PitiviTimelineMedia  *draggedWidget;
  PitiviCursor	       *cursor;
  PitiviLayerType      dragged_type_track;
  GtkWidget	       *current_wmed;
  GtkContainer	       *container;
  
  draggedWidget = (PitiviTimelineMedia *) selection->data;
  current_media = pitivi_timelinemedia_new (draggedWidget->sf);
  current_wmed = GTK_WIDGET (current_media);
  current_wmed->allocation.width = GTK_WIDGET (draggedWidget)->allocation.width;
  
  gtk_widget_set_size_request (current_wmed, 
			       GTK_WIDGET (draggedWidget)->allocation.width, 
			       FIXED_HEIGHT);
  
  dragged_type_track = check_media_type (draggedWidget->sf);
  if (dragged_type_track == self->track_type  || dragged_type_track == PITIVI_VIDEO_AUDIO_TRACK)
    {
      add_to_layout ( GTK_WIDGET (self), 
		      current_wmed, 
		      x,
		      y);
      
      self->motion_area->x =  GTK_WIDGET (draggedWidget)->allocation.width;
      self->motion_area->width =  GTK_WIDGET (draggedWidget)->allocation.width;
      self->motion_area->height =  GTK_WIDGET (draggedWidget)->allocation.height;
      gtk_widget_show (current_wmed);
    }
}

static PitiviTimelineCellRenderer*
get_track_by_id (PitiviTimelineCellRenderer *actual, PitiviLayerType type_track_cmp)
{
  PitiviTimelineCellRenderer *childcells;
  GtkWidget	*container;
  GList	*childlist;
  
  container = gtk_widget_get_parent (GTK_WIDGET (actual));
  GList *childLayouts = gtk_container_get_children (GTK_CONTAINER (container));
  for (childlist = childLayouts; childlist; childlist = childlist->next )
    {
      if (GTK_IS_LAYOUT (childlist->data))
	{
	  if (actual->track_nb ==  PITIVI_TIMELINECELLRENDERER (childlist->data)->track_nb 
	      &&  PITIVI_TIMELINECELLRENDERER (childlist->data)->track_type == type_track_cmp)
	    {
	      return ( PITIVI_TIMELINECELLRENDERER (childlist->data));
	    }
	}
    }
  return NULL;
}

static void
pitivi_timelinecellrenderer_drag_on_source_file (PitiviTimelineCellRenderer *self, GtkSelectionData *selection, int x, int y)
{
  PitiviTimelineCellRenderer *layout;
  PitiviTimelineMedia	*current_media;
  PitiviTimelineMedia	*current_media_second;
  PitiviSourceFile	*sf;
  PitiviLayerType	type_track_cmp;
  guint64		length;

  length = DEFAULT_MEDIA_SIZE;
  sf = (PitiviSourceFile *) selection->data;
  if ( sf->length <= 0 )
    length = DEFAULT_MEDIA_SIZE;
  else
    sf->length = DEFAULT_MEDIA_SIZE;

  type_track_cmp = check_media_type (sf);
  if (type_track_cmp == self->track_type || (type_track_cmp == PITIVI_VIDEO_AUDIO_TRACK))
    {
      current_media = pitivi_timelinemedia_new (sf);
      gtk_widget_set_size_request (GTK_WIDGET (current_media), length, FIXED_HEIGHT);
      gtk_widget_show (GTK_WIDGET (current_media));
      add_to_layout ( GTK_WIDGET (self), GTK_WIDGET (current_media), x, y);
      if (type_track_cmp == PITIVI_VIDEO_AUDIO_TRACK)
	{
	  current_media_second = pitivi_timelinemedia_new (sf);
	  gtk_widget_set_size_request (GTK_WIDGET (current_media_second), length, FIXED_HEIGHT);
	  gtk_widget_show (GTK_WIDGET (current_media_second));
	  if (self->track_type == PITIVI_VIDEO_TRACK)
	    {
	      layout = get_track_by_id (self, PITIVI_AUDIO_TRACK);
	      add_to_layout ( GTK_WIDGET (layout), GTK_WIDGET (current_media_second), x, y);
	    }
	  else
	    {
	      layout = get_track_by_id (self, PITIVI_VIDEO_TRACK);
	      add_to_layout ( GTK_WIDGET (layout), GTK_WIDGET (current_media_second), x, y);
	    }
	}
    }
  else
    {
      if (self->track_type == PITIVI_VIDEO_TRACK &&  type_track_cmp == PITIVI_AUDIO_TRACK )
	{
	  current_media = pitivi_timelinemedia_new (sf);
	  gtk_widget_set_size_request (GTK_WIDGET (current_media), length, FIXED_HEIGHT);
	  gtk_widget_show (GTK_WIDGET (current_media));
	  layout = get_track_by_id (self, PITIVI_AUDIO_TRACK);
	  add_to_layout ( GTK_WIDGET (layout), GTK_WIDGET (current_media), x, y);
	}
      else if (self->track_type == PITIVI_AUDIO_TRACK && type_track_cmp == PITIVI_VIDEO_TRACK )
	{ 
	  current_media = pitivi_timelinemedia_new (sf);
	  gtk_widget_set_size_request (GTK_WIDGET (current_media), length, FIXED_HEIGHT);
	  gtk_widget_show (GTK_WIDGET (current_media));
	  layout = get_track_by_id (self, PITIVI_VIDEO_TRACK);
	  add_to_layout ( GTK_WIDGET (layout), GTK_WIDGET (current_media), x, y);
	}
    }
}


static void
pitivi_timelinecellrenderer_drag_data_received (GObject *object,
						GdkDragContext *context,
						int x,
						int y,
						GtkSelectionData *selection,
						guint info,
						guint time,
						gpointer data)
{
  PitiviCursor *cursor;
  PitiviTimelineCellRenderer *self = PITIVI_TIMELINECELLRENDERER (object);

  self->private->current_selection = selection;
  if (!selection->data) {
    gtk_drag_finish (context, FALSE, FALSE, time);
    return;
  }
  
  cursor = pitivi_getcursor_id (GTK_WIDGET(self));
  self->private->current_selection = selection;
  switch (info) 
    {
    case DND_TARGET_SOURCEFILEWIN:
      pitivi_timelinecellrenderer_drag_on_source_file (self, self->private->current_selection, x, y);
      gtk_drag_finish (context, TRUE, TRUE, time);
      break;
    case DND_TARGET_TIMELINEWIN:
      if (cursor->type == PITIVI_CURSOR_SELECT || cursor->type == PITIVI_CURSOR_HAND)
	{
	  pitivi_timelinecellrenderer_drag_on_same_widget (self, self->private->current_selection, x, y);
	  gtk_drag_finish (context, TRUE, TRUE, time);
	}
      break;
    case DND_TARGET_EFFECTSWIN:
      gtk_drag_finish (context, TRUE, TRUE, time);
      break;
    default:
      break;
    }
}

static gboolean 
pitivi_timelinecellrenderer_drag_drop (GtkWidget *widget, 
				       GdkDragContext *dc, 
				       gint x, 
				       gint y, 
				       guint time, 
				       gpointer data)
{
}

static void
pitivi_timelinecellrenderer_drag_motion (GtkWidget          *widget,
					 GdkDragContext     *drag_context,
					 gint                x,
					 gint                y,
					 guint               time)
{ 
  PitiviTimelineCellRenderer *self = (PitiviTimelineCellRenderer *) widget;
  PitiviCursor *cursor;
  guint mask = 0;

  cursor = pitivi_getcursor_id (widget);
  if (cursor->type == PITIVI_CURSOR_SELECT || cursor->type == PITIVI_CURSOR_HAND)
    {
      gdk_window_clear (GTK_LAYOUT (widget)->bin_window);
      pitivi_draw_slide (widget, x, DEFAULT_MEDIA_SIZE);
    }
}

PitiviCursor *
pitivi_getcursor_id (GtkWidget *widget)
{
  PitiviCursor        *cursor;
  PitiviMainApp	      *mainApp;
  PitiviToolboxWindow *tbxwin;
  PitiviToolbox	      *toolbox;
  GtkWidget	      *parent;
  
  cursor = NULL;
  parent = gtk_widget_get_toplevel (GTK_WIDGET (widget));
  if ( GTK_IS_WINDOW (parent) )
    {
      mainApp = pitivi_timelinewindow_get_mainApp (PITIVI_TIMELINEWINDOW (parent));
      tbxwin  = pitivi_mainapp_get_toolboxwindow (mainApp);
      toolbox = pitivi_toolboxwindow_get_toolbox (tbxwin);
      cursor = toolbox->pitivi_cursor;
    }
  return cursor;
}


// Checking On all Tracks if there is a selection, delsection begin

void 
pitivi_timelinecellrenderer_deselection_ontracks (GtkWidget *widget, gboolean self_deselected)
{
  PitiviTimelineCellRenderer *childcells;
  GtkWidget	*container;
  GList	*childlist;

  PitiviTimelineCellRenderer *self = (PitiviTimelineCellRenderer *) widget;
  container = gtk_widget_get_parent (GTK_WIDGET (self));
  if (GTK_IS_VBOX (container))
    {
      container = gtk_widget_get_parent (GTK_WIDGET (self));
      GList *childLayouts = gtk_container_get_children (GTK_CONTAINER (container));
      for (childlist = childLayouts; childlist; childlist = childlist->next )
	{
	  if (GTK_IS_LAYOUT (childlist->data))
	    {
	      childcells = PITIVI_TIMELINECELLRENDERER (childlist->data);
	      if (childcells->private->selected)
		{
		  if (GTK_WIDGET (childcells) != widget)
		    childcells->private->selected = FALSE;
		  else if (self_deselected == TRUE)
		    childcells->private->selected = FALSE;
		  gdk_window_clear_area_e (GTK_LAYOUT (childcells)->bin_window,\
					   0, 0, 
					   GTK_WIDGET (childcells)->allocation.width, 
					   GTK_WIDGET (childcells)->allocation.height);
		}
	    }
	}
    }
}

static gint
pitivi_timelinecellrenderer_button_release_event (GtkWidget      *widget,
						  GdkEventButton *event)
{
  PitiviTimelineCellRenderer *self = (PitiviTimelineCellRenderer *) widget;
  PitiviCursor		*cursor;
  GdkModifierType	mods;
  guint			mask;
  GtkWidget		*widgetchild;
  GList			*childlist;
  guint64		x1_selection = 0;
  guint64		x2_selection = MY_MAX;
  gboolean		old_state;
  guint			x, y = 0;
  
  x = event->x;
  y = event->y;

    
  cursor = pitivi_getcursor_id (widget);
  if (cursor->type == PITIVI_CURSOR_SELECT && event->state != 0)
    {
      if (event->button == PITIVI_MOUSE_LEFT_CLICK)
       {
	 // Clearing old Selection
	  
	 pitivi_timelinecellrenderer_deselection_ontracks (widget, FALSE);
	 old_state = self->private->selected;
	 self->private->selected = FALSE;
	 self->private->selected = old_state;
	 gdk_window_clear_area_e (GTK_LAYOUT (widget)->bin_window, 
				  0, 
				  0, 
				  widget->allocation.width, 
				  widget->allocation.height);
	 self->private->selected_area.y = 0;
	 self->private->selected_area.height = widget->allocation.height;
      
	 // Looking For Childs on Container
     
	 if (self->children && g_list_length (self->children) > 0)
	   {
	     for (childlist = self->children; childlist; childlist = childlist->next)
	       {
		 widgetchild = GTK_WIDGET (childlist->data);
		 if (event->window != widgetchild->window)
		   {
		     if (!(event->x >= widgetchild->allocation.x 
			   && event->x <=  widgetchild->allocation.x+widgetchild->allocation.width))
		       { 
			 if (x1_selection < widgetchild->allocation.x + widgetchild->allocation.width) 
			   if (event->x > widgetchild->allocation.x  + widgetchild->allocation.width)
			     x1_selection = widgetchild->allocation.x + widgetchild->allocation.width;
			 if (event->x < widgetchild->allocation.x)
			   if (x2_selection > widgetchild->allocation.x)
			     x2_selection = widgetchild->allocation.x;
		       }
		   }
		 else
		   {
		     // Case Selection on Widget Child On Layout
		     // Later manage on same widget
		     
		     x1_selection = widgetchild->allocation.x;
		     x2_selection = widgetchild->allocation.x+widgetchild->allocation.width;
		     PITIVI_TIMELINEMEDIA (widgetchild)->selected = TRUE;
		     break;
		   }
	       }
	   }
	 else
	   {
	     if (!self->private->selected)
	       self->private->selected = TRUE;
	     else
	       self->private->selected = FALSE;
	     self->private->selected_area.x = 0;
	     self->private->selected_area.width = widget->allocation.width;
	     draw_selection (widget, 0, 0);
	     return FALSE;
	   }
	 
	 if (x2_selection == MY_MAX)
	   x2_selection = -1;
      
	 // Case Selection On Same Last Area

	 if (self->private->selected_area.x == x1_selection 
	     && self->private->selected_area.width == x2_selection - x1_selection)
	   {
	     if (!self->private->selected)
	       self->private->selected = TRUE;
	     else
	       self->private->selected = FALSE;
	   }
	 else // Case On other Area
	   self->private->selected = TRUE;
      
	 self->private->selected_area.x = x1_selection;
	 if (x1_selection > 0 && x2_selection)
	   self->private->selected_area.width = x2_selection - x1_selection;
	 else
	   self->private->selected_area.width = x2_selection;
	 draw_selection_area (widget, &self->private->selected_area, 0, NULL);
       }
      else if (event->button == PITIVI_MOUSE_LEFT_CLICK)
	{
	  // Managing MOUSE LEFT CLICK
	  g_printf ("----------------\n");
	}
    }
  return FALSE;
}

static void
pitivi_timelinecellrenderer_instance_init (GTypeInstance * instance, gpointer g_class)
{
  GdkPixmap *pixmap;
  
  PitiviTimelineCellRenderer *self = (PitiviTimelineCellRenderer *) instance;

  self->private = g_new0(PitiviTimelineCellRendererPrivate, 1);
  
  // Motion notify Event Button Press release for selection

  gtk_widget_set_events (GTK_WIDGET (self),
			 GDK_BUTTON_RELEASE_MASK |
			 GDK_POINTER_MOTION_MASK | 
			 gtk_widget_get_events (GTK_WIDGET (self)));
  
  /* initialize all public and private members to reasonable default values. */
  
  self->private->dispose_has_run = FALSE;
  
  /* If you need specific consruction properties to complete initialization, 
   * delay initialization completion until the property is set. 
   */  
  
  // Initializations
    
  self->private->width  = FIXED_WIDTH;
  self->private->height = FIXED_HEIGHT;
  self->private->selected = FALSE;
  self->children = NULL;
  self->track_nb = 0;
  self->motion_area = g_new0 (GdkRectangle, 1);
  
  // Set background Color
  
  pixmap = pitivi_drawing_getpixmap (GTK_WIDGET(self), bg_xpm);
  pitivi_drawing_set_pixmap_bg (GTK_WIDGET(self), pixmap);
  
  //Signal Connections
  gtk_drag_dest_set  (GTK_WIDGET (self), GTK_DEST_DEFAULT_ALL, 
		      TargetEntries, 
		      iNbTargetEntries, 
		      GDK_ACTION_COPY);
  
  g_signal_connect (G_OBJECT (self), "drag_data_received",\
		    G_CALLBACK ( pitivi_timelinecellrenderer_drag_data_received ), NULL);
  g_signal_connect (G_OBJECT (self), "drag_motion",
		    G_CALLBACK ( pitivi_timelinecellrenderer_drag_motion ), NULL);
  g_signal_connect (G_OBJECT (self), "drag_drop",
		    G_CALLBACK ( pitivi_timelinecellrenderer_drag_drop ), NULL);
}

static void
pitivi_timelinecellrenderer_dispose (GObject *object)
{
  PitiviTimelineCellRenderer	*self = PITIVI_TIMELINECELLRENDERER(object);

  /* If dispose did already run, return. */
  if (self->private->dispose_has_run)
    return;
  
  /* Make sure dispose does not run twice. */
  self->private->dispose_has_run = TRUE;	

  /* 
   * In dispose, you are supposed to free all types referenced from this 
   * object which might themselves hold a reference to self. Generally, 
   * the most simple solution is to unref all members on which you own a 
   * reference. 
   */

}

static void
pitivi_timelinecellrenderer_finalize (GObject *object)
{
  PitiviTimelineCellRenderer	*self = PITIVI_TIMELINECELLRENDERER(object);

  /* 
   * Here, complete object destruction. 
   * You might not need to do much... 
   */

  g_free (self->private);
}

static void
pitivi_timelinecellrenderer_set_property (GObject * object,
			      guint property_id,
			      const GValue * value, GParamSpec * pspec)
{
  PitiviTimelineCellRenderer *self = (PitiviTimelineCellRenderer *) object;

  switch (property_id)
    {
    case PITIVI_TML_LAYER_PROPERTY:
      break;
    case PITIVI_TML_TYPE_LAYER_PROPERTY:
      self->track_type = g_value_get_int (value);
      break;
    case PITIVI_TML_TRACK_NB_PROPERTY:
      self->track_nb = g_value_get_int (value);
      break;
    default:
      g_assert (FALSE);
      break;
    }

}

static void
pitivi_timelinecellrenderer_get_property (GObject * object,
			      guint property_id,
			      GValue * value, GParamSpec * pspec)
{
  PitiviTimelineCellRenderer *self = (PitiviTimelineCellRenderer *) object;

  switch (property_id)
    {
    case PITIVI_TML_LAYER_PROPERTY:
      break;
    case PITIVI_TML_TYPE_LAYER_PROPERTY:
      break;
    case PITIVI_TML_TRACK_NB_PROPERTY:
      break;
    default:
      g_assert (FALSE);
      break;
    }
}

void
pitivi_timelinecellrenderer_remove (GtkContainer *container, GtkWidget *child)
{
  GList *list;
  PitiviTimelineCellRenderer *cell = PITIVI_TIMELINECELLRENDERER (container);
  
  list = g_list_find(cell->children, child);
  if (list) {
    GTK_CONTAINER_CLASS(parent_class)->remove (GTK_CONTAINER (container), GTK_WIDGET (child));
    if (cell->children != NULL)
      {
	g_list_free (cell->children);
	cell->children = NULL;
      }
    cell->children =  gtk_container_get_children (GTK_CONTAINER (container));
  }
}

static void
pitivi_timelinecellrenderer_class_init (gpointer g_class, gpointer g_class_data)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (g_class);

  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (g_class);
  GtkContainerClass *container_class = (GtkContainerClass*) (g_class);
  
  parent_class = gtk_type_class (GTK_TYPE_LAYOUT);
  gobject_class->constructor = pitivi_timelinecellrenderer_constructor;
  gobject_class->dispose = pitivi_timelinecellrenderer_dispose;
  gobject_class->finalize = pitivi_timelinecellrenderer_finalize;
  gobject_class->set_property = pitivi_timelinecellrenderer_set_property;
  gobject_class->get_property = pitivi_timelinecellrenderer_get_property;
  
  /* Widget properties */
  
  widget_class->expose_event = pitivi_timelinecellrenderer_expose;
  widget_class->button_release_event = pitivi_timelinecellrenderer_button_release_event;
  
  /* Container Properties */
  
  container_class->remove = pitivi_timelinecellrenderer_remove; 

  /* Install the properties in the class here ! */
    
  g_object_class_install_property (G_OBJECT_CLASS (gobject_class), PITIVI_TML_TYPE_LAYER_PROPERTY,
				   g_param_spec_int ("type","type","type",
						     G_MININT, G_MAXINT, 0,G_PARAM_READWRITE));
  
  g_object_class_install_property (G_OBJECT_CLASS (gobject_class), PITIVI_TML_TYPE_LAYER_PROPERTY,
				   g_param_spec_int ("track_nb","track_nb","track_nb",
						     G_MININT, G_MAXINT, 0,G_PARAM_READWRITE));
}

GType
pitivi_timelinecellrenderer_get_type (void)
{
  static GType type = 0;
 
  if (type == 0)
    {
      static const GTypeInfo info = {
	sizeof (PitiviTimelineCellRendererClass),
	NULL,			/* base_init */
	NULL,			/* base_finalize */
	pitivi_timelinecellrenderer_class_init,	/* class_init */
	NULL,			/* class_finalize */
	NULL,			/* class_data */
	sizeof (PitiviTimelineCellRenderer),
	0,			/* n_preallocs */
	pitivi_timelinecellrenderer_instance_init	/* instance_init */
      };
      type = g_type_register_static (GTK_TYPE_LAYOUT,
				     "PitiviTimelineCellRendererType", &info, 0);
    }
  return type;
}
