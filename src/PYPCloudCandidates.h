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

#ifndef __PY_LIB_PINYIN_ClOUD_CANDIDATES_H_
#define __PY_LIB_PINYIN_ClOUD_CANDIDATES_H_

#include "PYString.h"
#include "PYPointer.h"
#include "PYPEnhancedCandidates.h"
#include <vector>
#include <libsoup/soup.h>
#include "PYConfig.h"



namespace PY {
    
#define BUFFERLENGTH 2048
    
class PhoneticEditor;

class CloudCandidates : public EnhancedCandidates<PhoneticEditor> 
{
public:

    CloudCandidates (PhoneticEditor *editor);
    ~CloudCandidates();
    
    gboolean processCandidates (std::vector<EnhancedCandidate> & candidates);
    
    int selectCandidate (EnhancedCandidate & enhanced);
    
    void cloudAsyncRequest(const gchar* requestStr, std::vector<EnhancedCandidate> & candidates);
    void cloudSyncRequest(const gchar* requestStr, std::vector<EnhancedCandidate> & candidates);
    
    gboolean m_cloud_state;
    guint m_cloud_source;
    guint m_cloud_candidates_number;        
    guint m_first_cloud_candidate_position;
    guint m_min_cloud_trigger_length;
    gboolean m_cloud_flag;

    
private:
    static void cloudResponseCallBack(GObject *object, GAsyncResult *result, gpointer user_data);
    
private:
    SoupSession *m_session;
    
protected:
    std::vector<EnhancedCandidate> m_candidates;
};

};

#endif


