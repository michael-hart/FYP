"""Methods for retrieving information from eDVS and displaying to canvas"""

from enum import Enum
import logging
from Queue import Queue, Empty
import time
import threading
import wx
from loggers import init_loggers
from edvs import DVSReaderThread, DVSReader


class DVSPixelValue(Enum):
    BLACK = 0
    GREY = 1
    WHITE = 2


class DVSCanvas(wx.Panel):
    WIDTH = 128
    HEIGHT = 128
    def __init__(self, parent):
        super(DVSCanvas, self).__init__(parent)
        self.SetBackgroundStyle(wx.BG_STYLE_CUSTOM)
        self.pixel_grid = []
        for i in range(self.HEIGHT):
            self.pixel_grid += [[DVSPixelValue.GREY]*self.WIDTH]
        self.grid_lock = threading.Lock()
        self.Bind(wx.EVT_SIZE, self.on_size)
        self.Bind(wx.EVT_PAINT, self.on_paint)

    def on_size(self, event):
        event.Skip()
        self.Refresh()

    def on_paint(self, event):
        _log = logging.getLogger("DVS::GUI::Paint")
        _log.debug("Paint method called")
        w, h = self.GetClientSize()
        pens = [
            wx.Pen(wx.BLACK, 1),
            wx.Pen(wx.Colour(180, 180, 180), 4),
            wx.Pen(wx.WHITE, 1),
        ]
        brushes = [
            wx.Brush(wx.BLACK, wx.SOLID),
            wx.Brush(wx.RED, wx.SOLID),
            wx.Brush(wx.WHITE, wx.SOLID),
        ]
        dc = wx.AutoBufferedPaintDC(self)
        dc.Clear()
        with self.grid_lock:
            for row_ind, row in enumerate(self.pixel_grid):
                for col_ind, cell in enumerate(row):
                    dc.SetPen(pens[cell.value])
                    dc.SetBrush(brushes[cell.value])
                    dc.DrawRectangle(4*col_ind, 4*row_ind, 4, 4)


class DVSFrame(wx.Frame):
    """Custom frame to contain DVS Canvas"""
    def __init__(self):
        super(DVSFrame, self).__init__(None)
        self.SetTitle("eDVS Canvas")
        self.SetClientSize((4*128, 4*128))
        self.Center()
        self.view = DVSCanvas(self)


def main():
    init_loggers()
    _log = logging.getLogger('eDVS_Driver')
    _log.info("Attempting to initialise eDVS")

    app = wx.App()
    frame = DVSFrame()
    frame.Show()

    dvs_queue = Queue()

    # Continue variables for threads
    dvs_continue = True
    gui_continue = True

    def dvs_controller():
        """Initialises and runs eDVS controller"""
        with DVSReaderThread(DVSReader) as dvs:
            dvs.set_event_sending(True)

            def event_listener(data):
                dvs_queue.put(data)
            dvs.add_event_listener(event_listener)

            while dvs_continue:
                time.sleep(0.01)
        _log.info("DVS thread finished")

    def gui_update():
        """Watches queue for events and updates GUI canvas"""

        while gui_continue:
            try:
                event = dvs_queue.get(False)
            except Empty:
                time.sleep(0.001)
                continue
            # TODO update GUI canvas here
            # event = (30, 30, DVSPixelValue.WHITE)
            # print("Updating at co-ords {} {}".format(event[0], event[1]))
            try:
                with frame.view.grid_lock:
                    for point in event:
                        val = DVSPixelValue.GREY
                        if point[2]:
                            # Paint in white
                            val = DVSPixelValue.WHITE
                        else:
                            # Paint in black
                            val = DVSPixelValue.BLACK
                        frame.view.pixel_grid[point[0]][point[1]] = val
                # Refresh the UI
                frame.view.Refresh()
            except Exception as e:
                print e
                break
        _log.info("GUI thread finished")

    pool = []
    pool += [threading.Thread(target=dvs_controller)]
    pool += [threading.Thread(target=gui_update)]

    for t in pool:
        t.start()

    app.MainLoop()
    dvs_continue = False
    gui_continue = False


if __name__ == '__main__':
    main()
