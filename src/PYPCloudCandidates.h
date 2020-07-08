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



namespace PY {

#define BUFFERLENGTH 2048
#define CLOUD_MINIMUM_TRIGGER_LENGTH 2
#define CLOUD_MINIMUM_UTF8_TRIGGER_LENGTH 2

class PhoneticEditor;

class CloudCandidates : public EnhancedCandidates<PhoneticEditor>
{
public:

    CloudCandidates (PhoneticEditor *editor);
    ~CloudCandidates();

    gboolean processCandidates (std::vector<EnhancedCandidate> & candidates);

    int selectCandidate (EnhancedCandidate & enhanced);

    void cloudAsyncRequest (const gchar* requestStr);
    void cloudAsyncRequest (const gchar* requestStr, std::vector<EnhancedCandidate> & candidates);
    void cloudSyncRequest (const gchar* requestStr, std::vector<EnhancedCandidate> & candidates);

    void delayedCloudAsyncRequest (const gchar* requestStr);

    void updateLookupTable ();

    guint m_cloud_source;
    guint m_cloud_candidates_number;
    guint m_delayed_time;
    guint m_source_event_id;
    SoupMessage *m_message;
    gchar *m_last_requested_pinyin;
private:
    static gboolean delayedCloudAsyncRequestCallBack (gpointer user_data);
    static void delayedCloudAsyncRequestDestroyCallBack (gpointer user_data);
    static void cloudResponseCallBack (GObject *object, GAsyncResult *result, gpointer user_data);

    void processCloudResponse (GInputStream *stream, std::vector<EnhancedCandidate> & candidates);
private:
    SoupSession *m_session;

protected:
    std::vector<EnhancedCandidate> m_candidates;
};

};

#endif


