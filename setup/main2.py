# vim:set et ts=4 sts=4:
# -*- coding: utf-8 -*-
#
# ibus-libpinyin - Intelligent Pinyin engine based on libpinyin for IBus
#
# Copyright (c) 2008-2010 Peng Huang <shawn.p.huang@gmail.com>
# Copyright (c) 2010 BYVoid <byvoid1@gmail.com>
# Copyright (c) 2011-2012 Peng Wu <alexepico@gmail.com>
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

from __future__ import print_function

import gettext

import locale
import os
import sys

from gi import require_version as gi_require_version
gi_require_version('GLib', '2.0')
gi_require_version('Gio', '2.0')
gi_require_version('Gtk', '3.0')
gi_require_version('IBus', '1.0')

from gi.repository import GLib
from gi.repository import Gio

# set_prgname before importing other modules to show the name in warning
# messages when import modules are failed. E.g. Gtk.
GLib.set_prgname('ibus-setup-libpinyin')

from gi.repository import Gtk
from gi.repository import IBus

import config
from dicttreeview import DictionaryTreeView
from shortcuteditor import ShortcutEditor

DOMAINNAME = 'ibus-libpinyin'
locale.setlocale(locale.LC_ALL, "")
localedir = os.getenv("IBUS_LOCALEDIR")
pkgdatadir = os.getenv("IBUS_PKGDATADIR") or "."

# Python's locale module doesn't provide all methods on some
# operating systems like FreeBSD
try:
    locale.bindtextdomain(DOMAINNAME, localedir)
    locale.bind_textdomain_codeset(DOMAINNAME, 'UTF-8')
except AttributeError:
    pass

gettext.bindtextdomain(DOMAINNAME, localedir)
gettext.bind_textdomain_codeset(DOMAINNAME, 'UTF-8')

gettext.install(DOMAINNAME, localedir)

class PreferencesDialog:
    def __init__(self, engine):
        self.__bus = IBus.Bus()
        self.__builder = Gtk.Builder()
        self.__builder.set_translation_domain(DOMAINNAME)
        self.__builder.add_from_file("ibus-libpinyin-preferences.ui")
        self.__dialog = self.__builder.get_object("dialog")
        self.__init_pages()
        
        if engine == "libpinyin":
            self.__config_namespace = "com.github.libpinyin.ibus-libpinyin.libpinyin"
            self.__config = Gio.Settings.new(self.__config_namespace)
            self.__init_general()
            self.__init_pinyin()
            self.__init_fuzzy()
            self.__init_dictionary()
            self.__init_user_data()
            self.__init_shortcut()
            self.__init_cloud_input()
            self.__init_about()
        elif engine == "libbopomofo":
            self.__config_namespace = "com.github.libpinyin.ibus-libpinyin.libbopomofo"
            self.__config = Gio.Settings.new(self.__config_namespace)
            self.__init_general()
            self.__init_bopomofo()
            self.__init_fuzzy()
            self.__init_dictionary()
            #self.__init_user_data()
            self.__init_shortcut()
            #self.__init_cloud_input()
            self.__init_about()
            self.__convert_fuzzy_pinyin_to_bopomofo()

        else:
            print("Error: Unknown Engine")
            exit()

        self.__pages.set_current_page(0)

    def __init_pages(self):
        self.__pages = self.__builder.get_object("pages")
        self.__page_general = self.__builder.get_object("pageGeneral")
        self.__page_pinyin_mode = self.__builder.get_object("pagePinyinMode")
        self.__page_bopomofo_mode = self.__builder.get_object("pageBopomofoMode")
        self.__page_fuzzy = self.__builder.get_object("pageFuzzy")
        self.__page_dictionary = self.__builder.get_object("pageDictionary")
        self.__page_user_data = self.__builder.get_object("pageUserData")
        self.__page_shortcut = self.__builder.get_object("pageShortcut")
        self.__page_cloud_input = self.__builder.get_object("pageCloudInput")
        self.__page_about = self.__builder.get_object("pageAbout")

        self.__page_general.hide()
        self.__page_pinyin_mode.hide()
        self.__page_bopomofo_mode.hide()
        self.__page_fuzzy.hide()
        self.__page_dictionary.hide()
        self.__page_user_data.hide()
        self.__page_cloud_input.hide()
        self.__page_about.hide()

    def __init_general(self):
        # page General
        self.__page_general.show()

        # init state
        self.__init_chinese = self.__builder.get_object("InitChinese")
        self.__init_english = self.__builder.get_object("InitEnglish")
        self.__init_full = self.__builder.get_object("InitFull")
        self.__init_half = self.__builder.get_object("InitHalf")
        self.__init_full_punct = self.__builder.get_object("InitFullPunct")
        self.__init_half_punct = self.__builder.get_object("InitHalfPunct")
        self.__init_simp = self.__builder.get_object("InitSimplifiedChinese")
        self.__init_trad = self.__builder.get_object("InitTraditionalChinese")

        # UI
        self.__lookup_table_orientation = self.__builder.get_object("LookupTableOrientation")
        self.__lookup_table_page_size = self.__builder.get_object("LookupTablePageSize")

        self.__dynamic_adjust = self.__builder.get_object("DynamicAdjust")
        self.__remember_every_input = self.__builder.get_object("RememberEveryInput")
        self.__show_suggestion = self.__builder.get_object("ShowSuggestion")
        self.__sort_candidate_option = self.__builder.get_object("SortCandidateOption")

        # read values
        self.__init_chinese.set_active(self.__get_value("init-chinese"))
        self.__init_full.set_active(self.__get_value("init-full"))
        self.__init_full_punct.set_active(self.__get_value("init-full-punct"))
        self.__init_simp.set_active(self.__get_value("init-simplified-chinese"))

        self.__lookup_table_orientation.set_active(self.__get_value("lookup-table-orientation"))
        self.__lookup_table_page_size.set_value(self.__get_value("lookup-table-page-size"))

        self.__dynamic_adjust.set_active(self.__get_value("dynamic-adjust"))
        self.__remember_every_input.set_active(self.__get_value("remember-every-input"))
        self.__show_suggestion.set_active(self.__get_value("show-suggestion"))
        self.__sort_candidate_option.set_active(self.__get_value("sort-candidate-option"))
        # connect signals
        self.__init_chinese.connect("toggled", self.__toggled_cb, "init-chinese")
        self.__init_full.connect("toggled", self.__toggled_cb, "init-full")
        self.__init_full_punct.connect("toggled", self.__toggled_cb, "init-full-punct")
        self.__init_simp.connect("toggled", self.__toggled_cb, "init-simplified-chinese")
        self.__dynamic_adjust.connect("toggled", self.__toggled_cb, "dynamic-adjust")
        self.__remember_every_input.connect("toggled", self.__toggled_cb, "remember-every-input")
        self.__show_suggestion.connect("toggled", self.__toggled_cb, "show-suggestion")

        def __lookup_table_page_size_changed_cb(adjustment):
            self.__set_value("lookup-table-page-size", int(adjustment.get_value()))

        def __lookup_table_orientation_changed_cb(widget):
            self.__set_value("lookup-table-orientation", widget.get_active())

        def __sort_candidate_option_changed_cb(widget):
            self.__set_value("sort-candidate-option", widget.get_active())

        self.__lookup_table_orientation.connect("changed", __lookup_table_orientation_changed_cb)
        self.__lookup_table_page_size.connect("value-changed", __lookup_table_page_size_changed_cb)
        self.__sort_candidate_option.connect("changed", __sort_candidate_option_changed_cb)

    def __init_pinyin(self):
        # page
        self.__page_pinyin_mode.show()
        
        # pinyin
        self.__full_pinyin = self.__builder.get_object("FullPinyin")
        self.__incomplete_pinyin = self.__builder.get_object("IncompletePinyin")
        self.__double_pinyin = self.__builder.get_object("DoublePinyin")
        self.__double_pinyin_schema = self.__builder.get_object("DoublePinyinSchema")
        # self.__double_pinyin_schema_label = self.__builder.get_object("labelDoublePinyinSchema")
        self.__double_pinyin_show_raw = self.__builder.get_object("DoublePinyinShowRaw")
        self.__double_pinyin_show_raw.hide ()

        # read value
        self.__incomplete_pinyin.set_active(self.__get_value("incomplete-pinyin"))
        self.__full_pinyin.set_active(not self.__get_value("double-pinyin"))
        self.__double_pinyin_schema.set_active(self.__get_value("double-pinyin-schema"))
        if self.__full_pinyin.get_active():
            # self.__incomplete_pinyin.set_sensitive(True)
            self.__double_pinyin_schema.set_sensitive(False)
            # self.__double_pinyin_schema_label.set_sensitive(False)
            self.__double_pinyin_show_raw.set_sensitive(False)
        else:
            # self.__incomplete_pinyin.set_sensitive(False)
            self.__double_pinyin_schema.set_sensitive(True)
            # self.__double_pinyin_schema_label.set_sensitive(True)
            self.__double_pinyin_show_raw.set_sensitive(True)

        def __double_pinyin_toggled_cb(widget):
            val = widget.get_active()
            self.__set_value("double-pinyin", val)
            self.__double_pinyin_schema.set_sensitive(val)
            # self.__double_pinyin_schema_label.set_sensitive(val)
            self.__double_pinyin_show_raw.set_sensitive(val)

        def __double_pinyin_schema_changed_cb(widget):
            self.__set_value("double-pinyin-schema", widget.get_active())

        # connect signals
        self.__double_pinyin.connect("toggled", __double_pinyin_toggled_cb)
        self.__incomplete_pinyin.connect("toggled", self.__toggled_cb, "incomplete-pinyin")
        self.__double_pinyin_schema.connect("changed", __double_pinyin_schema_changed_cb)
        self.__double_pinyin_show_raw.connect("toggled", self.__toggled_cb, "double-pinyin-show-raw")

        self.__init_input_custom()
        self.__init_correct_pinyin()

    def __init_bopomofo(self):
        # page Bopomodo Mode
        self.__page_bopomofo_mode.show()

        # bopomofo mode
        self.__incomplete_bopomofo = self.__builder.get_object("IncompleteBopomofo")
        self.__bopomofo_keyboard_mapping = self.__builder.get_object("BopomofoKeyboardMapping")
        
        # selection mode
        self.__select_keys = self.__builder.get_object("SelectKeys")
        self.__guide_key = self.__builder.get_object("GuideKey")
        self.__auxiliary_select_key_f = self.__builder.get_object("AuxiliarySelectKey_F")
        self.__auxiliary_select_key_kp = self.__builder.get_object("AuxiliarySelectKey_KP")

        # other
        self.__enter_key = self.__builder.get_object("CommitFirstCandidate")

        # read value
        self.__bopomofo_keyboard_mapping.set_active(self.__get_value("bopomofo-keyboard-mapping"))
        self.__incomplete_bopomofo.set_active(self.__get_value("incomplete-pinyin"))
        self.__select_keys.set_active(self.__get_value("select-keys"))
        self.__guide_key.set_active(self.__get_value("guide-key"))
        self.__auxiliary_select_key_f.set_active(self.__get_value("auxiliary-select-key-f"))
        self.__auxiliary_select_key_kp.set_active(self.__get_value("auxiliary-select-key-kp"))
        self.__enter_key.set_active(self.__get_value("enter-key"))

        # connect signals
        def __bopomofo_keyboard_mapping_changed_cb(widget):
            self.__set_value("bopomofo-keyboard-mapping", widget.get_active())
        def __select_keys_changed_cb(widget):
            self.__set_value("select-keys", widget.get_active())

        self.__bopomofo_keyboard_mapping.connect("changed", __bopomofo_keyboard_mapping_changed_cb)
        self.__incomplete_bopomofo.connect("toggled", self.__toggled_cb, "incomplete-pinyin")
        self.__select_keys.connect("changed", __select_keys_changed_cb)
        self.__guide_key.connect("toggled", self.__toggled_cb, "guide-key")
        self.__auxiliary_select_key_f.connect("toggled", self.__toggled_cb, "auxiliary-select-key-f")
        self.__auxiliary_select_key_kp.connect("toggled", self.__toggled_cb, "auxiliary-select-key-kp")
        self.__enter_key.connect("toggled", self.__toggled_cb, "enter-key")

    def __init_input_custom(self):
        # others
        self.__shift_select_candidate = self.__builder.get_object("ShiftSelectCandidate")
        self.__minus_equal_page = self.__builder.get_object("MinusEqualPage")
        self.__comma_period_page = self.__builder.get_object("CommaPeriodPage")
        self.__auto_commit = self.__builder.get_object("AutoCommit")

        # read values
        self.__shift_select_candidate.set_active(self.__get_value("shift-select-candidate"))
        self.__minus_equal_page.set_active(self.__get_value("minus-equal-page"))
        self.__comma_period_page.set_active(self.__get_value("comma-period-page"))
        self.__auto_commit.set_active(self.__get_value("auto-commit"))

        # connect signals
        self.__shift_select_candidate.connect("toggled", self.__toggled_cb, "shift-select-candidate")
        self.__minus_equal_page.connect("toggled", self.__toggled_cb, "minus-equal-page")
        self.__comma_period_page.connect("toggled", self.__toggled_cb, "comma-period-page")
        self.__auto_commit.connect("toggled", self.__toggled_cb, "auto-commit")

    def __init_correct_pinyin(self):
        # auto correct
        self.__correct_pinyin = self.__builder.get_object("CorrectPinyin")
        self.__correct_pinyin_widgets = [
            ('CorrectPinyin_GN_NG', 'correct-pinyin-gn-ng'),
            ('CorrectPinyin_MG_NG', 'correct-pinyin-mg-ng'),
            ('CorrectPinyin_IOU_IU', 'correct-pinyin-iou-iu'),
            ('CorrectPinyin_UEI_UI', 'correct-pinyin-uei-ui'),
            ('CorrectPinyin_UEN_UN', 'correct-pinyin-uen-un'),
            ('CorrectPinyin_UE_VE', 'correct-pinyin-ue-ve'),
            ('CorrectPinyin_V_U', 'correct-pinyin-v-u'),
            ('CorrectPinyin_ON_ONG', 'correct-pinyin-on-ong'),
        ]

        def __correct_pinyin_toggled_cb(widget):
            val = widget.get_active()
            for w in self.__correct_pinyin_widgets:
                self.__builder.get_object(w[0]).set_sensitive(val)
        self.__correct_pinyin.connect("toggled", __correct_pinyin_toggled_cb)

        # init value
        self.__correct_pinyin.set_active(self.__get_value("correct-pinyin"))
        for name, keyname in self.__correct_pinyin_widgets:
            widget = self.__builder.get_object(name)
            widget.set_active(self.__get_value(keyname))

        self.__correct_pinyin.connect("toggled", self.__toggled_cb, "correct-pinyin")
        for name, keyname in self.__correct_pinyin_widgets:
            widget = self.__builder.get_object(name)
            widget.connect("toggled", self.__toggled_cb, keyname)

    def __init_fuzzy(self):
        # page Fuzzy
        self.__page_fuzzy.show()

        # fuzzy pinyin
        self.__fuzzy_pinyin = self.__builder.get_object("FuzzyPinyin")
        self.__fuzzy_pinyin_widgets = [
            ('FuzzyPinyin_C_CH', 'fuzzy-pinyin-c-ch'),
            ('FuzzyPinyin_Z_ZH', 'fuzzy-pinyin-z-zh'),
            ('FuzzyPinyin_S_SH', 'fuzzy-pinyin-s-sh'),
            ('FuzzyPinyin_L_N', 'fuzzy-pinyin-l-n'),
            ('FuzzyPinyin_F_H', 'fuzzy-pinyin-f-h'),
            ('FuzzyPinyin_L_R', 'fuzzy-pinyin-l-r'),
            ('FuzzyPinyin_G_K', 'fuzzy-pinyin-g-k'),
            ('FuzzyPinyin_AN_ANG', 'fuzzy-pinyin-an-ang'),
            ('FuzzyPinyin_EN_ENG', 'fuzzy-pinyin-en-eng'),
            ('FuzzyPinyin_IN_ING', 'fuzzy-pinyin-in-ing'),
        ]

        def __fuzzy_pinyin_toggled_cb(widget):
            val = widget.get_active()
            for w in self.__fuzzy_pinyin_widgets:
                self.__builder.get_object(w[0]).set_sensitive(val)
        self.__fuzzy_pinyin.connect("toggled", __fuzzy_pinyin_toggled_cb)

        # init value
        self.__fuzzy_pinyin.set_active(self.__get_value("fuzzy-pinyin"))
        for name, keyname in self.__fuzzy_pinyin_widgets:
            widget = self.__builder.get_object(name)
            widget.set_active(self.__get_value(keyname))

        self.__fuzzy_pinyin.connect("toggled", self.__toggled_cb, "fuzzy-pinyin")
        for name, keyname in self.__fuzzy_pinyin_widgets:
            widget = self.__builder.get_object(name)
            widget.connect("toggled", self.__toggled_cb, keyname)

    def __convert_fuzzy_pinyin_to_bopomofo(self):
        options = [
            ("FuzzyPinyin_C_CH",   "ㄘ <=> ㄔ"),
            ("FuzzyPinyin_Z_ZH",   "ㄗ <=> ㄓ"),
            ("FuzzyPinyin_S_SH",   "ㄙ <=> ㄕ"),
            ("FuzzyPinyin_L_N",    "ㄌ <=> ㄋ"),
            ("FuzzyPinyin_F_H",    "ㄈ <=> ㄏ"),
            ("FuzzyPinyin_L_R",    "ㄌ <=> ㄖ"),
            ("FuzzyPinyin_G_K",    "ㄍ <=> ㄎ"),
            ("FuzzyPinyin_AN_ANG", "ㄢ <=> ㄤ"),
            ("FuzzyPinyin_EN_ENG", "ㄣ <=> ㄥ"),
            ("FuzzyPinyin_IN_ING", "ㄧㄣ <=> ㄧㄥ"),
        ]

        for name, label in options:
            self.__builder.get_object(name).set_label(label)


    def __init_dictionary(self):
        # page Dictionary
        self.__page_dictionary.show()

        # dictionary tree view
        self.__dict_treeview = self.__builder.get_object("Dictionaries")
        self.__dict_treeview.show()
        self.__dict_treeview.set_dictionaries(self.__get_value("dictionaries"))

        def __notified_dicts_cb(self, param, dialog):
            dialog.__set_value("dictionaries", self.get_dictionaries())

        # connect notify signal
        self.__dict_treeview.connect("notify::dictionaries", __notified_dicts_cb, self)

    def __init_user_data(self):
        #page User Data
        self.__page_user_data.show()

        self.__frame_lua_script = self.__builder.get_object("frameLuaScript")
        path = os.path.join(pkgdatadir, 'user.lua')
        if not os.access(path, os.R_OK):
            self.__frame_lua_script.hide()

        self.__edit_lua = self.__builder.get_object("EditLua")
        self.__edit_lua.connect("clicked", self.__edit_lua_cb)

        self.__import_dictionary = self.__builder.get_object("ImportDictionary")
        self.__import_dictionary.connect("clicked", self.__import_dictionary_cb)

        self.__export_dictionary = self.__builder.get_object("ExportDictionary")
        self.__export_dictionary.connect("clicked", self.__export_dictionary_cb)

        self.__clear_user_data = self.__builder.get_object("ClearUserData")
        self.__clear_user_data.connect("clicked", self.__clear_user_data_cb, "user")
        self.__clear_all_data = self.__builder.get_object("ClearAllData")
        self.__clear_all_data.connect("clicked", self.__clear_user_data_cb, "all")

    def __edit_lua_cb(self, widget):
        import shutil
        path = os.path.join(GLib.get_user_config_dir(), "ibus", "libpinyin")
        os.path.exists(path) or os.makedirs(path)
        path = os.path.join(path, "user.lua")
        if not os.path.exists(path):
            src = os.path.join(pkgdatadir, "user.lua")
            shutil.copyfile(src, path)
        os.system("xdg-open %s" % path)

    def __import_dictionary_cb(self, widget):
        dialog = Gtk.FileChooserDialog \
            (_("Please choose a file"), self.__dialog,
             Gtk.FileChooserAction.OPEN,
             (Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL,
              Gtk.STOCK_OPEN, Gtk.ResponseType.OK))

        filter_text = Gtk.FileFilter()
        filter_text.set_name("Text files")
        filter_text.add_mime_type("text/plain")
        dialog.add_filter(filter_text)

        response = dialog.run()
        if response == Gtk.ResponseType.OK:
            self.__set_value("import-dictionary", dialog.get_filename())

        dialog.destroy()

    def __export_dictionary_cb(self, widget):
        dialog = Gtk.FileChooserDialog \
                 (_("Please save a file"), self.__dialog,
                  Gtk.FileChooserAction.SAVE,
                  (Gtk.STOCK_CANCEL, Gtk.ResponseType.CANCEL,
                   Gtk.STOCK_SAVE, Gtk.ResponseType.OK))
        dialog.set_do_overwrite_confirmation(True)

        filter_text = Gtk.FileFilter()
        filter_text.set_name("Text files")
        filter_text.add_mime_type("text/plain")
        dialog.add_filter(filter_text)

        response = dialog.run()
        if response == Gtk.ResponseType.OK:
            self.__set_value("export-dictionary", dialog.get_filename())

        dialog.destroy()

    def __clear_user_data_cb(self, widget, name):
        self.__set_value("clear-user-data", name)

    def __init_shortcut(self):
        # page Shortcut
        self.__page_shortcut.show()

        # shortcut tree view
        self.__shortcut_editor = self.__builder.get_object("ShortcutsEditor")
        # work around for fedora 21
        self.__shortcut_editor.set_orientation(Gtk.Orientation.VERTICAL)
        self.__shortcut_editor.show()

        # connect "shortcut-changed" signal
        self.__shortcut_editor.connect("shortcut-changed", self.__shortcut_changed_cb)

        # set shortcuts
        self.__shortcut_editor.update_shortcuts(self.__config)

    def __shortcut_changed_cb(self, editor, key, value):
        self.__set_value(key, value)

    def __init_cloud_input(self):
        # page CloudInput
        self.__page_cloud_input.show()

        # init state
        self.__init_enable_cloud_input = self.__builder.get_object("InitEnableCloudInput")

        # cloud input option
        self.__candidates_order = self.__builder.get_object("FirstCloudCandidatePosition")
        self.__minimum_trigger_length = self.__builder.get_object("MinimumTriggerLength")
        self.__cloud_candidates_number = self.__builder.get_object("CloudCandidatesNumber")
        self.__cloud_input_source = self.__builder.get_object("CloudInputSource")

        if self.__init_enable_cloud_input.get_active():
            self.__cloud_input_source.set_sensitive(True)
            self.__minimum_trigger_length.set_sensitive(True)
            self.__cloud_candidates_number.set_sensitive(True)
            self.__candidates_order.set_sensitive(True)
        else:
            self.__cloud_input_source.set_sensitive(False)
            self.__minimum_trigger_length.set_sensitive(False)
            self.__cloud_candidates_number.set_sensitive(False)
            self.__candidates_order.set_sensitive(False)

        # read values
        self.__init_enable_cloud_input.set_active(self.__get_value("enable-cloud-input"))
        
        self.__candidates_order.set_value(self.__get_value("first-cloud-candidate-position"))
        self.__cloud_candidates_number.set_value(self.__get_value("cloud-candidates-number"))
        self.__minimum_trigger_length.set_value(self.__get_value("minimum-cloud-input-trigger-length"))
        self.__cloud_input_source.set_active(self.__get_value("cloud-input-source"))
        
        # connect signals
        def __enable_cloud_input_cb(widget):
            val = widget.get_active()
            self.__set_value("enable-cloud-input", val)
            self.__cloud_input_source.set_sensitive(val)
            self.__minimum_trigger_length.set_sensitive(val)
            self.__cloud_candidates_number.set_sensitive(val)
            self.__candidates_order.set_sensitive(val)
            
        def __candidates_order_changed_cb(adjustment):
            self.__set_value("first-cloud-candidate-position", int(adjustment.get_value()))

        def __minimum_trigger_length_changed_cb(adjustment):
            self.__set_value("minimum-cloud-input-trigger-length", int(adjustment.get_value()))
            
        def __cloud_candidates_number_changed_cb(adjustment):
            self.__set_value("cloud-candidates-number", int(adjustment.get_value()))

        def __cloud_input_source_changed_cb(widget):
            self.__set_value("cloud-input-source", widget.get_active())

        self.__init_enable_cloud_input.connect("toggled", __enable_cloud_input_cb)
        self.__candidates_order.connect("value-changed", __candidates_order_changed_cb)
        self.__minimum_trigger_length.connect("value-changed", __minimum_trigger_length_changed_cb)
        self.__cloud_candidates_number.connect("value-changed", __cloud_candidates_number_changed_cb)
        self.__cloud_input_source.connect("changed", __cloud_input_source_changed_cb)

    def __init_about(self):
        # page About
        self.__page_about.show()

        self.__name_version = self.__builder.get_object("NameVersion")
        self.__name_version.set_markup(_("<big><b>Intelligent Pinyin %s</b></big>") % config.get_version())

    def __changed_cb(self, widget, name):
        self.__set_value(name, widget.get_active())

    def __toggled_cb(self, widget, name):
        self.__set_value(name, widget.get_active ())

    def __get_value(self, name):
        var = self.__config.get_value(name)
        vartype = var.get_type_string()
        if vartype == 'b':
            return var.get_boolean()
        elif vartype == 'i':
            return var.get_int32()
        elif vartype == 's':
            return var.get_string()
        else:
            print("var(%s) is not in support type." % repr(var), file=sys.stderr)
            return None

    def __set_value(self, name, val):
        var = None
        if isinstance(val, bool):
            var = GLib.Variant.new_boolean(val)
        elif isinstance(val, int):
            var = GLib.Variant.new_int32(val)
        elif isinstance(val, str):
            var = GLib.Variant.new_string(val)
        else:
            print("val(%s) is not in support type." % repr(val), file=sys.stderr)
            return

        self.__config.set_value(name, var)

    def run(self):
        return self.__dialog.run()

def main():
    name = "libpinyin"
    if len(sys.argv) == 2:
        name = sys.argv[1]
    if name not in ("libpinyin", "libbopomofo"):
        name = "libpinyin"
    PreferencesDialog(name).run()


if __name__ == "__main__":
    main()
