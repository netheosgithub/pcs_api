/**
 * Copyright (c) 2014 Netheos (http://www.netheos.net)
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

package net.netheos.pcsapi.providers.googledrive;

import java.util.LinkedList;
import java.util.List;
import net.netheos.pcsapi.models.CPath;
import org.json.JSONObject;

/**
 * Utility class used to convert a CPath to a list of google drive files ids
 *
 * :param c_path : the path (exists or not) :param segments: tuple of segments in c_path (empty for root path) :param
 * files_chain : tuple of files. If remote c_path exists, len(files_chain) = len(segments). If trailing files do not
 * exist, chain_files list is truncated, and may even be empty.
 *
 * Examples : a,b are folders, c.pdf is a blob. /a/b/c.pdf --> segments = ('a','b','c.pdf') files_chain = [
 * {'id':'id_a'...}, {'id':'id_b'...}, {'id':'id_c'...} ] exists() ? True last_is_blob() ? True (c.pdf is not a folder)
 *
 * /a/b/c.pdf/d --> segments = ('a','b','c.pdf', 'd') files_chain = [ {'id':'id_a'...}, {'id':'id_b'...},
 * {'id':'id_c'...} ] exists() ? False last_is_blob() ? True (last is c.pdf)
 *
 * In case c.pdf does not exist : /a/b/c.pdf --> segments = ('a','b','c.pdf') files_chain = [ {'id':'id_a'...},
 * {'id':'id_b'...} ] exists() ? False last_is_blob() ? False (if b is folder)
 */
class RemotePath
{

    private final CPath path;
    final List<String> segments;
    final LinkedList<JSONObject> filesChain;

    /**
     *
     * @param path searched path (exists or not)
     * @param filesChain
     */
    RemotePath( CPath path, LinkedList<JSONObject> filesChain )
    {
        this.path = path;
        this.segments = path.split();
        this.filesChain = filesChain;
    }

    /**
     * Does this path exist google side ?
     */
    public boolean exists()
    {
        return filesChain.size() == segments.size();
    }

    /**
     * If this remote path does not exist, this is the last existing id, or 'root'.
     *
     * @return id of deepest folder in filesChain, or "root" if filesChain is empty
     * @throws IllegalStateException if last segment is a blob
     */
    public String getDeepestFolderId()
    {
        if ( filesChain.isEmpty() ) {
            return "root";
        }
        JSONObject last = filesChain.getLast();
        if ( GoogleDrive.MIME_TYPE_DIRECTORY.equals( last.getString( "mimeType" ) ) ) {
            return last.getString( "id" );
        }
        // If last is a blob, we return the parent id :
        if ( filesChain.size() == 1 ) {
            return "root";
        }
        return filesChain.get( filesChain.size() - 2 ).getString( "id" );
    }

    /**
     * Return deepest object (should be a blob).
     *
     * Object attributes are 'id', 'downloadUrl'
     *
     * @return deepest object
     * @throws IllegalStateException if last segment is a blob
     */
    public JSONObject getBlob()
    {
        if ( !lastIsBlob() ) {
            throw new IllegalStateException( "Inquiring blob of a folder for " + path );
        }
        return filesChain.getLast();
    }

    public boolean lastIsBlob()
    {
        return !filesChain.isEmpty()
               && !GoogleDrive.MIME_TYPE_DIRECTORY.equals( filesChain.getLast().getString( "mimeType" ) );
    }

    /**
     * @param depth
     * @return CPath composed of 'depth' first segments (/ if depth = 0)
     */
    public CPath getFirstSegmentsPath( int depth )
    {
        StringBuilder sb = new StringBuilder( "/" );
        for ( int i = 0; i < depth; i++ ) {
            if ( i >= segments.size() ) {
                break;
            }
            sb.append( segments.get( i ) ).append( "/" );
        }
        return new CPath( sb.toString() );
    }

    /**
     * @return CPath of last existing file
     */
    public CPath lastCPath()
    {
        return getFirstSegmentsPath( filesChain.size() );
    }

}
