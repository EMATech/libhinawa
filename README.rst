=====================
The libhinawa project
=====================

2022/06/30
Takashi Sakamoto

Instruction
===========

I design the library for userspace application to send asynchronous transaction to node in
IEEE 1394 bus and to handle asynchronous transaction initiated by the node. The library is
itself an application of Linux FireWire subsystem,
`GLib and GObject <https://gitlab.gnome.org/GNOME/glib>`_.

The library also has originally included some helper object classes for model-specific functions
via ALSA HwDep character device added by drivers in ALSA firewire stack. The object classes have
been already obsoleted and deligated the functions to
`libhitaki <https://github.com/alsa-project/libhitaki>`_, while are still kept for backward
compatibility. They should not be used for applications written newly.

The latest release is `2.5.1 <https://github.com/alsa-project/libhinawa/tags/2.5.1>`_.

Example of Python3 with PyGobject
=================================

::

    #!/usr/bin/env python3

    import gi
    gi.require_version('GLib', '2.0')
    gi.require_version('Hinawa', '3.0')
    from gi.repository import GLib, Hinawa

    from threading import Thread
    from struct import unpack

    node = Hinawa.FwNode.new()
    node.open('/dev/fw1')

    ctx = GLib.MainContext.new()
    src = node.create_source()
    src.attach(ctx)

    dispatcher = GLib.MainLoop.new(ctx, False)
    th = Thread(target=lambda d: d.run(), args=(dispatcher, ))
    th.start()

    addr = 0xfffff0000404
    req = Hinawa.FwReq.new()
    frame = [0] * 4
    frame = req.transaction_sync(node, Hinawa.FwTcode.READ_QUADLET_REQUEST, addr,
                                 len(frame), frame, 50)
    quad = unpack('>I', frame)[0]
    print('0x{:012x}: 0x{:02x}'.format(addr, quad))

    dispatcher.quit()
    th.join()

License
=======

- GNU Lesser General Public License version 2.1 or later

Documentation
=============

- `<https://alsa-project.github.io/gobject-introspection-docs/hinawa/>`_

Dependencies
============

- Glib 2.44.0 or later
- GObject Introspection 1.32.1 or later
- Linux kernel 3.12 or later

Requirements to build
=====================

- Meson 0.46.0 or later
- Ninja
- PyGObject (optional to run unit tests)
- gi-docgen (optional to generate API documentation)

How to build
============

::

    $ meson (--prefix=directory-to-install) build
    $ meson compile -C build
    $ meson install -C build
    ($ meson test -C build)

When working with gobject-introspection, ``Hinawa-3.0.typelib`` should be
installed in your system girepository so that ``libgirepository`` can find
it. Of course, your system LD should find ELF shared object for libhinawa2.
Before installing, it's good to check path of the above and configure
'--prefix' meson option appropriately. The environment variables,
``GI_TYPELIB_PATH`` and ``LD_LIBRARY_PATH`` are available for ad-hoc settings
of the above as well.

How to generate document
========================

::

    $ meson configure (--prefix=directory-to-install) -Ddoc=true build
    $ meson compile -C build
    $ meson install -C build

You can see documentation files under ``(directory-to-install)/share/doc/hinawa/``.

Sample scripts
==============

Some sample scripts are available under ``samples`` directory:

- gtk3 - PyGObject is required.
- gtk4 - PyGObject is required.
- qt5 - PyQt5 is required.

How to make DEB package
=======================

- Please refer to https://salsa.debian.org/debian/libhinawa.

How to make RPM package
=======================

1. Satisfy build dependencies

::

    $ dns install meson glib2-devel gobject-introspection-devel gi-docgen

2. make archive

::

    $ meson . build
    $ cd build
    $ meson dist
    ...
    meson-dist/libhinawa-2.5.1.tar.xz 3bc5833e102f38d3b08de89e6355deb83dffb81fb6cc34fc7f2fc473be5b4c47
    $ cd ..

3. copy the archive

::

    $ cp build/meson-dist/libhinawa-2.5.1.tar.xz ~/rpmbuild/SOURCES/

4. build package

::

    $ rpmbuild -bb libhinawa.spec

Deprecated object classes since v2.5 release
============================================

As I noted, some object classes are deprecated since `libhitaki <https://github.com/alsa-project/libhitaki>`_
is newly released with alternative classes. This is a list of the combination between deprecated
classes and alternatives:

- Hinawa.SndUnit / Hitaki.SndUnit
- Hinawa.SndDice / Hitaki.SndDice
- Hinawa.SndDg00x / Hitaki.SndDigi00x
- Hinawa.SndEfw / Hitaki.SndEfw
- Hinawa.SndMotu / Hitaki.SndMotu
- Hinawa.SndMotuRegisterDspParameter / Hitaki.SndMotuRegisterDspParameter
- Hinawa.SndTscm / Hitaki.SndTascam

Some GObject enumerations are also deprecated by the same reason. This is the list:

- Hinawa.SndUnitType / Hitaki.AlsaFirewireType
- Hinawa.SndUnitError / Hitaki.AlsaFirewireError
- Hinawa.SndEfwStatus / Hitaki.SndEfwError

Some instance properties are rewritten by GObject Interface. This is the list:

- Hinawa.SndUnit:card / Hitaki.AlsaFirewire:card-id
- Hinawa.SndUnit:device / Hitaki.AlsaFirewire:node-device
- Hinawa.SndUnit:guid / Hitaki.AlsaFirewire:guid
- Hinawa.SndUnit:streaming / Hitaki.AlsaFirewire:is-locked
- Hinawa.SndUnit:type / Hitaki.AlsaFirewire:unit-type

Some instance signals are rewritten by GObject Interface as well. This is the list:

- Hinawa.SndUnit::disconnected / use property change notify of Hitaki.AlsaFirewire:is-locked
- Hinawa.SndUnit::lock-status / use property change notify of Hitaki.AlsaFirewire:is-disconnected
- Hinawa.SndDg00x::message / Hitaki.QuadletNotification::notified
- Hinawa.SndDice::notified / Hitaki.QuadletNotification::notified
- Hinawa.SndMotu::notified / Hitaki.QuadletNotification::notified
- Hinawa.SndEfw::responded / Hitaki.EfwProtocol::responded
- Hinawa.SndMotu::register-dsp-changed / Hitaki.MotuRegisterDsp::changed

Some instance methods are rewritten by GObject Interface as well:

- Hinawa.SndUnit.create_source() / Hitaki.AlsaFirewire.create_source()

- Hinawa.SndUnit.lock() / Hitaki.AlsaFirewire.lock()
- Hinawa.SndUnit.unlock() / Hitaki.AlsaFirewire.unlock()
- Hinawa.SndUnit.open() / Hitaki.AlsaFirewire.open()
- Hinawa.SndDg00x.open() / Hitaki.AlsaFirewire.open()
- Hinawa.SndDice.open() / Hitaki.AlsaFirewire.open()
- Hinawa.SndEfw.open() / Hitaki.AlsaFirewire.open()
- Hinawa.SndMotu.open() / Hitaki.AlsaFirewire.open()
- Hinawa.SndTascam.open() / Hitaki.AlsaFirewire.open()
- Hinawa.SndEfw.transaction_async() / Hitaki.EfwProtocol.transmit_request()
- Hinawa.SndEfw.transaction_sync() / Hitaki.EfwProtocol.transaction()
- Hinawa.SndMotu.read_register_dsp_parameter() / Hitaki.MotuRegisterDsp.read_parameter()
- Hinawa.SndMotu.read_register_dsp_meter() / Hitaki.MotuRegisterDsp.read_byte_meter()
- Hinawa.SndMotu.read_command_dsp_meter() / Hitaki.MotuCommandDsp.read_float_meter()
- Hinawa.SndTscm.get_state() /  Hitaki.TascamProtocol.read_state()

Some GObject enumeration and methods are dropped due to some reasons:

- Hinawa.SndDiceError

  - (unused)

- Hinawa.SndUnit.get_node()

  - Please instantiate Hinawa.FwNode according to Hitaki.AlsaFirewire:node-device

- Hinawa.SndDice.transaction()

  - Please wait for Hitaki.SndDice::notified signal after any request transaction which causes
    the notification.

- Hinawa.SndEfw.transaction()

  - This is already deprecated. Hitaki.SndEfw.transaction() is available instead.

Lose of backward compatibility from v1 release.
===============================================

- HinawaFwUnit

  - This gobject class is dropped. Instead, HinawaFwNode should be used
    to communicate to the node on IEEE 1394 bus.

- HinawaFwReq/HinawaFwResp/HinawaFwFcp

  - Any API with arguments for HinawaFwUnit is dropped. Instead, use APIs
    with arguments for HinawaFwNode.
  - Any API with arguments for GByteArray is dropped. Instead, use APIs with
    arguments for guint8(buffer) and gsize(buffer length).

- HinawaSndEfw/HinawaSndDice

  - Any API with arguments for GArray is dropped. Instead, use APIs with
    arguments for guint32(buffer) and gsize(buffer length).

- I/O thread

  - No thread is launched internally for event dispatcher. Instead, retrieve
    GSource from HinawaFwNode and HinawaSndUnit and use it with GMainContext
    for event dispatcher. When no dispatcher runs, timeout occurs for any
    transaction.

- Notifier thread

  - No thread is launched internally for GObject signal notifier. Instead,
    implement another thread for your notifier by your own and delegate any
    transaction into it. This is required to prevent I/O thread to be stalled
    because of waiting for an additional event of the transaction.

end
