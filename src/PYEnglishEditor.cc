/* vim:set et ts=4 sts=4:
 *
 * ibus-libpinyin - Intelligent Pinyin engine based on libpinyin for IBus
 *
 * Copyright (c) 2010-2011 Peng Wu <alexepico@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "PYEnglishEditor.h"
#include <string.h>
#include <string>
#include <vector>
#include <stdio.h>
#include <libintl.h>
#include <sqlite3.h>
#include <glib.h>
#include <glib/gstdio.h>
#include "PYConfig.h"
#include "PYString.h"

#define _(text) (gettext(text))

namespace PY {

#define DB_BACKUP_TIMEOUT   (60)

class EnglishDatabase{
public:
    EnglishDatabase(){
        m_sqlite = NULL;
        m_sql = "";
        m_user_db = "";
        m_timeout_id = 0;
        m_timer = g_timer_new ();
    }

    ~EnglishDatabase(){
        g_timer_destroy (m_timer);
        if (m_timeout_id != 0) {
            saveUserDB ();
            g_source_remove (m_timeout_id);
        }

        if (m_sqlite){
            sqlite3_close (m_sqlite);
            m_sqlite = NULL;
        }
        m_sql = "";
        m_user_db = NULL;
    }

    gboolean isDatabaseExisted(const char *filename) {
         gboolean result = g_file_test (filename, G_FILE_TEST_IS_REGULAR);
         if (!result)
             return FALSE;

         sqlite3 *tmp_db = NULL;
         if (sqlite3_open_v2 (filename, &tmp_db,
                              SQLITE_OPEN_READONLY, NULL) != SQLITE_OK){
             return FALSE;
         }

         /* Check the desc table */
         sqlite3_stmt *stmt = NULL;
         const char *tail = NULL;
         m_sql = "SELECT value FROM desc WHERE name = 'version';";
         result = sqlite3_prepare_v2 (tmp_db, m_sql.c_str(), -1, &stmt, &tail);
         if (result != SQLITE_OK)
             return FALSE;

         result = sqlite3_step (stmt);
         if (result != SQLITE_ROW)
             return FALSE;

         result = sqlite3_column_type (stmt, 0);
         if (result != SQLITE_TEXT)
             return FALSE;

         const char *version = (const char *) sqlite3_column_text (stmt, 0);
         if (strcmp("1.2.0", version ) != 0)
             return FALSE;

         result = sqlite3_finalize (stmt);
         g_assert (result == SQLITE_OK);
         sqlite3_close (tmp_db);
         return TRUE;
    }

    gboolean createDatabase(const char *filename) {
        /* unlink the old database. */
        gboolean retval = g_file_test (filename, G_FILE_TEST_IS_REGULAR);
        if (retval) {
            int result = g_unlink (filename);
            if (result == -1)
                return FALSE;
        }

        char *dirname = g_path_get_dirname (filename);
        g_mkdir_with_parents (dirname, 0700);
        g_free (dirname);

        sqlite3 *tmp_db = NULL;
        if (sqlite3_open_v2 (filename, &tmp_db,
                             SQLITE_OPEN_READWRITE | 
                             SQLITE_OPEN_CREATE, NULL) != SQLITE_OK) {
            return FALSE;
        }

        /* Create DESCription table */
        m_sql = "BEGIN TRANSACTION;\n";
        m_sql << "CREATE TABLE IF NOT EXISTS desc (name TEXT PRIMARY KEY, value TEXT);\n";
        m_sql << "INSERT OR IGNORE INTO desc VALUES ('version', '1.2.0');";
        m_sql << "COMMIT;\n";

        if (!executeSQL (tmp_db)) {
            sqlite3_close (tmp_db);
            return FALSE;
        }

        /* Create Schema */
        m_sql = "CREATE TABLE IF NOT EXISTS english ("
                "word TEXT NOT NULL PRIMARY KEY,"
                "freq FLOAT NOT NULL DEFAULT(0)"
                ");";
        if (!executeSQL (tmp_db)) {
            sqlite3_close (tmp_db);
            return FALSE;
        }
        return TRUE;
    }

    gboolean openDatabase(const char *system_db, const char *user_db){
        if (!isDatabaseExisted (system_db))
            return FALSE;
        if (!isDatabaseExisted (user_db)) {
            gboolean result = createDatabase (user_db);
            if (!result)
                return FALSE;
        }
        /* cache the user db name. */
        m_user_db = user_db;

        /* do database attach here. :) */
        if (sqlite3_open_v2 (system_db, &m_sqlite,
                             SQLITE_OPEN_READWRITE |
                             SQLITE_OPEN_CREATE, NULL) != SQLITE_OK) {
            m_sqlite = NULL;
            return FALSE;
        }

#if 0
        m_sql.printf (SQL_ATTACH_DB, user_db);
        if (!executeSQL (m_sqlite)) {
            sqlite3_close (m_sqlite);
            m_sqlite = NULL;
            return FALSE;
        }
        return TRUE;
#endif
        return loadUserDB();
    }

    /* List the words in freq order. */
    gboolean listWords(const char *prefix, std::vector<std::string> & words){
        sqlite3_stmt *stmt = NULL;
        const char *tail = NULL;
        words.clear ();

        /* list words */
        const char *SQL_DB_LIST = 
            "SELECT word FROM ( "
            "SELECT * FROM english UNION ALL SELECT * FROM userdb.english) "
            " WHERE word LIKE \"%s%\" GROUP BY word ORDER BY SUM(freq) DESC;";
        m_sql.printf (SQL_DB_LIST, prefix);
        int result = sqlite3_prepare_v2 (m_sqlite, m_sql.c_str(), -1, &stmt, &tail);
        if (result != SQLITE_OK)
            return FALSE;

        result = sqlite3_step (stmt);
        while (result == SQLITE_ROW){
            /* get the words. */
            result = sqlite3_column_type (stmt, 0);
            if (result != SQLITE_TEXT)
                return FALSE;

            const char *word = (const char *)sqlite3_column_text (stmt, 0);
            words.push_back (word);
            result = sqlite3_step (stmt);
        }

        sqlite3_finalize (stmt);
        if (result != SQLITE_DONE)
            return FALSE;
        return TRUE;
    }

    /* Get the freq of user sqlite db. */
    gboolean getWordInfo(const char *word, float & freq){
        sqlite3_stmt *stmt = NULL;
        const char *tail = NULL;
        /* get word info. */
        const char *SQL_DB_SELECT = 
            "SELECT freq FROM userdb.english WHERE word = \"%s\";";
        m_sql.printf (SQL_DB_SELECT, word);
        int result = sqlite3_prepare_v2 (m_sqlite, m_sql.c_str(), -1, &stmt, &tail);
        g_assert (result == SQLITE_OK);
        result = sqlite3_step (stmt);
        if (result != SQLITE_ROW)
            return FALSE;
        result = sqlite3_column_type (stmt, 0);
        if (result != SQLITE_FLOAT)
            return FALSE;
        freq = sqlite3_column_double (stmt, 0);
        result = sqlite3_finalize (stmt);
        g_assert (result == SQLITE_OK);
        return TRUE;
    }

    /* Update the freq with delta value. */
    gboolean updateWord(const char *word, float freq){
        const char *SQL_DB_UPDATE =
            "UPDATE userdb.english SET freq = \"%f\" WHERE word = \"%s\";";
        m_sql.printf (SQL_DB_UPDATE, freq, word);
        gboolean retval =  executeSQL (m_sqlite);
        modified ();
        return retval;
    }

    /* Insert the word into user db with the initial freq. */
    gboolean insertWord(const char *word, float freq){
        const char *SQL_DB_INSERT =
            "INSERT INTO userdb.english (word, freq) VALUES (\"%s\", \"%f\");";
        m_sql.printf (SQL_DB_INSERT, word, freq);
        gboolean retval = executeSQL (m_sqlite);
        modified ();
        return retval;
    }

private:
    gboolean executeSQL(sqlite3 *sqlite){
        gchar *errmsg = NULL;
        if (sqlite3_exec (sqlite, m_sql.c_str (), NULL, NULL, &errmsg)
             != SQLITE_OK) {
            g_warning ("%s: %s", errmsg, m_sql.c_str());
            sqlite3_free (errmsg);
            return FALSE;
        }
        m_sql.clear ();
        return TRUE;
    }

    gboolean loadUserDB (void){
        sqlite3 *userdb =  NULL;
        /* Attach user database */
        do {
            const char *SQL_ATTACH_DB =
                "ATTACH DATABASE ':memory:' AS userdb;";
            m_sql.printf (SQL_ATTACH_DB);
            if (!executeSQL (m_sqlite))
                break;

            /* Note: user db is always created by openDatabase. */
            if (sqlite3_open_v2 ( m_user_db, &userdb,
                                  SQLITE_OPEN_READWRITE |
                                  SQLITE_OPEN_CREATE, NULL) != SQLITE_OK)
                break;

            sqlite3_backup *backup = sqlite3_backup_init (m_sqlite, "userdb", userdb, "main");

            if (backup) {
                sqlite3_backup_step (backup, -1);
                sqlite3_backup_finish (backup);
            }

            sqlite3_close (userdb);
            return TRUE;
        } while (0);

        if (userdb)
            sqlite3_close (userdb);
        return FALSE;
    }

    gboolean saveUserDB (void){
        sqlite3 *userdb = NULL;
        String tmpfile = String(m_user_db) + "-tmp";
        do {
            /* remove tmpfile if it exist */
            g_unlink(tmpfile);

            if (sqlite3_open_v2 (tmpfile, &userdb,
                                 SQLITE_OPEN_READWRITE |
                                 SQLITE_OPEN_CREATE, NULL) != SQLITE_OK)
                break;

            sqlite3_backup *backup = sqlite3_backup_init (userdb, "main", m_sqlite, "userdb");

            if (backup == NULL)
                break;

            sqlite3_backup_step (backup, -1);
            sqlite3_backup_finish (backup);
            sqlite3_close (userdb);

            g_rename(tmpfile, m_user_db);
            return TRUE;
        } while (0);

        if (userdb)
            sqlite3_close (userdb);
        g_unlink (tmpfile);
        return FALSE;
    }

    void modified (void){
        /* Restart the timer */
        g_timer_start (m_timer);

        if (m_timeout_id != 0)
            return;

        m_timeout_id = g_timeout_add_seconds (DB_BACKUP_TIMEOUT,
                                              EnglishDatabase::timeoutCallback,
                                              static_cast<gpointer> (this));
    }

    static gboolean timeoutCallback (gpointer data){
        EnglishDatabase *self = static_cast<EnglishDatabase *> (data);

        /* Get elapsed time since last modification of database. */
        guint elapsed = (guint) g_timer_elapsed (self->m_timer, NULL);

        if (elapsed >= DB_BACKUP_TIMEOUT &&
            self->saveUserDB ()) {
            self->m_timeout_id = 0;
            return FALSE;
        }

        return TRUE;
    }

    sqlite3 *m_sqlite;
    String m_sql;
    const char *m_user_db;

    guint m_timeout_id;
    GTimer *m_timer;
};

EnglishEditor::EnglishEditor (PinyinProperties & props, Config &config)
    : Editor (props, config), m_train_factor (0.1)
{
    m_english_database = new EnglishDatabase;

    gchar *path = g_build_filename (g_get_user_cache_dir (),
                                     "ibus", "libpinyin", "english-user.db", NULL);

    gboolean result = m_english_database->openDatabase
        (".." G_DIR_SEPARATOR_S "data" G_DIR_SEPARATOR_S "english.db",
         "english-user.db") ||
        m_english_database->openDatabase
        (PKGDATADIR G_DIR_SEPARATOR_S "db" G_DIR_SEPARATOR_S "english.db", path);
    if (!result)
        g_warning ("can't open English word list database.\n");
}

EnglishEditor::~EnglishEditor ()
{
    delete m_english_database;
    m_english_database = NULL;
}

gboolean
EnglishEditor::processKeyEvent (guint keyval, guint keycode, guint modifiers)
{
    //IBUS_SHIFT_MASK is removed.
    modifiers &= (IBUS_CONTROL_MASK |
                  IBUS_MOD1_MASK |
                  IBUS_SUPER_MASK |
                  IBUS_HYPER_MASK |
                  IBUS_META_MASK |
                  IBUS_LOCK_MASK);
    if (modifiers)
        return FALSE;

    //handle backspace/delete here.
    if (processEditKey (keyval))
        return TRUE;

    //handle page/cursor up/down here.
    if (processPageKey (keyval))
        return TRUE;

    //handle label key select here.
    if (processLabelKey (keyval))
        return TRUE;

    if (processSpace (keyval))
        return TRUE;

    if (processEnter (keyval))
        return TRUE;

    m_cursor = std::min (m_cursor, (guint)m_text.length ());

    /* Remember the input string. */
    if (m_cursor == 0) {
        g_return_val_if_fail ('v' == keyval || 'V' == keyval, FALSE);
        m_text.insert (m_cursor, keyval);
        m_cursor ++;
    } else {
        g_return_val_if_fail ('v' == m_text[0] || 'V' == m_text[0], FALSE);
        if ((keyval >= 'a' && keyval <= 'z') ||
            (keyval >= 'A' && keyval <= 'Z')) {
            m_text.insert (m_cursor, keyval);
            m_cursor ++;
        }
    }

    /* Deal other staff with updateStateFromInput (). */
    updateStateFromInput ();
    update ();
    return TRUE;
}

gboolean
EnglishEditor::processEditKey (guint keyval)
{
    switch (keyval) {
    case IBUS_Delete:
    case IBUS_KP_Delete:
        removeCharAfter ();
        updateStateFromInput ();
        update ();
        return TRUE;
    case IBUS_BackSpace:
        removeCharBefore ();
        updateStateFromInput ();
        update ();
        return TRUE;
    }
    return FALSE;
}

gboolean
EnglishEditor::processPageKey (guint keyval)
{
    switch (keyval) {
    case IBUS_comma:
        if (m_config.commaPeriodPage ()) {
            pageUp ();
            return TRUE;
        }
        break;
    case IBUS_minus:
        if (m_config.minusEqualPage ()) {
            pageUp ();
            return TRUE;
        }
        break;
    case IBUS_period:
        if (m_config.commaPeriodPage ()) {
            pageDown ();
            return TRUE;
        }
        break;
    case IBUS_equal:
        if (m_config.minusEqualPage ()) {
            pageDown ();
            return TRUE;
        }
        break;
    case IBUS_Up:
    case IBUS_KP_Up:
        cursorUp ();
        return TRUE;

    case IBUS_Down:
    case IBUS_KP_Down:
        cursorDown ();
        return TRUE;

    case IBUS_Page_Up:
    case IBUS_KP_Page_Up:
        pageUp ();
        return TRUE;

    case IBUS_Page_Down:
    case IBUS_KP_Page_Down:
        pageDown ();
        return TRUE;

    case IBUS_Escape:
        reset ();
        return TRUE;
    }
    return FALSE;
}

gboolean
EnglishEditor::processLabelKey (guint keyval)
{
    switch (keyval) {
    case '1' ... '9':
        return selectCandidateInPage (keyval - '1');
        break;
    case '0':
        return selectCandidateInPage (9);
        break;
    }
    return FALSE;
}

gboolean
EnglishEditor::processEnter (guint keyval)
{
    if (keyval != IBUS_Return)
        return FALSE;

    if (m_text.length () == 0)
        return FALSE;

    String word = m_text;
    word.erase (0, 1);

    Text text(word);
    commitText (text);
    train (word.c_str (), m_train_factor);
    reset ();
    return TRUE;
}

gboolean
EnglishEditor::processSpace (guint keyval)
{
    if (!(keyval == IBUS_space || keyval == IBUS_KP_Space))
        return FALSE;

    if (m_text == "v" || m_text == "V") {
        reset ();
        return TRUE;
    }

    guint cursor_pos = m_lookup_table.cursorPos ();
    return selectCandidate (cursor_pos);
}

void
EnglishEditor::candidateClicked (guint index, guint button, guint state)
{
    selectCandidateInPage (index);
}

gboolean
EnglishEditor::selectCandidateInPage (guint index)
{
    guint page_size = m_lookup_table.pageSize ();
    guint cursor_pos = m_lookup_table.cursorPos ();

    if (G_UNLIKELY (index >= page_size))
        return FALSE;
    index += (cursor_pos / page_size) * page_size;

    return selectCandidate (index);
}

gboolean
EnglishEditor::selectCandidate (guint index)
{
    if (index >= m_lookup_table.size ())
        return FALSE;

    IBusText *candidate = m_lookup_table.getCandidate (index);
    Text text (candidate);
    commitText (text);
    train (candidate->text, m_train_factor);
    reset ();
    return TRUE;
}

gboolean
EnglishEditor::updateStateFromInput (void)
{
    /* Do parse and candidates update here. */
    /* prefix v double check here. */
    if (m_text.empty ()) {
        m_preedit_text = "";
        m_auxiliary_text = "";
        m_cursor = 0;
        clearLookupTable ();
        return FALSE;
    }

    if ('v' != m_text[0] && 'V' != m_text[0]) {
        g_warning ("v is expected in m_text string.\n");
        m_auxiliary_text = "";
        clearLookupTable ();
        return FALSE;
    }

    m_auxiliary_text = m_text[0];

    if (1 == m_text.length ()) {
        clearLookupTable ();

        const char * help_string = _("Please input the English word.");
        int space_len = std::max ( 0, m_aux_text_len
                                   - (int) g_utf8_strlen (help_string, -1));
        m_auxiliary_text.append (space_len, ' ');
        m_auxiliary_text += help_string;

        return TRUE;
    }

    m_auxiliary_text += " ";

    String prefix = m_text.substr (1);
    m_auxiliary_text += prefix;

    /* lookup table candidate fill here. */
    std::vector<std::string> words;
    gboolean retval = m_english_database->listWords (prefix.c_str (), words);
    if (!retval)
        return FALSE;

    clearLookupTable ();
    std::vector<std::string>::iterator iter;
    for (iter = words.begin (); iter != words.end (); ++iter){
        Text text (*iter);
        m_lookup_table.appendCandidate (text);
    }
    return TRUE;
}

/* Auxiliary Functions */

void
EnglishEditor::pageUp (void)
{
    if (G_LIKELY (m_lookup_table.pageUp ())) {
        update ();
    }
}

void
EnglishEditor::pageDown (void)
{
    if (G_LIKELY (m_lookup_table.pageDown ())) {
        update ();
    }
}

void
EnglishEditor::cursorUp (void)
{
    if (G_LIKELY (m_lookup_table.cursorUp ())) {
        update ();
    }
}

void
EnglishEditor::cursorDown (void)
{
    if (G_LIKELY (m_lookup_table.cursorDown ())) {
        update ();
    }
}

void
EnglishEditor::update (void)
{
    updateLookupTable ();
    updatePreeditText ();
    updateAuxiliaryText ();
}

void
EnglishEditor::reset (void)
{
    m_text = "";
    updateStateFromInput ();
    update ();
}

void
EnglishEditor::clearLookupTable (void)
{
    m_lookup_table.clear ();
    m_lookup_table.setPageSize (m_config.pageSize ());
    m_lookup_table.setOrientation (m_config.orientation ());
}

void
EnglishEditor::updateLookupTable (void)
{
    if (m_lookup_table.size ()) {
        Editor::updateLookupTableFast (m_lookup_table, TRUE);
    } else {
        hideLookupTable ();
    }
}

void
EnglishEditor::updatePreeditText (void)
{
    if (G_UNLIKELY (m_preedit_text.empty ())) {
        hidePreeditText ();
        return;
    }

    StaticText preedit_text (m_preedit_text);
    Editor::updatePreeditText (preedit_text, m_cursor, TRUE);
}

void
EnglishEditor::updateAuxiliaryText (void)
{
    if (G_UNLIKELY (m_auxiliary_text.empty ())) {
        hideAuxiliaryText ();
        return;
    }
    
    StaticText aux_text (m_auxiliary_text);
    Editor::updateAuxiliaryText (aux_text, TRUE);
}

gboolean
EnglishEditor::removeCharBefore (void)
{
    if (G_UNLIKELY (m_cursor <= 0)) {
        m_cursor = 0;
        return FALSE;
    }

    if (G_UNLIKELY (m_cursor > m_text.length ())) {
        m_cursor = m_text.length ();
        return FALSE;
    }

    m_text.erase (m_cursor - 1, 1);
    m_cursor = std::max (0, static_cast<int>(m_cursor) - 1);
    return TRUE;
}

gboolean
EnglishEditor::removeCharAfter (void)
{
    if (G_UNLIKELY (m_cursor < 0)) {
        m_cursor = 0;
        return FALSE;
    }

    if (G_UNLIKELY (m_cursor >= m_text.length ())) {
        m_cursor = m_text.length ();
        return FALSE;
    }

    m_text.erase (m_cursor, 1);
    m_cursor = std::min (m_cursor, (guint) m_text.length ());
    return TRUE;
}

gboolean
EnglishEditor::train (const char *word, float delta)
{
    float freq = 0;
    gboolean retval = m_english_database->getWordInfo (word, freq);
    if (retval) {
        freq += delta;
        m_english_database->updateWord (word, freq);
    } else {
        m_english_database->insertWord (word, delta);
    }
    return TRUE;
}

#if 0

/* using static initializor to test english database here. */
static class TestEnglishDatabase{
public:
    TestEnglishDatabase (){
        EnglishDatabase *db = new EnglishDatabase ();
        bool retval = db->isDatabaseExisted ("/tmp/english-user.db");
        g_assert (!retval);
        retval = db->createDatabase ("english-user.db");
        g_assert (retval);
        retval = db->openDatabase ("english.db", "english-user.db");
        g_assert (retval);
        float freq = 0;
        retval = db->getWordInfo ("hello", freq);
        printf ("word hello:%d, %f.\n", retval, freq);
        if (retval) {
            db->updateWord ("hello", 0.1);
        } else {
            db->insertWord ("hello", 0.1);
        }
        printf ("english database test ok.\n");
    }
} test_english_database;

#endif
};
