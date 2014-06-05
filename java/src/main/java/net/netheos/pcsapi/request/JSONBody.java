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

import java.io.IOException;
import java.io.OutputStream;
import net.netheos.pcsapi.utils.PcsUtils;
import org.apache.http.entity.mime.MIME;
import org.apache.http.entity.mime.content.AbstractContentBody;
import org.json.JSONArray;
import org.json.JSONObject;

/**
 *
 */
public class JSONBody
        extends AbstractContentBody
{

    private static final String CONTENT_TYPE = "application/json";

    private final byte[] content;
    private final String filename;

    public JSONBody( JSONObject json, String filename )
    {
        super( CONTENT_TYPE );
        this.content = json.toString().getBytes( PcsUtils.UTF8 );
        this.filename = filename;
    }

    public JSONBody( JSONArray json, String filename )
    {
        super( CONTENT_TYPE );
        this.content = json.toString().getBytes( PcsUtils.UTF8 );
        this.filename = filename;
    }

    @Override
    public String getFilename()
    {
        return filename;
    }

    @Override
    public void writeTo( OutputStream out )
            throws IOException
    {
        out.write( content );
        out.flush();
    }

    @Override
    public String getCharset()
    {
        return PcsUtils.UTF8.name();
    }

    @Override
    public String getTransferEncoding()
    {
        return MIME.ENC_8BIT;
    }

    @Override
    public long getContentLength()
    {
        return content.length;
    }

}
