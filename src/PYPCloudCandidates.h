/* vim:set et ts=4 sts=4:
 *
 * ibus-libpinyin - Intelligent Pinyin engine based on libpinyin for IBus
 *
 * Copyright (c) 2018 linyu Xu <liannaxu07@gmail.com>
 * Copyright (c) 2020 Weixuan XIAO <veyx.shaw@gmail.com>
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

#ifndef __PY_LIB_PINYIN_ClOUD_CANDIDATES_H_
#define __PY_LIB_PINYIN_ClOUD_CANDIDATES_H_

#include "PYString.h"
#include "PYPointer.h"
#include "PYPEnhancedCandidates.h"
#include <vector>
#include <libsoup/soup.h>
#include <json-glib/json-glib.h>
#include "PYConfig.h"



class BaiduCloudCandidatesResponseJsonParser;
class GoogleCloudCandidatesResponseJsonParser;

namespace PY {

#define BUFFERLENGTH 2048
#define CLOUD_MINIMUM_UTF8_TRIGGER_LENGTH 2

enum InputMode {
    FullPinyin = 0,
    DoublePinyin,
    Bopomofo
};

class PhoneticEditor;

class CloudCandidates : public EnhancedCandidates<PhoneticEditor>
{
public:

    CloudCandidates (PhoneticEditor *editor);
    ~CloudCandidates();

    void setInputMode (InputMode mode) { m_input_mode = mode; }

    gboolean processCandidates (std::vector<EnhancedCandidate> & candidates);

    int selectCandidate (EnhancedCandidate & enhanced);

    void cloudAsyncRequest (const gchar* requestStr);
    void cloudSyncRequest (const gchar* requestStr, std::vector<EnhancedCandidate> & candidates);

    void delayedCloudAsyncRequest (const gchar* requestStr);

    void updateLookupTable ();

    guint m_source_event_id;
    SoupMessage *m_message;
    std::string m_last_requested_pinyin;

private:
    static gboolean delayedCloudAsyncRequestCallBack (gpointer user_data);
    static void delayedCloudAsyncRequestDestroyCallBack (gpointer user_data);
    static void cloudResponseCallBack (GObject *object, GAsyncResult *result, gpointer user_data);

    void processCloudResponse (GInputStream *stream, std::vector<EnhancedCandidate> & candidates);

    /* get internal full pinyin representation */
    String getFullPinyin ();
private:
    SoupSession *m_session;
    InputMode m_input_mode;

    BaiduCloudCandidatesResponseJsonParser *m_baidu_parser;
    GoogleCloudCandidatesResponseJsonParser *m_google_parser;
protected:
    std::vector<EnhancedCandidate> m_candidates;
};

};

#endif


