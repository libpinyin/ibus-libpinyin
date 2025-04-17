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

#include "PYPCloudCandidates.h"
#include "PYString.h"
#include "PYConfig.h"
#include "PYPPhoneticEditor.h"
#include "PYPPinyinEditor.h"
#include "PYLibPinyin.h"

#include <assert.h>
#include <pinyin.h>
#include <cstring>
#include <glib.h>


using namespace PY;

/* enable to choose the cloud candidate after 100ms. */
#define CANDIDATE_SENSITIVE_TIMEWAIT 0.1

enum CandidateResponseParserError {
    PARSER_NOERR,
    PARSER_INVALID_DATA,
    PARSER_BAD_FORMAT,
    PARSER_NO_CANDIDATE,
    PARSER_NETWORK_ERROR,
    PARSER_UNKNOWN
};

static const std::string CANDIDATE_CLOUD_PREFIX = "☁";

typedef struct
{
    guint event_id;
    SoupMessage *message;
    GCancellable *cancel_message;
    gchar requested_pinyin[MAX_PINYIN_LEN + 1];
    CloudCandidates *cloud_candidates;
} CloudAsyncRequestUserData;

class CloudCandidatesResponseParser
{
public:
    CloudCandidatesResponseParser (CloudInputSource input_source) :
        m_input_source (input_source) {}

    virtual ~CloudCandidatesResponseParser () {}

    virtual gchar *getRequestString (const gchar *pinyin, gint number) = 0;

    virtual guint parse (GInputStream *stream) = 0;

    virtual std::vector<std::string> &getStringCandidates () { return m_candidates; }

protected:
    std::vector<std::string> m_candidates;
    const CloudInputSource m_input_source;
};

class CloudCandidatesResponseJsonParser : public CloudCandidatesResponseParser
{
public:
    CloudCandidatesResponseJsonParser (CloudInputSource input_source) :
        CloudCandidatesResponseParser (input_source)
    {
        m_parser = json_parser_new ();
    }

    virtual ~CloudCandidatesResponseJsonParser () {
        /* free json parser object if necessary */
        if (m_parser) {
            g_object_unref (m_parser);
            m_parser = NULL;
        }
    }

    guint parse (GInputStream *stream)
    {
        GError **error = NULL;

        if (!stream)
            return PARSER_NETWORK_ERROR;

        /* parse Json from input steam */
        if (!json_parser_load_from_stream (m_parser, stream, NULL, error) || error != NULL) {
            g_input_stream_close (stream, NULL, error);  /* Close stream to release libsoup connection */
            return PARSER_BAD_FORMAT;
        }
        g_input_stream_close (stream, NULL, error);  /* Close stream to release libsoup connection */

        return parseJsonResponse (json_parser_get_root (m_parser));
    }

protected:
    JsonParser *m_parser;

    virtual guint parseJsonResponse (JsonNode *root) = 0;
};

class GoogleCloudCandidatesResponseJsonParser : public CloudCandidatesResponseJsonParser
{
protected:
    guint parseJsonResponse (JsonNode *root)
    {
        /* clear the last result */
        m_candidates.clear ();

        if (!JSON_NODE_HOLDS_ARRAY (root))
            return PARSER_BAD_FORMAT;

        /* validate Google source and the structure of response */
        JsonArray *google_root_array = json_node_get_array (root);

        /**
         * declare variables to refer to structures in the result
         * a typical cloud candidate response from Google is:
         * [                        <- google_root_array
         *  "SUCCESS",                  <- google_response_status
         *  [                           <- google_response_array
         *      [                           <- google_result_array
         *          "ceshi",                    <- google_candidate_annotation
         *          ["测试"],                    <- google_candidate_array
         *          [],
         *          {
         *              "annotation":["ce shi"],
         *              "candidate_type":[0],
         *              "lc":["16 16"]
         *          }
         *      ]                           <- google_result_array end
         *  ]                           <- google_response_array end
         * ]                        <- google_root_array end
         */
        const gchar *google_response_status;
        JsonArray *google_response_array;
        JsonArray *google_result_array;
        const gchar *google_candidate_annotation;
        JsonArray *google_candidate_array;
        guint result_counter;

        /* validate google_root_array length */
        if (json_array_get_length (google_root_array) <= 1)
            return PARSER_INVALID_DATA;

        /* get and validate google_response_status */
        google_response_status = json_array_get_string_element (google_root_array, 0);
        if (g_strcmp0 (google_response_status, "SUCCESS"))
            return PARSER_INVALID_DATA;

        /* get google_response_array */
        google_response_array = json_array_get_array_element (google_root_array, 1);

        /* validate google_response_array length */
        if (json_array_get_length (google_response_array) < 1)
            return PARSER_INVALID_DATA;

        /* get google_result_array */
        google_result_array = json_array_get_array_element (google_response_array, 0);

        /* get and validate google_candidate_annotation */
        google_candidate_annotation = json_array_get_string_element (google_result_array, 0);
        if (!google_candidate_annotation)
            return PARSER_INVALID_DATA;

        /* get google_candidate_array */
        google_candidate_array = json_array_get_array_element (google_result_array, 1);

        /* there should be at least one candidate */
        result_counter = json_array_get_length (google_candidate_array);
        if (result_counter < 1)
            return PARSER_NO_CANDIDATE;

        /* put all parsed candidates into m_candidates array of parser instance */
        for (guint i = 0; i < result_counter; ++i) {
            std::string candidate = json_array_get_string_element (google_candidate_array, i);
            m_candidates.push_back (candidate);
        }

        return PARSER_NOERR;
    }

public:
    gchar *getRequestString (const gchar *pinyin, gint number) {
        assert (m_input_source == CLOUD_INPUT_SOURCE_GOOGLE ||
                m_input_source == CLOUD_INPUT_SOURCE_GOOGLE_CN);

        const char *GOOGLE_URL_TEMPLATE = NULL;

        if (m_input_source == CLOUD_INPUT_SOURCE_GOOGLE)
            GOOGLE_URL_TEMPLATE = "https://www.google.com/inputtools/request?ime=pinyin&text=%s&num=%d";
        else if (m_input_source == CLOUD_INPUT_SOURCE_GOOGLE_CN)
            GOOGLE_URL_TEMPLATE = "https://www.google.cn/inputtools/request?ime=pinyin&text=%s&num=%d";

        return g_strdup_printf (GOOGLE_URL_TEMPLATE, pinyin, number);
    }

public:
    GoogleCloudCandidatesResponseJsonParser (CloudInputSource input_source) : CloudCandidatesResponseJsonParser (input_source) {}
};

class BaiduCloudCandidatesResponseJsonParser : public CloudCandidatesResponseJsonParser
{
private:
    guint parseJsonResponse (JsonNode *root)
    {
        /* clear the last result */
        m_candidates.clear ();

        if (!JSON_NODE_HOLDS_OBJECT (root))
            return PARSER_BAD_FORMAT;

        /* validate Baidu source and the structure of response */
        JsonObject *baidu_root_object = json_node_get_object (root);

        /**
         * declare variables to refer to structures in the result
         * a typical cloud candidate response from Baidu is:
         * {                <- baidu_root_object
         *  "errmsg":"",
         *  "errno":"0",
         *  "result":
         *      [               <- baidu_result_array
         *          [               <- baidu_candidate_array
         *              [               <- baidu_candidate
         *                  "测试",
         *                  5,
         *                  {
         *                      "pinyin":"ce'shi",
         *                      "type":"IMEDICT"
         *                  }
         *              ]               <- baidu_candidate end
         *          ],              <- baidu_candidate_array end
         *          "ce'shi"        <- baidu_candidate_annotation
         *      ],              <- baidu_result_array end
         *  "status":"T"        <- baidu_response_status
         * }                <- baidu_root_object end
         */
        const gchar *baidu_response_status;
        JsonArray *baidu_result_array;
        JsonArray *baidu_candidate_array;
        const gchar *baidu_candidate_annotation;
        guint result_counter;

        /* get and validate baidu_response_status */
        if (!json_object_has_member (baidu_root_object, "status"))
            return PARSER_INVALID_DATA;
        baidu_response_status = json_object_get_string_member (baidu_root_object, "status");
        if (g_strcmp0 (baidu_response_status, "T"))
            return PARSER_INVALID_DATA;

        /* get baidu_result_array */
        if (!json_object_has_member (baidu_root_object, "result"))
            return PARSER_INVALID_DATA;
        baidu_result_array = json_object_get_array_member (baidu_root_object, "result");

        /* get baidu_candidate_array and baidu_candidate_annotation */
        if (json_array_get_length (baidu_result_array) < 2)
            return PARSER_INVALID_DATA;

        baidu_candidate_array = json_array_get_array_element (baidu_result_array, 0);
        baidu_candidate_annotation = json_array_get_string_element (baidu_result_array, 1);

        /* validate baidu_candidate_annotation */
        if (!baidu_candidate_annotation)
            return PARSER_INVALID_DATA;

        /* there should be at least one candidate */
        result_counter = json_array_get_length (baidu_candidate_array);
        if (result_counter < 1)
            return PARSER_NO_CANDIDATE;

        /* visit all baidu_candidate */
        for (guint i = 0; i < result_counter; ++i) {
            std::string candidate;
            JsonArray *baidu_candidate = json_array_get_array_element (baidu_candidate_array, i);

            /* confirm the candidate exists in this baidu_candidate */
            if (json_array_get_length (baidu_candidate) < 1)
                break;
            else
                candidate = json_array_get_string_element (baidu_candidate, 0);

            /* put all parsed candidates into m_candidates array of parser instance */
            m_candidates.push_back (candidate);
        }

        return PARSER_NOERR;
    }

public:
    gchar *getRequestString (const gchar *pinyin, gint number) {
        assert (m_input_source == CLOUD_INPUT_SOURCE_BAIDU);

        const char *BAIDU_URL_TEMPLATE = "https://olime.baidu.com/py?input=%s&inputtype=py&bg=0&ed=%d&result=hanzi&resultcoding=utf-8&ch_en=1&clientinfo=web&version=1";

        return g_strdup_printf (BAIDU_URL_TEMPLATE, pinyin, number);
    }

public:
    BaiduCloudCandidatesResponseJsonParser (CloudInputSource input_source) : CloudCandidatesResponseJsonParser (input_source) {}
};

CloudCandidates::CloudCandidates (PhoneticEditor * editor) : m_input_mode(FullPinyin)
{
    m_session = soup_session_new ();
    m_editor = editor;

    m_source_event_id = 0;
    m_message = NULL;
    m_cancel_message = NULL;

    m_input_source = CLOUD_INPUT_SOURCE_BAIDU;
    m_parser = NULL;
    resetCloudResponseParser ();

    m_timer = g_timer_new ();
}

CloudCandidates::~CloudCandidates ()
{
    if (m_timer) {
        g_timer_destroy (m_timer);
        m_timer = NULL;
    }

    if (m_source_event_id != 0) {
        g_source_remove (m_source_event_id);
        m_source_event_id = 0;
    }

    if (m_message) {
        g_cancellable_cancel (m_cancel_message);
        m_message = NULL;
    }

    if (m_session) {
        g_object_unref (m_session);
        m_session = NULL;
    }

    if (m_parser) {
        delete m_parser;
        m_parser = NULL;
    }
}

void
CloudCandidates::resetCloudResponseParser ()
{
    CloudInputSource input_source = m_editor->m_config.cloudInputSource ();

    /* m_parser is initialized and not changed */
    if (m_parser && m_input_source == input_source)
        return;

    /* cloud input option is changed */
    if (m_parser) {
        delete m_parser;
        m_parser = NULL;
    }

    m_input_source = input_source;

    if (input_source == CLOUD_INPUT_SOURCE_BAIDU)
        m_parser = new BaiduCloudCandidatesResponseJsonParser (input_source);
    else if (input_source == CLOUD_INPUT_SOURCE_GOOGLE ||
             input_source == CLOUD_INPUT_SOURCE_GOOGLE_CN)
        m_parser = new GoogleCloudCandidatesResponseJsonParser (input_source);
}

gboolean
CloudCandidates::processCandidates (std::vector<EnhancedCandidate> & candidates)
{
    /* refer pinyin retrieved in full pinyin mode */
    String full_pinyin_text;

    /* find the first position after n-gram candidates */
    std::vector<EnhancedCandidate>::iterator pos, iter;

    /* check the length of candidates */
    if (0 == candidates.size ())
        return FALSE;   /* no candidate */

    const String & display_string = candidates[0].m_display_string;
    if (display_string.utf8Length () < CLOUD_MINIMUM_UTF8_TRIGGER_LENGTH) {
        m_last_requested_pinyin = "";
        return FALSE;   /* do not request because there is only one character */
    }

    resetCloudResponseParser ();

    /* cache the candidates in the first page */
    m_candidate_cache.clear ();

    /* search the first non-ngram candidate position */
    for (pos = candidates.begin (); pos != candidates.end (); ++pos) {
        if (CANDIDATE_NBEST_MATCH != pos->m_candidate_type &&
            CANDIDATE_LONGER != pos->m_candidate_type &&
            CANDIDATE_LONGER_USER != pos->m_candidate_type)
            break;
        m_candidate_cache.insert (pos->m_display_string);
    }

    for (iter = pos; iter != candidates.end (); ++iter) {
        /* only check the duplicated candidates in the first page */
        if (m_editor->m_config.pageSize () == m_candidate_cache.size ())
            break;

        /* skip the cloud candidate */
        if (CANDIDATE_CLOUD_INPUT == iter->m_candidate_type)
            continue;

        m_candidate_cache.insert (iter->m_display_string);
    }

    /* neither double pinyin mode nor bopomofo mode */
    if (m_input_mode == FullPinyin)
        full_pinyin_text = m_editor->m_text;
    else
        full_pinyin_text = getFullPinyin ();

    if (m_last_requested_pinyin == full_pinyin_text) {
        /* do not request again and update cached ones */

        for (int i = 0; i < m_candidates.size (); ++i){
            EnhancedCandidate candidate = m_candidates[i];
            std::string & display_string = candidate.m_display_string;

            std::set<std::string>::iterator iter =
                m_candidate_cache.find (display_string);

            /* skip the already existed candidate */
            if (iter != m_candidate_cache.end ())
                continue;

            /* insert cloud prefix */
            candidate.m_display_string = CANDIDATE_CLOUD_PREFIX + display_string;
            candidates.insert (pos, candidate);
            ++pos;
        }

        /* enable to choose cloud candidate after short period */
        g_timer_start (m_timer);
        return TRUE;
    }

    m_candidates.clear ();
    delayedCloudAsyncRequest (full_pinyin_text);

    return TRUE;
}

int
CloudCandidates::selectCandidate (EnhancedCandidate & enhanced)
{
    assert (CANDIDATE_CLOUD_INPUT == enhanced.m_candidate_type);

    if (g_timer_elapsed (m_timer, NULL) < CANDIDATE_SENSITIVE_TIMEWAIT)
        return SELECT_CANDIDATE_ALREADY_HANDLED;

    g_timer_stop (m_timer);

    if (enhanced.m_candidate_id < m_candidates.size ()) {
        enhanced.m_display_string =
            m_candidates[enhanced.m_candidate_id].m_display_string;

        /* remember the cloud input */
        if (m_editor->m_config.rememberEveryInput ())
            LibPinyinBackEnd::instance ().rememberCloudInput (m_editor->m_instance, m_last_requested_pinyin.c_str (), enhanced.m_display_string.c_str ());
        LibPinyinBackEnd::instance ().modified ();

        /* modify in-place and commit */
        return SELECT_CANDIDATE_COMMIT | SELECT_CANDIDATE_MODIFY_IN_PLACE;
    }

    return SELECT_CANDIDATE_ALREADY_HANDLED;
}

void
CloudCandidates::delayedCloudAsyncRequest (const gchar* pinyin)
{
    gpointer user_data;
    CloudAsyncRequestUserData *data;

    /* cancel the latest timer, if applied */
    if (m_source_event_id != 0)
        g_source_remove (m_source_event_id);

    /* allocate memory for a CloudAsyncRequestUserData instance to take more callback user data */
    user_data = g_malloc (sizeof(CloudAsyncRequestUserData));
    data = static_cast<CloudAsyncRequestUserData *> (user_data);

    strncpy (data->requested_pinyin, pinyin, MAX_PINYIN_LEN);
    data->requested_pinyin[MAX_PINYIN_LEN] = '\0';
    data->cloud_candidates = this;

    /* record the latest timer */
    m_source_event_id = g_timeout_add (m_editor->m_config.cloudRequestDelayTime (),
                                       delayedCloudAsyncRequestCallBack,
                                       user_data);
    data->event_id = m_source_event_id;
    data->message = NULL;
    data->cancel_message = NULL;
}

gboolean
CloudCandidates::delayedCloudAsyncRequestCallBack (gpointer user_data)
{
    CloudAsyncRequestUserData *data = static_cast<CloudAsyncRequestUserData *> (user_data);

    if (!data)
        return FALSE;

    CloudCandidates *cloud_candidates = data->cloud_candidates;

    if (!cloud_candidates)
        return FALSE;

    /* only send with a latest timer */
    if (data->event_id == cloud_candidates->m_source_event_id) {
        cloud_candidates->m_source_event_id = 0;
        cloud_candidates->cloudAsyncRequest (user_data);
    }

    return FALSE;
}

void
CloudCandidates::cloudAsyncRequest (gpointer user_data)
{
    guint number = m_editor->m_config.cloudCandidatesNumber ();

    CloudAsyncRequestUserData *data = static_cast<CloudAsyncRequestUserData *> (user_data);
    /* cache the last request string */
    m_last_requested_pinyin = data->requested_pinyin;

    gchar *query_request = m_parser->getRequestString (data->requested_pinyin, number);

    /* cancel message if there is a pending one */
    if (m_message) {
        g_cancellable_cancel (m_cancel_message);
        m_message = NULL;
    }

    m_cancel_message = g_cancellable_new ();
    data->cancel_message = m_cancel_message;

    m_message = soup_message_new ("GET", query_request);
    soup_session_send_async (m_session, m_message, SOUP_MESSAGE_PRIORITY_NORMAL, m_cancel_message, cloudResponseCallBack, user_data);
    data->message = m_message;

    /* free url string */
    if (query_request)
        g_free(query_request);
}

void
CloudCandidates::cloudResponseCallBack (GObject *source_object, GAsyncResult *result, gpointer user_data)
{
    GError *error = NULL;
    GInputStream *stream = soup_session_send_finish (SOUP_SESSION (source_object), result, &error);
    CloudAsyncRequestUserData *data = static_cast<CloudAsyncRequestUserData *> (user_data);

    CloudCandidates *cloud_candidates = data->cloud_candidates;

    if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
        /* process results */
        cloud_candidates->processCloudResponse (stream, cloud_candidates->m_editor->m_candidates, data->requested_pinyin);

        cloud_candidates->updateLookupTable ();

        /* reset m_message pointer only when it is not replaced with a new m_message */
        cloud_candidates->m_message = NULL;
        cloud_candidates->m_cancel_message = NULL;
    }

    if (error) {
        g_error_free (error);
    } else {
        g_object_unref (stream);
    }

    /* clean up message */
    g_object_unref (data->message);

    /* clean up cancellable */
    g_object_unref (data->cancel_message);

    g_free (user_data);
}

void
CloudCandidates::cloudSyncRequest (const gchar* pinyin, std::vector<EnhancedCandidate> & candidates)
{
    guint number = m_editor->m_config.cloudCandidatesNumber ();
    gchar *query_request = m_parser->getRequestString (pinyin, number);
    SoupMessage *msg = soup_message_new ("GET", query_request);

    GInputStream *stream = soup_session_send (m_session, msg, NULL, NULL);
    processCloudResponse (stream, candidates, pinyin);

    /* free msg */
    g_object_unref (msg);
    /* free url string */
    if (query_request)
        g_free(query_request);
}

gboolean
CloudCandidates::processCloudResponse (GInputStream *stream, std::vector<EnhancedCandidate> & candidates, const gchar * requested_pinyin)
{
    guint retval = m_parser->parse (stream);

    m_candidates.clear ();

    if (retval != PARSER_NOERR)
        return FALSE;

    if (m_last_requested_pinyin == requested_pinyin) {
        /* update to the cached candidates list */
        std::vector<std::string> & string_candidates =
            m_parser->getStringCandidates ();

        for (guint i = 0; i < string_candidates.size (); ++i) {
            EnhancedCandidate candidate;
            /* insert candidate without prefix in m_candidates */
            candidate.m_candidate_type = CANDIDATE_CLOUD_INPUT;
            candidate.m_candidate_id = i;
            candidate.m_display_string = string_candidates[i];
            m_candidates.push_back (candidate);
        }
        return TRUE;
    }
    return FALSE;
}

void
CloudCandidates::updateLookupTable ()
{
    LookupTable & lookup_table = m_editor->m_lookup_table;
    /* retrieve cursor position in lookup table */
    guint cursor = lookup_table.cursorPos ();

    /* update cached cloud input candidates */
    m_editor->updateCandidates ();

    /* regenerate lookup table */
    lookup_table.clear ();
    m_editor->fillLookupTable ();

    /* recover cursor position in lookup table */
    if (cursor < lookup_table.size ())
        lookup_table.setCursorPos (cursor);

    /* notify ibus */
    if (lookup_table.size ())
        m_editor->updateLookupTableFast ();
    else
        m_editor->hideLookupTable ();
}

String
CloudCandidates::getFullPinyin ()
{
    String buffer;

    gchar * aux_text                = NULL;
    gchar * pinyin_text             = NULL;
    gchar * pinyin_text_with_quote  = NULL;
    gchar** tempArray               = NULL;

    /* get full pinyin auxiliary text */
    pinyin_get_full_pinyin_auxiliary_text (m_editor->m_instance, m_editor->m_cursor, &aux_text);

    /* remove tone and cursor */
    tempArray =  g_strsplit_set (aux_text, "|12345", -1);
    pinyin_text = g_strjoinv ("", tempArray);
    g_strfreev (tempArray);

    /* remove space */
    pinyin_text = g_strstrip(pinyin_text);

    /* replace space with quote */
    tempArray =  g_strsplit_set (pinyin_text, " ", -1);
    pinyin_text_with_quote = g_strjoinv ("'", tempArray);
    buffer << pinyin_text_with_quote;
    g_strfreev (tempArray);

    /* free */
    g_free(aux_text);
    g_free(pinyin_text);
    g_free(pinyin_text_with_quote);

    return buffer;
}
