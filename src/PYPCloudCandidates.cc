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

#include <assert.h>
#include <pinyin.h>
#include <cstring>
#include <glib.h>


using namespace PY;

enum CandidateResponseParserError {
    PARSER_NOERR,
    PARSER_INVALID_DATA,
    PARSER_BAD_FORMAT,
    PARSER_NO_CANDIDATE,
    PARSER_NETWORK_ERROR,
    PARSER_UNKNOWN
};

static const std::string CANDIDATE_CLOUD_PREFIX = "☁";

static const std::string CANDIDATE_PENDING_TEXT_WITHOUT_PREFIX      = "[⏱]";
static const std::string CANDIDATE_LOADING_TEXT_WITHOUT_PREFIX      = "...";
static const std::string CANDIDATE_NO_CANDIDATE_TEXT_WITHOUT_PREFIX = "[🚫]";
static const std::string CANDIDATE_INVALID_DATA_TEXT_WITHOUT_PREFIX = "[❌]";
static const std::string CANDIDATE_BAD_FORMAT_TEXT_WITHOUT_PREFIX   = "[❓]";

static const std::string CANDIDATE_PENDING_TEXT         = CANDIDATE_CLOUD_PREFIX + CANDIDATE_PENDING_TEXT_WITHOUT_PREFIX;
static const std::string CANDIDATE_LOADING_TEXT         = CANDIDATE_CLOUD_PREFIX + CANDIDATE_LOADING_TEXT_WITHOUT_PREFIX;
static const std::string CANDIDATE_NO_CANDIDATE_TEXT    = CANDIDATE_CLOUD_PREFIX + CANDIDATE_NO_CANDIDATE_TEXT_WITHOUT_PREFIX;
static const std::string CANDIDATE_INVALID_DATA_TEXT    = CANDIDATE_CLOUD_PREFIX + CANDIDATE_INVALID_DATA_TEXT_WITHOUT_PREFIX ;
static const std::string CANDIDATE_BAD_FORMAT_TEXT      = CANDIDATE_CLOUD_PREFIX + CANDIDATE_BAD_FORMAT_TEXT_WITHOUT_PREFIX;

static const char *BAIDU_URL_TEMPLATE = "http://olime.baidu.com/py?input=%s&inputtype=py&bg=0&ed=%d&result=hanzi&resultcoding=utf-8&ch_en=1&clientinfo=web&version=1";
static const char *GOOGLE_URL_TEMPLATE = "https://www.google.com/inputtools/request?ime=pinyin&text=%s&num=%d";

typedef struct
{
    guint event_id;
    gchar request_str[MAX_PINYIN_LEN + 1];
    CloudCandidates *cloud_candidates;
} DelayedCloudAsyncRequestCallbackUserData;

class CloudCandidatesResponseParser
{
public:
    CloudCandidatesResponseParser () : m_annotation (NULL) {}
    virtual ~CloudCandidatesResponseParser () {}

    virtual guint parse (GInputStream *stream) = 0;

    virtual std::vector<std::string> &getStringCandidates () { return m_candidates; }
    virtual const gchar *getAnnotation () { return m_annotation; }

protected:
    std::vector<std::string> m_candidates;
    const gchar *m_annotation;
};

class CloudCandidatesResponseJsonParser : public CloudCandidatesResponseParser
{
public:
    CloudCandidatesResponseJsonParser ();
    virtual ~CloudCandidatesResponseJsonParser ();

    guint parse (GInputStream *stream)
    {
        GError **error = NULL;

        if (!stream)
            return PARSER_NETWORK_ERROR;

        /* parse Json from input steam */
        if (!json_parser_load_from_stream (m_parser, stream, NULL, error) || error != NULL) {
            g_input_stream_close (stream, NULL, error);  /* Close stream to release libsoup connexion */
            return PARSER_BAD_FORMAT;
        }
        g_input_stream_close (stream, NULL, error);  /* Close stream to release libsoup connexion */

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

        /* update annotation with the parsed annotation */
        m_annotation = google_candidate_annotation;

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
    GoogleCloudCandidatesResponseJsonParser () : CloudCandidatesResponseJsonParser () {}
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

        /* update annotation with the returned annotation */
        m_annotation = baidu_candidate_annotation;

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
                candidate = CANDIDATE_INVALID_DATA_TEXT_WITHOUT_PREFIX;
            else
                candidate = json_array_get_string_element (baidu_candidate, 0);

            /* put all parsed candidates into m_candidates array of parser instance */
            m_candidates.push_back (candidate);
        }

        return PARSER_NOERR;
    }

public:
    BaiduCloudCandidatesResponseJsonParser () : CloudCandidatesResponseJsonParser () {}
    ~BaiduCloudCandidatesResponseJsonParser () { if (m_annotation) g_free ((gpointer)m_annotation); }
};

gboolean
CloudCandidates::delayedCloudAsyncRequestCallBack (gpointer user_data)
{
    DelayedCloudAsyncRequestCallbackUserData *data = static_cast<DelayedCloudAsyncRequestCallbackUserData *> (user_data);
    CloudCandidates *cloudCandidates;

    if (!data)
        return FALSE;

    cloudCandidates = data->cloud_candidates;

    if (!cloudCandidates)
        return FALSE;

    /* only send with a latest timer */
    if (data->event_id == cloudCandidates->m_source_event_id) {
        cloudCandidates->m_source_event_id = 0;
        cloudCandidates->cloudAsyncRequest (data->request_str);
    }

    return FALSE;
}

void
CloudCandidates::delayedCloudAsyncRequestDestroyCallBack (gpointer user_data)
{
    /* clean up */
    if (user_data)
        g_free (user_data);
}

CloudCandidates::CloudCandidates (PhoneticEditor * editor) : m_input_mode(FullPinyin)
{
    m_session = soup_session_new ();
    m_editor = editor;

    m_source_event_id = 0;
    m_message = NULL;

    m_baidu_parser = new BaiduCloudCandidatesResponseJsonParser ();
    m_google_parser = new GoogleCloudCandidatesResponseJsonParser ();
}

CloudCandidates::~CloudCandidates ()
{
    if (m_source_event_id != 0) {
        g_source_remove (m_source_event_id);
        m_source_event_id = 0;
    }

    if (m_message) {
        soup_session_cancel_message (m_session, m_message, SOUP_STATUS_CANCELLED);
        m_message = NULL;
    }

    if (m_session) {
        g_object_unref (m_session);
        m_session = NULL;
    }

    if (m_baidu_parser) {
        delete m_baidu_parser;
        m_baidu_parser = NULL;
    }

    if (m_google_parser) {
        delete m_google_parser;
        m_google_parser = NULL;
    }
}

gboolean
CloudCandidates::processCandidates (std::vector<EnhancedCandidate> & candidates)
{
    /* refer pinyin retrieved in full pinyin mode */
    String full_pinyin_text;

    /* find the first position after n-gram candidates */
    std::vector<EnhancedCandidate>::iterator first_pos;

    /* check the length of candidates */
    if (0 == candidates.size ())
        return FALSE;   /* no candidate */

    const String & display_string = candidates[0].m_display_string;
    if (display_string.utf8Length () < CLOUD_MINIMUM_UTF8_TRIGGER_LENGTH) {
        m_last_requested_pinyin = "";
        return FALSE;   /* do not request because there is only one character */
    }

    /* search the first non-ngram candidate */
    for (first_pos = candidates.begin (); first_pos != candidates.end (); ++first_pos) {
        if (CANDIDATE_NBEST_MATCH != first_pos->m_candidate_type)
            break;
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
            /* insert cloud prefix */
            candidate.m_display_string = CANDIDATE_CLOUD_PREFIX + candidate.m_display_string;
            candidates.insert (first_pos + i, candidate);
        }
        return TRUE;
    }

    /* have cloud candidates already */
    if (first_pos->m_candidate_type == CANDIDATE_CLOUD_INPUT)
        return FALSE;

    /* insert cloud candidates' placeholders */
    m_candidates.clear ();
    for (guint i = 0; i < m_editor->m_config.cloudCandidatesNumber (); ++i) {
        EnhancedCandidate enhanced;
        enhanced.m_candidate_id = i;
        enhanced.m_display_string = CANDIDATE_PENDING_TEXT;
        enhanced.m_candidate_type = CANDIDATE_CLOUD_INPUT;
        m_candidates.push_back (enhanced);
    }
    candidates.insert (first_pos, m_candidates.begin (), m_candidates.end ());

    delayedCloudAsyncRequest (full_pinyin_text);

    return TRUE;
}

int
CloudCandidates::selectCandidate (EnhancedCandidate & enhanced)
{
    assert (CANDIDATE_CLOUD_INPUT == enhanced.m_candidate_type);

    if (enhanced.m_display_string == CANDIDATE_PENDING_TEXT ||
        enhanced.m_display_string == CANDIDATE_LOADING_TEXT ||
        enhanced.m_display_string == CANDIDATE_BAD_FORMAT_TEXT ||
        enhanced.m_display_string == CANDIDATE_INVALID_DATA_TEXT)
        return SELECT_CANDIDATE_ALREADY_HANDLED;

    if (enhanced.m_candidate_id < m_candidates.size ()) {
        enhanced.m_display_string =
            m_candidates[enhanced.m_candidate_id].m_display_string;

        /* modify in-place and commit */
        return SELECT_CANDIDATE_COMMIT | SELECT_CANDIDATE_MODIFY_IN_PLACE;
    }

    return SELECT_CANDIDATE_ALREADY_HANDLED;
}

void
CloudCandidates::delayedCloudAsyncRequest (const gchar* request_str)
{
    gpointer user_data;
    DelayedCloudAsyncRequestCallbackUserData *data;

    /* cancel the latest timer, if applied */
    if (m_source_event_id != 0)
        g_source_remove (m_source_event_id);

    /* allocate memory for a DelayedCloudAsyncRequestCallbackUserData instance to take more callback user data */
    user_data = g_malloc (sizeof(DelayedCloudAsyncRequestCallbackUserData));
    data = static_cast<DelayedCloudAsyncRequestCallbackUserData *> (user_data);

    strncpy (data->request_str, request_str, MAX_PINYIN_LEN);
    data->request_str[MAX_PINYIN_LEN] = '\0';
    data->cloud_candidates = this;

    /* record the latest timer */
    m_source_event_id = g_timeout_add_full (G_PRIORITY_DEFAULT,
                                            m_editor->m_config.cloudRequestDelayTime (),
                                            delayedCloudAsyncRequestCallBack,
                                            user_data,
                                            delayedCloudAsyncRequestDestroyCallBack);
    data->event_id = m_source_event_id;
}

void
CloudCandidates::cloudAsyncRequest (const gchar* request_str)
{
    GError **error = NULL;
    gchar *query_request;
    guint cloud_source = m_editor->m_config.cloudInputSource ();
    guint cloud_candidates_number = m_editor->m_config.cloudCandidatesNumber ();

    if (cloud_source == BAIDU)
        query_request= g_strdup_printf (BAIDU_URL_TEMPLATE, request_str, cloud_candidates_number);
    else if (cloud_source == GOOGLE)
        query_request= g_strdup_printf (GOOGLE_URL_TEMPLATE, request_str, cloud_candidates_number);

    /* cancel message if there is a pending one */
    if (m_message)
        soup_session_cancel_message (m_session, m_message, SOUP_STATUS_CANCELLED);

    SoupMessage *msg = soup_message_new ("GET", query_request);
    soup_session_send_async (m_session, msg, NULL, cloudResponseCallBack, static_cast<gpointer> (this));
    m_message = msg;

    /* cache the last request string */
    m_last_requested_pinyin = request_str;

    /* update loading text to replace pending text */
    for (std::vector<EnhancedCandidate>::iterator pos = m_candidates.begin (); pos != m_candidates.end (); ++pos) {
        pos->m_display_string = CANDIDATE_LOADING_TEXT_WITHOUT_PREFIX;
    }

    /* only update lookup table when there is still pinyin text */
    if (m_editor->m_text.utf8Length () >= CLOUD_MINIMUM_UTF8_TRIGGER_LENGTH)
        updateLookupTable ();

    /* free url string */
    if (query_request)
        g_free(query_request);
}

void
CloudCandidates::cloudResponseCallBack (GObject *source_object, GAsyncResult *result, gpointer user_data)
{
    GError **error = NULL;
    GInputStream *stream = soup_session_send_finish (SOUP_SESSION (source_object), result, error);
    CloudCandidates *cloudCandidates = static_cast<CloudCandidates *> (user_data);

    /* process results */
    cloudCandidates->processCloudResponse (stream, cloudCandidates->m_editor->m_candidates);

    /* only update lookup table when there is still pinyin text */
    if (cloudCandidates->m_editor->m_text.utf8Length () >=
        CLOUD_MINIMUM_UTF8_TRIGGER_LENGTH) {
        cloudCandidates->updateLookupTable ();

        /* clean up message */
        cloudCandidates->m_message = NULL;
    }
}

void
CloudCandidates::cloudSyncRequest (const gchar* request_str, std::vector<EnhancedCandidate> & candidates)
{
    GError **error = NULL;
    gchar *queryRequest;
    guint cloud_source = m_editor->m_config.cloudInputSource ();
    guint cloud_candidates_number = m_editor->m_config.cloudCandidatesNumber ();

    if (cloud_source == BAIDU)
        queryRequest= g_strdup_printf (BAIDU_URL_TEMPLATE, request_str, cloud_candidates_number);
    else if (cloud_source == GOOGLE)
        queryRequest= g_strdup_printf (GOOGLE_URL_TEMPLATE, request_str, cloud_candidates_number);
    SoupMessage *msg = soup_message_new ("GET", queryRequest);

    GInputStream *stream = soup_session_send (m_session, msg, NULL, error);

    processCloudResponse (stream, candidates);
}

void
CloudCandidates::processCloudResponse (GInputStream *stream, std::vector<EnhancedCandidate> & candidates)
{
    guint retval;
    CloudCandidatesResponseJsonParser *parser = NULL;
    String text;
    gchar annotation[MAX_PINYIN_LEN + 1];
    guint cloud_source = m_editor->m_config.cloudInputSource ();

    if (cloud_source == BAIDU)
        parser = m_baidu_parser;
    else if (cloud_source == GOOGLE)
        parser = m_google_parser;

    retval = parser->parse (stream);

    /* no annotation if there is NETWORK_ERROR, process before detecting parsed annotation */
    if (retval == PARSER_NETWORK_ERROR) {
        for (std::vector<EnhancedCandidate>::iterator pos = m_candidates.begin (); pos != m_candidates.end (); ++pos) {
            pos->m_display_string = CANDIDATE_INVALID_DATA_TEXT_WITHOUT_PREFIX;
        }
    }

    if (parser->getAnnotation ())
        strncpy (annotation, parser->getAnnotation (), MAX_PINYIN_LEN);
    else /* the request might have been cancelled */
        return;

    if (m_input_mode == FullPinyin) {
        /* get current text in editor */
        text = m_editor->m_text;
    } else {
        /* get current text */
        text = getFullPinyin ();
    }

    /* ignore annotation match while using Baidu source */
    if (cloud_source == BAIDU || !g_strcmp0 (annotation, text)) {
        if (retval == PARSER_NOERR) {
            /* update to the cached candidates list */
            std::vector<std::string> &updated = parser->getStringCandidates ();
            assert (m_candidates.size () == updated.size ());

            for (guint i = 0; i < m_candidates.size (); ++i) {
                /* cache candidate without prefix in m_candidates */
                m_candidates[i].m_display_string = updated[i];
            }
        }
        else {
            String display_text;

            switch (retval) {
            case PARSER_NO_CANDIDATE:
                display_text = CANDIDATE_NO_CANDIDATE_TEXT_WITHOUT_PREFIX;
                break;
            case PARSER_INVALID_DATA:
                display_text = CANDIDATE_INVALID_DATA_TEXT_WITHOUT_PREFIX;
                break;
            case PARSER_BAD_FORMAT:
                display_text = CANDIDATE_BAD_FORMAT_TEXT_WITHOUT_PREFIX;
                break;
            }

            for (std::vector<EnhancedCandidate>::iterator pos = m_candidates.begin (); pos != m_candidates.end (); ++pos) {
                pos->m_display_string = display_text;
            }
        }
    }
}

void
CloudCandidates::updateLookupTable ()
{
    /* retrieve cursor position in lookup table */
    guint cursor = m_editor->m_lookup_table.cursorPos ();

    /* update cached cloud input candidates */
    m_editor->updateCandidates ();

    /* regenerate lookup table */
    m_editor->m_lookup_table.clear ();
    m_editor->fillLookupTable ();

    /* recover cursor position in lookup table */
    m_editor->m_lookup_table.setCursorPos (cursor);

    /* notify ibus */
    m_editor->updateLookupTableFast ();
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

CloudCandidatesResponseJsonParser::CloudCandidatesResponseJsonParser () : m_parser (NULL)
{
    m_parser = json_parser_new ();
}

CloudCandidatesResponseJsonParser::~CloudCandidatesResponseJsonParser ()
{
    /* free json parser object if necessary */
    if (m_parser)
        g_object_unref (m_parser);
}
