#!/usr/bin/env python3

import sys

# Gtk+3 gir
from gi.repository import Gtk

# Hinawa-1.0 gir
from gi.repository import Hinawa

from array import array

# helper function
def get_array():
    # The width with 'L' parameter is depending on environment.
    arr = array('L')
    if arr.itemsize is not 4:
        arr = array('I')
    return arr

# query sound devices
index = -1
while True:
    try:
        index = Hinawa.UnitQuery.get_sibling(index)
    except Exception as e:
        break
    break

# no fw sound devices are detected.
if index == -1:
    print('No sound FireWire devices found.')
    sys.exit()

# get unit type
try:
    unit_type = Hinawa.UnitQuery.get_unit_type(index)
except Exception as e:
    print(e)
    sys.exit()

# create sound unit
def handle_lock_status(snd_unit, status):
    if status:
        print("streaming is locked.");
    else:
        print("streaming is unlocked.");
if unit_type == 1:
    snd_unit = Hinawa.SndDice()
elif unit_type == 2:
    snd_unit = Hinawa.SndEfw()
elif unit_type == 3 or unit_type == 4:
    snd_unit = Hinawa.SndUnit()
path = "hw:{0}".format(index)
try:
    snd_unit.open(path)
except Exception as e:
    print(e)
    sys.exit()
print('Sound device info:')
print(' name:\t{0}'.format(snd_unit.get_property("name")))
print(' type:\t{0}'.format(snd_unit.get_property("type")))
print(' card:\t{0}'.format(snd_unit.get_property("card")))
print(' device:\t{0}'.format(snd_unit.get_property("device")))
print(' GUID:\t{0:016x}'.format(snd_unit.get_property("guid")))
snd_unit.connect("lock-status", handle_lock_status)

# create FireWire unit
def handle_bus_update(snd_unit):
	print(snd_unit.get_property('generation'))
snd_unit.connect("bus-update", handle_bus_update)

# start listening
try:
    snd_unit.listen()
except Exception as e:
    print(e)
    sys.exit()

# create firewire responder
resp = Hinawa.FwResp()
def handle_request(resp, tcode, frame):
    print('Requested with tcode {0}:'.format(tcode))
    for i in range(len(frame)):
        print(' [{0:02d}]: 0x{1:08x}'.format(i, frame[i]))
    return True
try:
    resp.register(snd_unit, 0xfffff0000d00, 0x100)
    resp.connect('requested', handle_request)
except Exception as e:
    print(e)
    sys.exit()

# create firewire requester
req = Hinawa.FwReq()

# Fireworks/BeBoB/OXFW supports FCP and some AV/C commands
if snd_unit.get_property('type') is not 1:
    request = bytes([0x01, 0xff, 0x19, 0x00, 0xff, 0xff, 0xff, 0xff])
    try:
        response = snd_unit.fcp_transact(request)
    except Exception as e:
        print(e)
        sys.exit()
    print('FCP Response:')
    for i in range(len(response)):
        print(' [{0:02d}]: 0x{1:02x}'.format(i, response[i]))

# Echo Fireworks Transaction
if snd_unit.get_property("type") is 2:
    args = get_array()
    args.append(5)
    try:
        params = snd_unit.transact(6, 1, args)
    except Exception as e:
        print(e)
        sys.exit()
    print('Echo Fireworks Transaction Response:')
    for i in range(len(params)):
        print(" [{0:02d}]: {1:08x}".format(i, params[i]))

# Dice notification
def handle_notification(self, message):
    print("Dice Notification: {0:08x}".format(message))
if snd_unit.get_property('type') is 1:
    snd_unit.connect('notified', handle_notification)

# GUI
class Sample(Gtk.Window):

    def __init__(self):
        Gtk.Window.__init__(self, title="Hinawa-1.0 gir sample")
        self.set_border_width(20)

        vbox = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=10)
        self.add(vbox)

        topbox = Gtk.Box(spacing=10)
        vbox.pack_start(topbox, True, True, 0)

        button = Gtk.Button("transact")
        button.connect("clicked", self.on_click_transact)
        topbox.pack_start(button, True, True, 0)

        button = Gtk.Button("_Close", use_underline=True)
        button.connect("clicked", self.on_click_close)
        topbox.pack_start(button, True, True, 0)

        bottombox = Gtk.Box(spacing=10)
        vbox.pack_start(bottombox, True, True, 0)

        self.entry = Gtk.Entry()
        self.entry.set_text("0xfffff0000980")
        bottombox.pack_start(self.entry, True, True, 0)

        self.label = Gtk.Label("result")
        self.label.set_text("0x00000000")
        bottombox.pack_start(self.label, True, True, 0)

    def on_click_transact(self, button):
        try:
            addr = int(self.entry.get_text(), 16)
            val = snd_unit.read_transact(addr, 1)
        except Exception as e:
            print(e)
            return

        self.label.set_text('0x{0:08x}'.format(val[0]))
        print(self.label.get_text())

    def on_click_close(self, button):
        print("Closing application")
        Gtk.main_quit()

# Main logic
win = Sample()
win.connect("delete-event", Gtk.main_quit)
win.show_all()

Gtk.main()

sys.exit()
