/* vim:set et ts=4 sts=4:
 *
 * ibus-libpinyin - Intelligent Pinyin engine based on libpinyin for IBus
 *
 * Copyright (c) 2018 linyu Xu <liannaxu07@gmail.com>
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
    m_cloud_candidates_number = 1;
    m_first_cloud_candidate_position = 3;
    m_min_cloud_trigger_length = 2;
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

    GInputStream *stream = soup_session_send (m_session, msg, NULL, error);

    simpleProcessCloudResponse(stream, candidates);
}

void
CloudCandidates::simpleProcessCloudResponse (GInputStream *stream, std::vector<EnhancedCandidate> & candidates)
{
    GError **error = NULL;
    gchar buffer[BUFFERLENGTH];
    int length = g_input_stream_read (stream, buffer, BUFFERLENGTH, NULL, error);

    while (length < 50)
    {
        printf("Retrying\n");
        length += g_input_stream_read (stream, buffer + length, BUFFERLENGTH, NULL, error);
    }

    buffer[length] = '\0';

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
}

void
CloudCandidates::processCloudResponse (GInputStream *stream, std::vector<EnhancedCandidate> & candidates)
{
    GError **error = NULL;
    JsonParser *parser = json_parser_new();
    JsonNode *root;
    guint result_counter = 0;

    if (json_parser_load_from_stream(parser, stream, NULL, error) && error == NULL)
    {
        g_object_unref(parser);
        return;
    }

    root = json_parser_get_root(parser);
    if (m_cloud_source == BAIDU && JSON_NODE_TYPE(root) == JSON_NODE_OBJECT)
    {
        JsonObject *baidu_root_object = json_node_get_object(root);
        const gchar *baidu_response_status;
        JsonArray *baidu_result_array;
        JsonArray *baidu_candidate_array;
        const gchar *baidu_candidate_annotation;

        if (!json_object_has_member(baidu_root_object, "status"))
        {
            g_object_unref(parser);
            return;
        }
        
        baidu_response_status = json_object_get_string_member(baidu_root_object, "status");

        if (g_strcmp0(baidu_response_status, "T"))
        {
            g_object_unref(parser);
            return;
        }

        baidu_result_array = json_object_get_array_member(baidu_root_object, "result");

        baidu_candidate_array = json_array_get_array_element(baidu_result_array, 0);
        baidu_candidate_annotation = json_array_get_string_element(baidu_result_array, 1);

        result_counter = json_array_get_length(baidu_candidate_array);

        for(int i = 0; i < result_counter; ++i)
        {
            m_candidates[i].m_display_string = json_array_get_string_element(baidu_candidate_array, i);
        }
    }
    else if (m_cloud_source == GOOGLE && JSON_NODE_TYPE(root) == JSON_NODE_ARRAY)
    {
        /**
         * 
         */

        JsonArray *google_root_array = json_node_get_array(root);
        
        const gchar *google_response_status;
        JsonArray *google_response_array;
        JsonArray *google_result_array;
        const gchar *google_candidate_annotation;
        JsonArray *google_candidate_array;

        if (json_array_get_length(google_root_array) <= 1)
        {
            g_object_unref(parser);
            return;
        }
            
        google_response_status = json_array_get_string_element(google_root_array, 0);

        if (g_strcmp0(google_response_status, "SUCCESS"))
        {
            g_object_unref(parser);
            return;
        }
        
        google_response_array = json_array_get_array_element(google_root_array, 1);

        if (json_array_get_length(google_root_array) < 1)
        {
            g_object_unref(parser);
            return;
        }

        google_result_array = json_array_get_array_element(google_response_array, 0);

        google_candidate_annotation = json_array_get_string_element(google_result_array, 0);

        google_candidate_array = json_array_get_array_element(google_result_array, 1);

        result_counter = json_array_get_length(google_candidate_array);

        for(int i = 0; i < result_counter; ++i)
        {
            m_candidates[i].m_display_string = json_array_get_string_element(google_candidate_array, i);
        }
    }

    /*
        if (JSON_NODE_TYPE(root) == JSON_NODE_ARRAY) {

            
            
        } else {
            puts("Baidu");
            JsonObject *object = json_node_get_object(root);

            // Do a type validation
            if (json_object_has_member(object, "status")) {
                const gchar *baidu_response_status = json_object_get_string_member(object, "status");
                g_print("Status: %s\n", baidu_response_status);

                if (!g_strcmp0(baidu_response_status, "T")) {
                    JsonArray *baidu_result_array = json_object_get_array_member(object, "result");
                    g_print("Array length: %d\n", json_array_get_length(baidu_result_array));

                    JsonArray *baidu_candidate_array = json_array_get_array_element(baidu_result_array, 0);
                    const gchar *baidu_candidate_annotation = json_array_get_string_element(baidu_result_array, 1);

                    g_print("Get %d candidates from %s\n\n", json_array_get_length(baidu_candidate_array), baidu_candidate_annotation);

                } else {
                    puts("Status is not equals to T\n");
                }
            }
        }
    } else {
        puts("Load failed\n");
    }
    */

    for (guint i = 0; i < m_cloud_candidates_number && i < result_counter; ++i) {
        EnhancedCandidate & enhanced = candidates[i + m_first_cloud_candidate_position - 1];
        enhanced.m_display_string = m_candidates[i].m_display_string;
        enhanced.m_candidate_type = CANDIDATE_CLOUD_INPUT;
        m_candidates.push_back(enhanced);
    }

    g_object_unref(parser);
}


