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

package net.netheos.pcsapi.request;

import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import net.netheos.pcsapi.utils.PcsUtils;
import org.apache.http.entity.AbstractHttpEntity;
import org.apache.http.message.BasicHeader;
import org.json.JSONArray;
import org.json.JSONObject;

/**
 *
 */
public class JSONEntity
        extends AbstractHttpEntity
{

    private final byte[] content;

    public JSONEntity( JSONObject json )
    {
        this( json.toString() );
    }

    public JSONEntity( JSONArray json )
    {
        this( json.toString() );
    }

    private JSONEntity( String json_str )
    {
        this.content = json_str.getBytes( PcsUtils.UTF8 );
        contentType = new BasicHeader( "Content-Type", "application/json" );
    }

    @Override
    public boolean isRepeatable()
    {
        return true;
    }

    @Override
    public long getContentLength()
    {
        return content.length;
    }

    @Override
    public InputStream getContent()
            throws IOException, IllegalStateException
    {
        return new ByteArrayInputStream( content );
    }

    @Override
    public void writeTo( OutputStream out )
            throws IOException
    {
        if ( out == null ) {
            throw new IllegalArgumentException( "Output stream may not be null" );
        }
        out.write( content );
        out.flush();
    }

    @Override
    public boolean isStreaming()
    {
        return false;
    }

}
