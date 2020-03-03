# vim:set et ts=4 sts=4:
# -*- coding: utf-8 -*-
#
# ibus-libpinyin - Intelligent Pinyin engine based on libpinyin for IBus
#
# Copyright (c) 2014 Peng Wu <alexepico@gmail.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.


__all__ = (
    "ShortcutEditor",
    "ShortcutEditorDialog"
)

import gettext
from gi.repository import GObject
from gi.repository import Gdk
from gi.repository import Gtk

from keyboardshortcut import KeyboardShortcutSelectionDialog

gettext.install('ibus-libpinyin')

(
COLUMN_DESCRIPTION,
COLUMN_CONFIG_KEYNAME,
COLUMN_ACCELERATOR,
) = range(3)

# The default shortcut value is stored here.
accelerators = \
    (
        (_("Switch Chinese/English"), "main-switch" , "<Shift>"),
        (_("Full/Half Width Letter"), "letter-switch", ""),
        (_("Full/Half Width Punct"), "punct-switch", "<Control>period"),
        (_("Full/Half Width Both Switch"), "both-switch", ""),
        (_("Switch Traditional/Simplfied Chinese"), "trad-switch", "<Control><Shift>f")
    )

class ShortcutTreeView(Gtk.TreeView):
    def __init__(self, editor):
        super(ShortcutTreeView, self).__init__()

        self.set_headers_visible(True)

        self.__model = self.__create_model()
        self.set_model(self.__model)

        self.__add_columns()

        self.__editor = editor

    def __create_model(self):
        model = Gtk.ListStore(str, str, str)

        for label, keyname, defvalue in accelerators:
            iter = model.append()
            # (accel_key, accel_mods) = Gtk.accelerator_parse(accel_str)
            model.set(iter,
                      COLUMN_DESCRIPTION, label,
                      COLUMN_CONFIG_KEYNAME, keyname,
                      COLUMN_ACCELERATOR, defvalue,
                      )

        return model

    def __add_columns(self):
        # column for description
        renderer = Gtk.CellRendererText()
        column = Gtk.TreeViewColumn(_('Description'), renderer, text=COLUMN_DESCRIPTION)
        self.append_column(column)

        # column for accelerator
        renderer = Gtk.CellRendererText()
        column = Gtk.TreeViewColumn(_('Accelerator'), renderer, text=COLUMN_ACCELERATOR)
        self.append_column(column)

    def set_shortcut_value(self, key, value):
        # just clean shortcut
        if value == "":
            for row in self.__model:
                if row[COLUMN_CONFIG_KEYNAME] == key:
                    row[COLUMN_ACCELERATOR] = value
                    return True

        # check duplicate shortcut
        for row in self.__model:
            if row[COLUMN_CONFIG_KEYNAME] == key:
                continue
            if row[COLUMN_ACCELERATOR] == value:
                dialog = Gtk.MessageDialog(None, 0, Gtk.MessageType.WARNING,
                                           Gtk.ButtonsType.OK,
                                           _("This shortcut key is already in use."))
                dialog.run()
                dialog.destroy()
                return False

        # store the shortcut
        for row in self.__model:
            if row[COLUMN_CONFIG_KEYNAME] == key:
                row[COLUMN_ACCELERATOR] = value
                return True

    def set_default_shortcut(self):
        selection = self.get_selection()
        (model, iterator) = selection.get_selected()

        if not iterator:
            return

        key = model[iterator][COLUMN_CONFIG_KEYNAME]
        for label, keyname, defvalue in accelerators:
            if key == keyname:
                if self.set_shortcut_value(key, defvalue):
                    self.__editor.emit_shortcut_changed(key, defvalue)

    def set_shortcut(self, value=""):
        selection = self.get_selection()
        (model, iterator) = selection.get_selected()

        if not iterator:
            return

        key = model[iterator][COLUMN_CONFIG_KEYNAME]
        if self.set_shortcut_value(key, value):
            self.__editor.emit_shortcut_changed(key, value)


class ShortcutEditor(Gtk.Box):
    __gtype_name__ = 'ShortcutEditor'

    __gsignals__ = {
        'shortcut-changed': (GObject.SIGNAL_RUN_FIRST, None,
                             (str, str, ))
    }

    def __init__(self):
        super(ShortcutEditor, self).__init__(
            orientation=Gtk.Orientation.VERTICAL)
        self.__init_ui()

    def __init_ui(self):
        # shortcut tree view
        self.__shortcut_treeview = ShortcutTreeView(self)
        self.__shortcut_treeview.connect("cursor-changed", self.__shortcut_treeview_cursor_changed_cb)
        self.pack_start(self.__shortcut_treeview, False, True, 4)

        # buttons
        hbox = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL)
        # set default button
        self.__set_default_button = Gtk.Button(label = _("_Default"),
                                               use_underline = True)
        self.__set_default_button.connect("clicked", self.__set_default_button_clicked_cb)
        hbox.pack_start(self.__set_default_button, False, True, 0)
        # edit button
        self.__edit_button = Gtk.Button(label= _("_Edit"),
                                        use_underline = True)
        self.__edit_button.connect("clicked", self.__edit_button_clicked_cb)
        hbox.pack_start(self.__edit_button, False, True, 0)
        self.pack_start(hbox, False, True, 4)
        # select the first row
        selection = self.__shortcut_treeview.get_selection()
        selection.set_mode(Gtk.SelectionMode.SINGLE)
        selection.select_path(Gtk.TreePath(0))
        self.show_all()

    def __shortcut_treeview_cursor_changed_cb(self, treeview):
        selection = treeview.get_selection()
        (model, iterator) = selection.get_selected()

        if iterator:
            self.__set_default_button.set_sensitive(True)
            self.__edit_button.set_sensitive(True)
        else:
            self.__set_default_button.set_sensitive(False)
            self.__edit_button.set_sensitive(False)

    def __set_default_button_clicked_cb(self, button):
        self.__shortcut_treeview.set_default_shortcut()

    def __edit_button_clicked_cb(self, button):
        dlg = KeyboardShortcutSelectionDialog(title = _("Select Switching Key"))
        buttons = (_("_Cancel"), Gtk.ResponseType.CANCEL,
                   _("_OK"), Gtk.ResponseType.OK)
        dlg.add_buttons(*buttons)

        selection = self.__shortcut_treeview.get_selection()
        (model, iterator) = selection.get_selected()
        if iterator:
            dlg.set_shortcut(model[iterator][COLUMN_ACCELERATOR])

        response = dlg.run()
        dlg.destroy()
        if response == Gtk.ResponseType.CANCEL:
            return

        self.__shortcut_treeview.set_shortcut(dlg.get_shortcut())

    def emit_shortcut_changed(self, key, value):
        self.emit("shortcut-changed", key, value)

    def update_shortcuts(self, config):
        for label, keyname, defvalue in accelerators:
            value = config.get_value(keyname).get_string()
            self.__shortcut_treeview.set_shortcut_value(keyname, value)
            # store the default value
            # self.emit_shortcut_changed(keyname, value)

class ShortcutEditorDialog(Gtk.Dialog):
    def __init__(self, title = None, transient_for = None, flags = 0):
        super(ShortcutEditorDialog, self).__init__(
                title = title, transient_for = transient_for, flags = flags)
        self.__shortcut_editor = ShortcutEditor()
        self.vbox.pack_start(self.__shortcut_editor, False, True, 0)
        self.vbox.show_all()


if __name__ == "__main__":
    dlg = ShortcutEditorDialog(title = "Shortcut Editor test")
    buttons = (_("_Cancel"), Gtk.ResponseType.CANCEL,
               _("_OK"), Gtk.ResponseType.OK)
    dlg.add_buttons(*buttons)
    print((dlg.run()))
