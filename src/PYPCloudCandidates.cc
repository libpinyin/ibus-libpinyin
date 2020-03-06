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

enum CandidateResponseParserError {
    PARSER_NOERR,
    PARSER_INVALID_DATA,
    PARSER_BAD_FORMAT,
    PARSER_NO_CANDIDATE,
    PARSER_UNKNOWN
};

class CloudCandidatesResponseParser
{
public:
    CloudCandidatesResponseParser() {}
    virtual ~CloudCandidatesResponseParser() {}

    virtual guint parse(GInputStream *stream) = 0;
    virtual guint parse(const gchar *data) = 0;

    virtual std::vector<std::string> &getStringCandidates() = 0;
    virtual std::vector<EnhancedCandidate> getCandidates() = 0;
};

class CloudCandidatesResponseJsonParser : public CloudCandidatesResponseParser
{
public:
    CloudCandidatesResponseJsonParser();
    virtual ~CloudCandidatesResponseJsonParser();

    std::vector<std::string> &getStringCandidates() { return m_candidates; }
    std::vector<EnhancedCandidate> getCandidates();
    const gchar *getAnnotation() { return m_annotation; }

    guint parse(GInputStream *stream);
    guint parse(const gchar *data);

protected:
    JsonParser *m_parser;
    std::vector<std::string> m_candidates;
    const gchar *m_annotation;

    virtual guint parseJsonResponse(JsonNode *root) = 0;
};

class GoogleCloudCandidatesResponseJsonParser : public CloudCandidatesResponseJsonParser
{
protected:
    guint parseJsonResponse(JsonNode *root);

public:
    GoogleCloudCandidatesResponseJsonParser() : CloudCandidatesResponseJsonParser() {}
};

class BaiduCloudCandidatesResponseJsonParser : public CloudCandidatesResponseJsonParser
{
private:
    guint parseJsonResponse(JsonNode *root);

public:
    BaiduCloudCandidatesResponseJsonParser() : CloudCandidatesResponseJsonParser() {}
    ~BaiduCloudCandidatesResponseJsonParser() { if (m_annotation) g_free((gpointer)m_annotation); }
};

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
    EnhancedCandidate testCan;
    const gchar *text = m_editor->m_text;

    if (strlen (text) < m_min_cloud_trigger_length)
        return FALSE;
    
    /* Validate candidate list length */
    if (candidates.size() < m_first_cloud_candidate_position + m_cloud_candidates_number)
        return FALSE;

    /*have cloud candidates already*/
    testCan = candidates[m_first_cloud_candidate_position-1];
    if (testCan.m_candidate_type == CANDIDATE_CLOUD_INPUT)
        return FALSE;

    /* insert cloud candidates' placeholders */
    m_candidates.clear ();
    for (guint i = 0; i < m_cloud_candidates_number; ++i) {
        EnhancedCandidate  enhanced;
        enhanced.m_display_string = "...";
        enhanced.m_candidate_type = CANDIDATE_CLOUD_INPUT;
        m_candidates.push_back (enhanced);
    }
    candidates.insert (candidates.begin () + m_first_cloud_candidate_position - 1, m_candidates.begin (), m_candidates.end ());

    if (! m_editor->m_config.doublePinyin ())
    {
        if (strlen (text) >= m_min_cloud_trigger_length)
            cloudAsyncRequest (text, candidates);
    }
    else
    {
        m_editor->updateAuxiliaryText ();
        String stripped = m_editor->m_buffer;
        const gchar *temp= stripped;
        gchar** tempArray =  g_strsplit_set (temp, " |", -1);
        gchar *text=g_strjoinv ("",tempArray);

        if (strlen (text) >= m_min_cloud_trigger_length)
            cloudAsyncRequest (text, candidates);

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
    CloudCandidates *cloudCandidates = static_cast<CloudCandidates *> (user_data);

    /* Process results */
    cloudCandidates->processCloudResponse(stream, cloudCandidates->m_editor->m_candidates);

    /* Regenerate lookup table */
    cloudCandidates->m_editor->m_lookup_table.clear();
    cloudCandidates->m_editor->fillLookupTable ();
    cloudCandidates->m_editor->updateLookupTableFast ();
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

    processCloudResponse(stream, candidates);
}

void
CloudCandidates::processCloudResponse (GInputStream *stream, std::vector<EnhancedCandidate> & candidates)
{
    guint ret_code;
    CloudCandidatesResponseJsonParser *parser = NULL;

    if (m_cloud_source == BAIDU)
        parser = new BaiduCloudCandidatesResponseJsonParser();
    else if (m_cloud_source == GOOGLE)
        parser = new GoogleCloudCandidatesResponseJsonParser();

    ret_code = parser->parse(stream);

    if (!g_strcmp0(parser->getAnnotation(), m_editor->m_text))
    {
        /* Update to the candidates list */
        std::vector<std::string> &updated_candidates = parser->getStringCandidates();
        m_candidates.clear();
        for (guint i = 0; i < m_cloud_candidates_number && i < updated_candidates.size(); ++i)
        {
            EnhancedCandidate & enhanced = candidates[i + m_first_cloud_candidate_position - 1];
            enhanced.m_display_string = updated_candidates[i];
            enhanced.m_candidate_type = CANDIDATE_CLOUD_INPUT;
            m_candidates.push_back(enhanced);
        }
    }

    if (parser)
        delete parser;
}

CloudCandidatesResponseJsonParser::CloudCandidatesResponseJsonParser() : m_parser(NULL), m_annotation(NULL)
{
    m_parser = json_parser_new();
}

CloudCandidatesResponseJsonParser::~CloudCandidatesResponseJsonParser()
{
    /* Free json parser object if necessary */
    if (m_parser)
        g_object_unref(m_parser);
}

std::vector<EnhancedCandidate> CloudCandidatesResponseJsonParser::getCandidates ()
{
    std::vector<EnhancedCandidate> candidates;

    for (std::vector<std::string>::const_iterator i = m_candidates.cbegin (); i != m_candidates.cend(); ++i)
    {
        EnhancedCandidate candidate;
        candidate.m_candidate_type = CANDIDATE_CLOUD_INPUT;
        candidate.m_display_string = *i;
        candidates.push_back(candidate);
    }

    return candidates;
}


guint CloudCandidatesResponseJsonParser::parse (GInputStream *stream)
{
    GError **error = NULL;

    if (!stream)
        return PARSER_INVALID_DATA;

    // Parse Json from input steam
    if (!json_parser_load_from_stream(m_parser, stream, NULL, error) || error != NULL)
    {
        g_input_stream_close(stream, NULL, error);  // Close stream to release libsoup connexion
        return PARSER_INVALID_DATA;
    }
    g_input_stream_close(stream, NULL, error);  // Close stream to release libsoup connexion

    return parseJsonResponse(json_parser_get_root(m_parser));
}

guint CloudCandidatesResponseJsonParser::parse (const gchar *data)
{
    GError **error = NULL;

    if (!data)
        return PARSER_INVALID_DATA;

    /* Parse Json from data */
    if (!json_parser_load_from_data(m_parser, data, strlen(data), error) || error != NULL)
        return PARSER_INVALID_DATA;

    return parseJsonResponse(json_parser_get_root(m_parser));
}

guint GoogleCloudCandidatesResponseJsonParser::parseJsonResponse (JsonNode *root)
{
    if (!JSON_NODE_HOLDS_ARRAY(root))
        return PARSER_INVALID_DATA;

    /* Validate Google source and the structure of response */
    JsonArray *google_root_array = json_node_get_array(root);

    const gchar *google_response_status;
    JsonArray *google_response_array;
    JsonArray *google_result_array;
    const gchar *google_candidate_annotation;
    JsonArray *google_candidate_array;
    guint result_counter;

    if (json_array_get_length(google_root_array) <= 1)
        return PARSER_INVALID_DATA;

    google_response_status = json_array_get_string_element(google_root_array, 0);

    if (g_strcmp0(google_response_status, "SUCCESS"))
        return PARSER_INVALID_DATA;

    google_response_array = json_array_get_array_element(google_root_array, 1);

    if (json_array_get_length(google_root_array) < 1)
        return PARSER_INVALID_DATA;

    google_result_array = json_array_get_array_element(google_response_array, 0);

    google_candidate_annotation = json_array_get_string_element(google_result_array, 0);

    if (!google_candidate_annotation)
        return PARSER_INVALID_DATA;

    /* Update annotation with the returned annotation */
    m_annotation = google_candidate_annotation;

    google_candidate_array = json_array_get_array_element(google_result_array, 1);

    result_counter = json_array_get_length(google_candidate_array);

    if (result_counter < 1)
        return PARSER_NO_CANDIDATE;

    for(guint i = 0; i < result_counter; ++i)
    {
        std::string candidate = json_array_get_string_element(google_candidate_array, i);
        m_candidates.push_back(candidate);
    }

    return PARSER_NOERR;
}

guint BaiduCloudCandidatesResponseJsonParser::parseJsonResponse (JsonNode *root)
{
    if (!JSON_NODE_HOLDS_OBJECT(root))
        return PARSER_INVALID_DATA;

    /* Validate Baidu source and the structure of response */
    JsonObject *baidu_root_object = json_node_get_object(root);
    const gchar *baidu_response_status;
    JsonArray *baidu_result_array;
    JsonArray *baidu_candidate_array;
    const gchar *baidu_candidate_annotation;
    guint result_counter;

    if (!json_object_has_member(baidu_root_object, "status"))
        return PARSER_INVALID_DATA;

    baidu_response_status = json_object_get_string_member(baidu_root_object, "status");

    if (g_strcmp0(baidu_response_status, "T"))
        return PARSER_INVALID_DATA;

    if (!json_object_has_member(baidu_root_object, "result"))
        return PARSER_INVALID_DATA;

    baidu_result_array = json_object_get_array_member(baidu_root_object, "result");

    baidu_candidate_array = json_array_get_array_element(baidu_result_array, 0);
    baidu_candidate_annotation = json_array_get_string_element(baidu_result_array, 1);

    if (!baidu_candidate_annotation)
        return PARSER_INVALID_DATA;

    /* Update annotation with the returned annotation */
    m_annotation = NULL;
    gchar **words = g_strsplit(baidu_candidate_annotation, "'", -1);
    m_annotation = g_strjoinv("", words);
    g_strfreev(words);

    result_counter = json_array_get_length(baidu_candidate_array);

    if (result_counter < 1)
        return PARSER_NO_CANDIDATE;

    for(guint i = 0; i < result_counter; ++i)
    {
        std::string candidate;
        JsonArray *baidu_candidate = json_array_get_array_element(baidu_candidate_array, i);

        if (json_array_get_length(baidu_candidate) < 1)
            candidate = "...";
        else
            candidate = json_array_get_string_element(baidu_candidate, 0);

        m_candidates.push_back(candidate);
    }

    return PARSER_NOERR;
}
