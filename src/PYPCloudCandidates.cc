/* vim:set et ts=4 sts=4:
 *
 * ibus-libpinyin - Intelligent Pinyin engine based on libpinyin for IBus
 *
 * Copyright (c) 2018
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

#include "PYPCloudCandidates.h"
#include "PYString.h"
#include "PYConfig.h"
#include <assert.h>
#include <pinyin.h>
#include "PYPPhoneticEditor.h"


using namespace PY;

CloudCandidates::CloudCandidates (PhoneticEditor * editor)
{
    m_session = soup_session_new ();
    m_editor = editor;

    m_cloud_state = m_editor->m_config.enableCloudInput ();
    m_cloud_source = m_editor->m_config.cloudInputSource ();
    m_cloud_candidates_number = m_editor->m_config.cloudCandidatesNumber ();
    m_first_cloud_candidate_position = m_editor->m_config.firstCloudCandidatePos ();
    m_min_cloud_trigger_length = m_editor->m_config.minCloudInputTriggerLen ();
    m_cloud_flag = FALSE;
}

CloudCandidates::~CloudCandidates ()
{
}

gboolean
CloudCandidates::processCandidates (std::vector<EnhancedCandidate> & candidates)
{

    EnhancedCandidate testCan = candidates[m_first_cloud_candidate_position-1];
    /*have cloud candidates already*/
    if (testCan.m_candidate_type == CANDIDATE_CLOUD_INPUT)
        return FALSE;


    /* insert cloud candidates' placeholders */
    m_candidates.clear ();
    for (guint i = 0; i != m_cloud_candidates_number; i++) {
        EnhancedCandidate  enhanced;
        enhanced.m_display_string = "...";
        enhanced.m_candidate_type = CANDIDATE_CLOUD_INPUT;
        m_candidates.push_back (enhanced);
    }
    candidates.insert (candidates.begin ()+m_first_cloud_candidate_position-1, m_candidates.begin(), m_candidates.end());

    if (! m_editor->m_config.doublePinyin ())
    {
        const gchar *text = m_editor->m_text;
        if (strlen (text) >= m_min_cloud_trigger_length)
            cloudSyncRequest (text, candidates);
    }
    else
    {
        m_editor->updateAuxiliaryText ();
        String stripped = m_editor->m_buffer;
        const gchar *temp= stripped;
        gchar** tempArray =  g_strsplit_set (temp, " |", -1);
        gchar *text=g_strjoinv ("",tempArray);

        if (strlen (text) >= m_min_cloud_trigger_length)
            cloudSyncRequest (text, candidates);

        g_strfreev (tempArray);
        g_free (text);
    }

    return TRUE;
}

int
CloudCandidates::selectCandidate (EnhancedCandidate & enhanced)
{
    assert (CANDIDATE_CLOUD_INPUT == enhanced.m_candidate_type);

    if (enhanced.m_display_string == "...")
        return SELECT_CANDIDATE_ALREADY_HANDLED;

    return SELECT_CANDIDATE_COMMIT;
}

void
CloudCandidates::cloudAsyncRequest (const gchar* requestStr, std::vector<EnhancedCandidate> & candidates)
{
    GError **error = NULL;
    gchar *queryRequest;
    if (m_cloud_source == BAIDU)
        queryRequest= g_strdup_printf("http://olime.baidu.com/py?input=%s&inputtype=py&bg=0&ed=%d&result=hanzi&resultcoding=utf-8&ch_en=1&clientinfo=web&version=1",requestStr, m_cloud_candidates_number);
    else if (m_cloud_source == GOOGLE)
        queryRequest= g_strdup_printf("https://www.google.com/inputtools/request?ime=pinyin&text=%s",requestStr);
    SoupMessage *msg = soup_message_new ("GET", queryRequest);
    soup_session_send_async (m_session, msg, NULL, cloudResponseCallBack, static_cast<gpointer> (this));
}

void
CloudCandidates::cloudResponseCallBack (GObject *source_object, GAsyncResult *result, gpointer user_data)
{
    GError **error = NULL;
    GInputStream *stream = soup_session_send_finish (SOUP_SESSION(source_object), result, error);

    gchar buffer[BUFFERLENGTH];
    error = NULL;
    g_input_stream_read (stream, buffer, BUFFERLENGTH, NULL, error);
    CloudCandidates *cloudCandidates = static_cast<CloudCandidates *> (user_data);

    String res;
    res.clear ();
    res.append (buffer);

    if (res)
    {
        if (cloudCandidates->m_cloud_source == BAIDU)
        {
            /*BAIDU */
            if (res[11]=='T')
            {
                if (res[49] !=']')
                {   
                    /*respond true , with results*/
                    gchar **resultsArr = g_strsplit(res.data()+49, "],", 0);
                    guint resultsArrLength = g_strv_length(resultsArr);
                    for(int i = 0; i != resultsArrLength-1; ++i)
                    {
                        int end =strcspn(resultsArr[i], ",");
                        std::string tmp = g_strndup(resultsArr[i]+2,end-3);
                        cloudCandidates->m_candidates[i].m_display_string = tmp;
                    }
                }
            }
        }
        else if (cloudCandidates->m_cloud_source == GOOGLE)
        {
            /*GOOGLE */
            const gchar *tmp_res = res;
            const gchar *prefix = "[\"SUCCESS\"";
            if (g_str_has_prefix (tmp_res, prefix))
            {
                gchar **prefix_arr = g_strsplit (tmp_res, "\",[\"", -1);
                gchar *prefix_str = prefix_arr[1];
                gchar **suffix_arr = g_strsplit (prefix_str, "\"],", -1);
                std::string tmp = suffix_arr[0];
                cloudCandidates->m_candidates[0].m_display_string = tmp;
                g_strfreev (prefix_arr);
                g_strfreev (suffix_arr);
            }
        }
    }

    cloudCandidates->m_editor->update ();

}

void
CloudCandidates::cloudSyncRequest (const gchar* requestStr, std::vector<EnhancedCandidate> & candidates)
{
    GError **error = NULL;
    gchar *queryRequest;
    if (m_cloud_source == BAIDU)
        queryRequest= g_strdup_printf ("http://olime.baidu.com/py?input=%s&inputtype=py&bg=0&ed=%d&result=hanzi&resultcoding=utf-8&ch_en=1&clientinfo=web&version=1",requestStr, m_cloud_candidates_number);
    else if (m_cloud_source == GOOGLE)
        queryRequest= g_strdup_printf ("https://www.google.com/inputtools/request?ime=pinyin&text=%s",requestStr);
    SoupMessage *msg = soup_message_new ("GET", queryRequest);
    guint status = soup_session_send_message (m_session, msg);

    SoupMessageBody *msgBody =soup_message_body_new ();
    soup_message_body_truncate (msgBody);
    msgBody = msg->response_body;
    /* clear useless characters */
    soup_message_body_flatten(msgBody);
    SoupBuffer *bufferBody= soup_message_body_get_chunk(msgBody, 0);

    const gchar *buffer= bufferBody->data;
    String res;
    res.clear ();
    res.append (buffer);

    if (res)
    {
        if (m_cloud_source == BAIDU)
        {
            /*BAIDU */
            if (res[11] == 'T')
            {
                if (res[49] != ']')
                {
                    /*respond true , with results*/
                    gchar **resultsArr = g_strsplit (res.data () + 49, "],", 0);
                    guint resultsArrLength = g_strv_length (resultsArr);
                    for (int i = 0; i != resultsArrLength - 1; ++i)
                    {
                        int end =strcspn (resultsArr[i], ",");
                        std::string tmp = g_strndup (resultsArr[i]+2, end-3);
                        m_candidates[i].m_display_string = tmp;
                    }
                }
            }
        }
        else if (m_cloud_source == GOOGLE)
        {
            /*GOOGLE */
            const gchar *tmp_res = res;
            const gchar *prefix = "[\"SUCCESS\"";
            if (g_str_has_prefix (tmp_res, prefix))
            {
                gchar **prefix_arr = g_strsplit (tmp_res, "\",[\"", -1);
                gchar *prefix_str = prefix_arr[1];
                gchar **suffix_arr = g_strsplit (prefix_str, "\"],", -1);
                std::string tmp = suffix_arr[0];
                m_candidates[0].m_display_string = tmp;
                g_strfreev (prefix_arr);
                g_strfreev (suffix_arr);
            }
        }
    }

    for (guint i = 0; i != m_cloud_candidates_number; i++) {
        EnhancedCandidate & enhanced = candidates[i + m_first_cloud_candidate_position - 1];
        enhanced.m_display_string = m_candidates[i].m_display_string;
        enhanced.m_candidate_type = CANDIDATE_CLOUD_INPUT;
        m_candidates.push_back(enhanced);
    }

    soup_message_body_free (msgBody);

}




