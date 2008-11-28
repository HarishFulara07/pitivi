# PiTiVi , Non-linear video editor
#
#       pitivi/ui/complextimeline.py
#
# Copyright (c) 2006, Edward Hervey <bilboed@bilboed.com>
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this program; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.

"""
Timeline widgets for the complex view
"""

import gtk
import gst
import pitivi.instance as instance

from pitivi.bin import SmartTimelineBin
from pitivi.timeline.source import TimelineFileSource
from complexlayer import LayerInfoList
import ruler
from complexinterface import Zoomable
import goocanvas
# FIXME : wildcard imports are BAD !
from util import *

from pitivi.utils import closest_item
from gettext import gettext as _

# ui imports
import dnd
from track import Track

# default heights for composition layer objects

RAZOR_LINE = (
    goocanvas.Rect,
    {
        "line_width" : 0,
        "fill_color" : "orange",
        "width" : 1,
    },
    {}
)

# the vsiual appearance for the selection marquee
MARQUEE = (
    goocanvas.Rect,
    {
        "stroke_color_rgba" : 0x33CCFF66,
        "fill_color_rgba" : 0x33CCFF66,
    },
    {}
)

# cursors to be used for resizing objects
ARROW = gtk.gdk.Cursor(gtk.gdk.ARROW)
# TODO: replace this with custom cursor
RAZOR_CURSOR = gtk.gdk.Cursor(gtk.gdk.XTERM)

# FIXME: do we want this expressed in pixels or miliseconds?
# If we express it in miliseconds, then we can have the core handle edge
# snapping (it's really best implemented in the core). On the other hand, if
# the dead-band is a constant unit of time, it will be too large at high zoom,
# and too small at low zoom. So we might want to be able to adjust the
# deadband from the UI.
# default number of pixels to use for edge snaping
DEADBAND = 5

# tooltip text for toolbar
DELETE = _("Delete Selected")
RAZOR = _("Cut clip at mouse position")
ZOOM_IN =  _("Zoom In")
ZOOM_OUT =  _("Zoom Out")
SELECT_BEFORE = ("Select all sources before selected")
SELECT_AFTER = ("Select all after selected")

# ui string for the complex timeline toolbar
ui = '''
<ui>
    <toolbar name="TimelineToolBar">
        <toolitem action="ZoomOut" />
        <toolitem action="ZoomIn" />
        <separator />
        <toolitem action="Razor" />
        <separator />
        <toolitem action="DeleteObj" />
        <toolitem action="SelectBefore" />
        <toolitem action="SelectAfter" />
    </toolbar>
</ui>
'''

# FIXME: this class should be renamed CompositionTracks, or maybe just Tracks.

class CompositionLayers(goocanvas.Canvas, Zoomable):
    """ Souped-up VBox that contains the timeline's CompositionLayer """

    def __init__(self, layerinfolist):
        goocanvas.Canvas.__init__(self)
        self._selected_sources = []
        self._timeline_position = 0

        self._block_size_request = False
        self.props.integer_layout = True
        self.props.automatic_bounds = False

        self.layerInfoList = layerinfolist
        self.layerInfoList.connect('layer-added', self._layerAddedCb)
        self.layerInfoList.connect('layer-removed', self._layerRemovedCb)

        self._createUI()
        self.connect("size_allocate", self._size_allocate)
       
    def _createUI(self):
        self._cursor = ARROW

        self.layers = VList(canvas=self)
        self.layers.connect("notify::width", self._request_size)
        self.layers.connect("notify::height", self._request_size)

        root = self.get_root_item()
        root.add_child(self.layers)

        root.connect("enter_notify_event", self._mouseEnterCb)
        self._marquee = make_item(MARQUEE)
        manage_selection(self, self._marquee, True, self._selection_changed_cb)

        self._razor = make_item(RAZOR_LINE)
        self._razor.props.visibility = goocanvas.ITEM_INVISIBLE
        root.add_child(self._razor)

## methods for dealing with updating the canvas size

    def block_size_request(self, status):
        self._block_size_request = status

    def _size_allocate(self, unused_layout, allocation):
        self._razor.props.height = allocation.height

    def _request_size(self, unused_item, unused_prop):
        #TODO: figure out why this doesn't work... (wtf?!?)
        if self._block_size_request:
            return True
        # we only update the bounds of the canvas by chunks of 100 pixels
        # in width, otherwise we would always be redrawing the whole canvas.
        # Make sure canvas is at least 800 pixels wide, and at least 100 pixels 
        # wider than it actually needs to be.
        w = max(800, ((int(self.layers.width + 100) / 100) + 1 ) * 100)
        h = int(self.layers.height)
        x1, y1, x2, y2 = self.get_bounds()
        pw = abs(x2 - x1)
        ph = abs(y2 - y1)
        if not (w == pw and h == ph):
            self.set_bounds(0, 0, w, h)
        return True

## mouse callbacks

    def _mouseEnterCb(self, unused_item, unused_target, event):
        event.window.set_cursor(self._cursor)
        return True

## Editing Operations

    # FIXME: here once again we're doing something that would be better done
    # in the core. As we add different types of objects in the Core, we'll
    # have to modify this code here (maybe there are different ways of
    # deleting different objects: you might delete() a source, but unset() a
    # keyframe)

    def deleteSelected(self, unused_action):
        for obj in self._selected_sources:
            if obj.comp:
                obj.comp.removeSource(obj.element, remove_linked=True, 
                    collapse_neighbours=False)
        set_selection(self, set())
        return True


    # FIXME: the razor is the one toolbar tool that violates the noun-verb
    # principle. Do I really want to make an exception for this? What about
    # just double-clicking on the source like jokosher?

    def activateRazor(self, unused_action):
        self._cursor = RAZOR_CURSOR
        # we don't want mouse events passing through to the canvas items
        # underneath, so we connect to the canvas's signals
        self._razor_sigid = self.connect("button_press_event", 
            self._razorClickedCb)
        self._razor_motion_sigid = self.connect("motion_notify_event",
            self._razorMovedCb)
        self._razor.props.visibility = goocanvas.ITEM_VISIBLE
        return True

    def _razorMovedCb(self, canvas, event):
        x = event_coords(self, event)[0]
        self._razor.props.x = self.nsToPixel(self.pixelToNs(x))
        return True

    def _razorClickedCb(self, unused_canvas, event):
        self._cursor = ARROW
        event.window.set_cursor(ARROW)
        self.disconnect(self._razor_sigid)
        self.disconnect(self._razor_motion_sigid)
        self._razor.props.visibility = goocanvas.ITEM_INVISIBLE

        # Find the topmost source under the mouse. This is tricky because not
        # all objects in the timeline are TimelineObjects. Some of them
        # are drag handles, for example. For now, only objects marked as
        # selectable should be sources
        x, y = event_coords(self, event)
        items = self.get_items_at(x, y, True)
        if not items:
            return True
        for item in items:
            if item.get_data("selectable"):
                parent = item.get_parent()
                gst.log("attempting to split source at position %d" %  x)
                self._splitSource(parent, self.pixelToNs(x))
        return True

    # FIXME: this DEFINITELY needs to be in the core. Also, do we always want
    # to split linked sources? Should the user be forced to un-link linked
    # sources when they only wisth to split one of them? If not, 

    def _splitSource(self, obj, editpoint):
        comp = obj.comp
        element = obj.element

        # we want to divide element in elementA, elementB at the
        # edit point.
        a_start = element.start
        a_end = editpoint
        b_start = editpoint
        b_end = element.start + element.duration

        # so far so good, but we need this expressed in the form
        # start/duration.
        a_dur = a_end - a_start
        b_dur = b_end - b_start
        if not (a_dur and b_dur):
            gst.Log("cannot cut at existing edit point, aborting")
            return

        # and finally, we need the media-start/duration for both sources.
        # in this case, media-start = media-duration, but this would not be
        # true if timestretch were applied to either source. this is why I
        # really think we should not have to care about media-start /duratoin
        # here, and have a more abstract method for setting time stretch that
        # would keep media start/duration in sync for sources that have it.
        a_media_start = element.media_start
        b_media_start = a_media_start + a_dur

        # trim source a
        element.setMediaStartDurationTime(a_media_start, a_dur)
        element.setStartDurationTime(a_start, a_dur)

        # add source b
        # TODO: for linked sources, split linked and create brother
        # TODO: handle other kinds of sources
        new = TimelineFileSource(factory=element.factory,
            media_type=comp.media_type)
        new.setMediaStartDurationTime(b_media_start, b_dur)
        new.setStartDurationTime(b_start, b_dur)
        comp.addSource(new, 0, True)

    # FIXME: should be implemented in core, if at all. Another alternative
    # would be directly suppporting ripple edits in the core, rather than
    # doing select after + move selection. 

    def selectBeforeCurrent(self, unused_action):
        pass

    def selectAfterCurrent(self, unused_action):
        ## helper function
        #def source_pos(ui_obj):
        #    return ui_obj.comp.getSimpleSourcePosition(ui_obj.element)

        ## mapping from composition -> (source1, ... sourceN)
        #comps = dict()
        #for source in self._selected_sources:
        #    if not source.comp in comps:
        #        comps[source.comp] = []
        #    comps[source.comp].append(source)

        ## find the latest source in each compostion, and all sources which
        ## occur after it. then select them.
        #to_select = set()
        #for comp, sources in comps.items():
        #    # source positions start at 1, not 0.
        #    latest = max((source_pos(source) for source in sources)) - 1
        #    # widget is available in "widget" data member of object.
        #    # we add the background of the widget, not the widget itself.
        #    objs = [obj.get_data("widget").bg for obj in comp.condensed[latest:]]
        #    to_select.update(set(objs))
        #set_selection(self, to_select)
        pass

    def _selection_changed_cb(self, selected, deselected):
        # TODO: filter this list for things other than sources, and put them
        # into appropriate lists
        for item in selected:
            item.props.fill_color_rgba = item.get_data("selected_color")
            parent = item.get_parent()
            self._selected_sources.append(parent)
        for item in deselected:
            item.props.fill_color_rgba = item.get_data("normal_color")
            parent = item.get_parent()
            self._selected_sources.remove(parent)

    def timelinePositionChanged(self, value, unused_frame):
        self._timeline_position = value

## Zoomable Override

    def zoomChanged(self):
        instance.PiTiVi.current.timeline.setDeadband(self.pixelToNs(DEADBAND))

    def setChildZoomAdjustment(self, adj):
        for layer in self.layers:
            layer.setZoomAdjustment(adj)

## LayerInfoList callbacks

    def _layerAddedCb(self, unused_infolist, layer, position):
        track = Track()
        track.setZoomAdjustment(self.getZoomAdjustment())
        track.set_composition(layer.composition)
        track.set_canvas(self)
        self.layers.insert_child(track, position)
        self.set_bounds(0, 0, self.layers.width, self.layers.height)
        self.set_size_request(int(self.layers.width), int(self.layers.height))

    def _layerRemovedCb(self, unused_layerInfoList, position):
        child = self.layers.item_at(position)
        self.layers.remove_child(child)
#
# Complex Timeline Design v2 (08 Feb 2006)
#
#
# Tree of contents (ClassName(ParentClass))
# -----------------------------------------
#
# ComplexTimelineWidget(gtk.VBox)
# |  Top container
# |
# +--ScaleRuler(gtk.Layout)
# |
# +--gtk.ScrolledWindow
#    |
#    +--CompositionLayers(goocanas.Canvas)
#    |  |
#    |  +--Track(SmartGroup)
#    |
#    +--Status Bar ??
#

class ComplexTimelineWidget(gtk.VBox):

    # the screen width of the current unit
    unit_width = 10 
    # specific levels of zoom, in (multiplier, unit) pairs which 
    # from zoomed out to zoomed in
    zoom_levels = (1, 5, 10, 20, 50, 100, 150) 

    def __init__(self):
        gst.log("Creating ComplexTimelineWidget")
        gtk.VBox.__init__(self)

        self._zoom_adj = gtk.Adjustment()
        self._zoom_adj.lower = self._computeZoomRatio(0)
        self._zoom_adj.upper = self._computeZoomRatio(-1)
        self._cur_zoom = 2
        self._zoom_adj.set_value(self._computeZoomRatio(self._cur_zoom))

        # common LayerInfoList
        self.layerInfoList = LayerInfoList()

        instance.PiTiVi.playground.connect('position',
           self._playgroundPositionCb)
        # project signals
        instance.PiTiVi.connect("new-project-loading",
            self._newProjectLoadingCb)
        instance.PiTiVi.connect("new-project-loaded",
            self._newProjectLoadedCb)
        instance.PiTiVi.connect("new-project-failed",
            self._newProjectFailedCb)
        self._createUI()

        # force update of UI
        self.layerInfoList.setTimeline(instance.PiTiVi.current.timeline)
        self.layerInfoList.connect("start-duration-changed",
            self._layerStartDurationChanged)

    def _createUI(self):
        self.leftSizeGroup = gtk.SizeGroup(gtk.SIZE_GROUP_HORIZONTAL)
        self.hadj = gtk.Adjustment()
        self.ruler = ruler.ScaleRuler(self.hadj)
        self.ruler.setZoomAdjustment(self._zoom_adj)
        self.ruler.set_size_request(0, 35)
        self.ruler.set_border_width(2)
        self.pack_start(self.ruler, expand=False, fill=True)

        # List of CompositionLayers
        self.compositionLayers = CompositionLayers(self.layerInfoList)
        self.compositionLayers.setZoomAdjustment(self._zoom_adj)
        self.scrolledWindow = gtk.ScrolledWindow(self.hadj)
        self.scrolledWindow.set_policy(gtk.POLICY_ALWAYS, gtk.POLICY_AUTOMATIC)
        self.scrolledWindow.add(self.compositionLayers)
        #FIXME: remove padding between scrollbar and scrolled window
        self.pack_start(self.scrolledWindow, expand=True)

        # drag and drop
        self.drag_dest_set(gtk.DEST_DEFAULT_DROP | gtk.DEST_DEFAULT_MOTION, 
            [dnd.FILESOURCE_TUPLE],
            gtk.gdk.ACTION_COPY)
        self.connect("drag-data-received", self._dragDataReceivedCb)
        self.connect("drag-leave", self._dragLeaveCb)
        self.connect("drag-motion", self._dragMotionCb)


        # toolbar actions
        actions = (
            ("ZoomIn", gtk.STOCK_ZOOM_IN, None, None, ZOOM_IN,
                self._zoomInCb),
            ("ZoomOut", gtk.STOCK_ZOOM_OUT, None, None, ZOOM_OUT, 
                self._zoomOutCb),
            ("DeleteObj", gtk.STOCK_DELETE, None, None, DELETE, 
                self.compositionLayers.deleteSelected),
            ("SelectBefore", gtk.STOCK_GOTO_FIRST, None, None, SELECT_BEFORE, 
                self.compositionLayers.selectBeforeCurrent),
            ("SelectAfter", gtk.STOCK_GOTO_LAST, None, None, SELECT_AFTER,
                self.compositionLayers.selectAfterCurrent),
            ("Razor", gtk.STOCK_CUT, None, None, RAZOR,
                self.compositionLayers.activateRazor)
        )
        self.actiongroup = gtk.ActionGroup("complextimeline")
        self.actiongroup.add_actions(actions)
        self.actiongroup.set_visible(False)
        uiman = instance.PiTiVi.gui.uimanager
        uiman.insert_action_group(self.actiongroup, 0)
        uiman.add_ui_from_string(ui)

## Drag and Drop callbacks

    def _dragMotionCb(self, unused_layout, unused_context, x, y, timestamp):

        # FIXME: temporarily add source to timeline, and put it in drag mode
        # so user can see where it will go
        gst.info("SimpleTimeline x:%d , source would go at %d" % (x, 0))

    def _dragLeaveCb(self, unused_layout, unused_context, unused_tstamp):
        gst.info("SimpleTimeline")
        #TODO: remove temp source from timeline

    def _dragDataReceivedCb(self, unused_layout, context, x, y, 
        selection, targetType, timestamp):
        gst.log("SimpleTimeline, targetType:%d, selection.data:%s" % 
            (targetType, selection.data))
        # FIXME: need to handle other types
        if targetType == dnd.TYPE_PITIVI_FILESOURCE:
            uri = selection.data
        else:
            context.finish(False, False, timestamp)
        # FIXME: access of instance, and playground
        factory = instance.PiTiVi.current.sources[uri]
        instance.PiTiVi.current.timeline.addFactory(factory)
        context.finish(True, False, timestamp)
        instance.PiTiVi.playground.switchToTimeline()

## Project callbacks

    def _newProjectLoadingCb(self, unused_inst, project):
        self.layerInfoList.setTimeline(project.timeline)

    def _newProjectLoadedCb(self, unused_inst, unused_project):
        # force set deadband when new timeline loads
        self.compositionLayers.zoomChanged()

    def _newProjectFailedCb(self, unused_inst, unused_reason, unused_uri):
        self.layerInfoList.setTimeline(None)

## layer callbacks

    def _layerStartDurationChanged(self, unused_layer):
        self.ruler.startDurationChanged()

## ToolBar callbacks

    ## override show()/hide() methods to take care of actions
    def show(self):
        super(ComplexTimelineWidget, self).show()
        self.actiongroup.set_visible(True)

    def show_all(self):
        super(ComplexTimelineWidget, self).show_all()
        self.actiongroup.set_visible(True)

    def hide(self):
        self.actiongroup.set_visible(False)
        super(ComplexTimelineWidget, self).hide()

    def _computeZoomRatio(self, index):
        return self.zoom_levels[index]

    def _zoomInCb(self, unused_action):
        self._cur_zoom = min(len(self.zoom_levels) - 1, self._cur_zoom + 1)
        self._zoom_adj.set_value(self._computeZoomRatio(self._cur_zoom))

    def _zoomOutCb(self, unused_action):
        self._cur_zoom = max(0, self._cur_zoom - 1)
        self._zoom_adj.set_value(self._computeZoomRatio(self._cur_zoom))

## PlayGround timeline position callback

    def _playgroundPositionCb(self, unused_playground, smartbin, value):
        if isinstance(smartbin, SmartTimelineBin):
            # for the time being we only inform the ruler
            self.ruler.timelinePositionChanged(value, 0)
            self.compositionLayers.timelinePositionChanged(value, 0)
