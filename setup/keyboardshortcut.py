# vim:set et sts=4 sw=4:
#
# ibus-libpinyin - Intelligent Pinyin engine based on libpinyin for IBus
#
# Copyright (c) 2007-2014 Peng Huang <shawn.p.huang@gmail.com>
# Copyright (c) 2007-2014 Red Hat, Inc.
# Copyright (c) 2014 Peng Wu <alexepico@gmail.com>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
# USA

__all__ = (
    "KeyboardShortcutSelection",
    "KeyboardShortcutSelectionDialog",
);

import gettext
import locale
import os

from gi.repository import Gdk
from gi.repository import GObject
from gi.repository import Gtk
from gi.repository import IBus
from gi.repository import Pango

locale.setlocale(locale.LC_ALL, "")
localedir = os.getenv("IBUS_LOCALEDIR")
pkgdatadir = os.getenv("IBUS_PKGDATADIR") or "."
gettext.install('ibus-libpinyin', localedir)

class KeyboardShortcutSelection(Gtk.Box):
    def __init__(self, shortcut = None):
        super(KeyboardShortcutSelection, self).__init__(
                orientation=Gtk.Orientation.VERTICAL)
        self.__init_ui()
        self.set_shortcut(shortcut)

    def __init_ui(self):
        # shortcut label
        hbox = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL)
        label = Gtk.Label(_("Shortcut:"))
        label.set_justify(Gtk.Justification.LEFT)
        label.set_alignment(0.0, 0.5)
        hbox.pack_start(label, False, False, 4)

        self.__accel_label = Gtk.Label("")
        self.__accel_label.set_justify(Gtk.Justification.RIGHT)
        self.__accel_label.set_alignment(0.0, 0.5)
        hbox.pack_start(self.__accel_label, False, False, 4)
        self.pack_start(hbox, False, True, 4)

        # key code
        hbox = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL)
        label = Gtk.Label(label = _("Key code:"))
        label.set_justify(Gtk.Justification.LEFT)
        label.set_alignment(0.0, 0.5)
        hbox.pack_start(label, False, True, 4)

        self.__keycode_entry = Gtk.Entry()
        self.__keycode_entry.connect("notify::text", self.__keycode_entry_notify_cb)
        hbox.pack_start(self.__keycode_entry, True, True, 4)
        self.__keycode_button = Gtk.Button(label = "...")
        self.__keycode_button.connect("clicked", self.__keycode_button_clicked_cb)
        hbox.pack_start(self.__keycode_button, False, True, 4)
        self.pack_start(hbox, False, True, 4)

        # modifiers
        hbox = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL)
        label = Gtk.Label(label = _("Modifiers:"))
        label.set_justify(Gtk.Justification.LEFT)
        label.set_alignment(0.0, 0.5)
        hbox.pack_start(label, False, True, 4)

        table = Gtk.Table(n_rows = 4, n_columns = 2)
        self.__modifier_buttons = []
        self.__modifier_buttons.append(("Control",
                                        Gtk.CheckButton.new_with_mnemonic("_Control"),
                                        Gdk.ModifierType.CONTROL_MASK))
        self.__modifier_buttons.append(("Alt",
                                        Gtk.CheckButton.new_with_mnemonic("A_lt"),
                                        Gdk.ModifierType.MOD1_MASK))
        self.__modifier_buttons.append(("Shift",
                                        Gtk.CheckButton.new_with_mnemonic("_Shift"),
                                        Gdk.ModifierType.SHIFT_MASK))
        self.__modifier_buttons.append(("Meta",
                                        Gtk.CheckButton.new_with_mnemonic("_Meta"),
                                        Gdk.ModifierType.META_MASK))
        self.__modifier_buttons.append(("Super",
                                        Gtk.CheckButton.new_with_mnemonic("S_uper"),
                                        Gdk.ModifierType.SUPER_MASK))
        self.__modifier_buttons.append(("Hyper",
                                        Gtk.CheckButton.new_with_mnemonic("_Hyper"),
                                        Gdk.ModifierType.HYPER_MASK))
        # <CapsLock> is not parsed by gtk_accelerator_parse()
        # <Release> is not supported by XIGrabKeycode()
        for name, button, mask in self.__modifier_buttons:
            button.connect("toggled", self.__modifier_button_toggled_cb, name)

        table.attach(self.__modifier_buttons[0][1], 0, 1, 0, 1)
        table.attach(self.__modifier_buttons[1][1], 1, 2, 0, 1)
        table.attach(self.__modifier_buttons[2][1], 2, 3, 0, 1)
        table.attach(self.__modifier_buttons[3][1], 0, 1, 1, 2)
        table.attach(self.__modifier_buttons[4][1], 1, 2, 1, 2)
        table.attach(self.__modifier_buttons[5][1], 2, 3, 1, 2)
        hbox.pack_start(table, True, True, 4)
        self.pack_start(hbox, False, True, 4)

    def set_shortcut(self, shortcut = None):
        if shortcut == None:
            shortcut = ""
        self.__accel_label.set_label(shortcut)
        self.__set_shortcut_to_buttons(shortcut)

    def get_shortcut(self):
        return self.__accel_label.get_label()

    def __get_shortcut_from_buttons(self):
        modifiers = []
        keycode = self.__keycode_entry.get_text()
        if Gdk.keyval_from_name(keycode) == 0:
            return None

        for name, button, mask in self.__modifier_buttons:
            if button.get_active():
                modifiers.append(name)
        if keycode.startswith("_"):
            keycode = keycode[1:]
        shortcut = "".join(['<' + m + '>' for m in modifiers])
        shortcut += keycode
        return shortcut

    def __set_shortcut_to_buttons(self, shortcut):
        (keyval, state) = Gtk.accelerator_parse(shortcut)
        if keyval == 0 and state == 0:
            return
        for name, button, mask in self.__modifier_buttons:
            if state & mask:
                button.set_active(True)
            else:
                button.set_active(False)
        self.__keycode_entry.set_text(shortcut.rsplit('>', 1)[-1])

    def __update_accel_label(self):
        shortcut = self.__get_shortcut_from_buttons()
        can_add = shortcut != None
        self.__accel_label.set_label(shortcut)

    def __modifier_button_toggled_cb(self, button, name):
        self.__update_accel_label()

    def __keycode_entry_notify_cb(self, entry, arg):
        self.__update_accel_label()

    def __keycode_button_clicked_cb(self, button):
        out = []
        dlg = Gtk.MessageDialog(transient_for = self.get_toplevel(),
                                buttons = Gtk.ButtonsType.CLOSE)
        message = _("Please press a key (or a key combination).\n" \
                    "The dialog will close when the key is released.")
        dlg.set_markup(message)
        dlg.set_title(_("Please press a key (or a key combination)"))

        def __accel_edited_cb(c, path, keyval, state, keycode):
            out.append(keyval)
            out.append(state)
            out.append(keycode)
            dlg.response(Gtk.ResponseType.OK)

        model = Gtk.ListStore(GObject.TYPE_INT,
                              GObject.TYPE_UINT,
                              GObject.TYPE_UINT)
        accel_view = Gtk.TreeView(model)
        column = Gtk.TreeViewColumn(_("Shorcut Editor"))
        renderer = Gtk.CellRendererAccel(accel_mode=Gtk.CellRendererAccelMode.OTHER,
                                         editable=True)
        renderer.connect('accel-edited', __accel_edited_cb)
        column.pack_start(renderer, True)
        column.add_attribute(renderer, 'accel-mods', 0)
        column.add_attribute(renderer, 'accel-key', 1)
        column.add_attribute(renderer, 'keycode', 2)
        accel_view.append_column(column)
        it = model.append(None)
        area = dlg.get_message_area()
        area.pack_end(accel_view, True, True, 0)
        area.show_all()
        id = dlg.run()
        dlg.destroy()
        if id != Gtk.ResponseType.OK or len(out) < 3:
            return
        keyval = out[0]
        state = out[1]
        keycode = out[2]

        for name, button, mask in self.__modifier_buttons:
            if state & mask:
                button.set_active(True)
            else:
                button.set_active(False)

        shortcut = Gtk.accelerator_name_with_keycode(None,
                                                     keyval,
                                                     keycode,
                                                     state)
        shortcut = shortcut.replace('<Primary>', '<Control>')
        self.__keycode_entry.set_text(shortcut.rsplit('>', 1)[-1])


class KeyboardShortcutSelectionDialog(Gtk.Dialog):
    def __init__(self, title = None, transient_for = None, flags = 0):
        super(KeyboardShortcutSelectionDialog, self).__init__(
                title = title, transient_for = transient_for, flags = flags)
        self.__selection_view = KeyboardShortcutSelection()
        self.vbox.pack_start(self.__selection_view, False, True, 0)
        self.vbox.show_all()

    def set_shortcut(self, shortcut = None):
        self.__selection_view.set_shortcut(shortcut)

    def get_shortcut(self):
        return self.__selection_view.get_shortcut()



if __name__ == "__main__":
    dlg = KeyboardShortcutSelectionDialog(title = "Select test")
    buttons = (_("_Cancel"), Gtk.ResponseType.CANCEL,
               _("_OK"), Gtk.ResponseType.OK)
    dlg.add_buttons(*buttons)
    dlg.set_shortcut("<Shift>")
    dlg.set_shortcut(None)
    print((dlg.run()))
    print((dlg.get_shortcut()))

