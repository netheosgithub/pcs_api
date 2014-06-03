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

import net.netheos.pcsapi.exceptions.CStorageException;
import net.netheos.pcsapi.utils.PcsUtils;
import net.netheos.pcsapi.utils.XmlUtils;

import org.apache.http.HttpEntity;
import org.apache.http.HttpResponse;
import org.apache.http.client.methods.HttpUriRequest;
import org.apache.http.util.EntityUtils;
import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.w3c.dom.Document;
import java.io.Closeable;
import java.io.IOException;
import java.io.InputStream;
import java.net.URI;

/**
 * Represents a response returned by an HTTP client.
 */
public class CResponse
        implements Closeable
{

    private final int status;
    private final HttpEntity entity;
    private final String method;
    private final URI uri;
    private String contentType;
    private String reason;
    private final Headers headers;

    public CResponse( HttpUriRequest request, HttpResponse response )
    {
        method = request.getMethod();
        uri = request.getURI();

        status = response.getStatusLine().getStatusCode();
        entity = response.getEntity();

        if ( entity != null && entity.getContentType() != null ) {
            contentType = entity.getContentType().getValue();
        }

        reason = response.getStatusLine().getReasonPhrase();
        if ( reason == null || reason.isEmpty() ) {
            reason = "No reason specified";
        }
        headers = new Headers( response.getAllHeaders() );
    }

    public int getStatus()
    {
        return status;
    }

    public String getMethod()
    {
        return method;
    }

    public URI getUri()
    {
        return uri;
    }

    public String getContentType()
    {
        return contentType;
    }

    public String getReason()
    {
        return reason;
    }

    @Override
    public void close()
            throws IOException
    {
        if ( entity != null ) {
            entity.getContent().close();
        }
    }

    /**
     * Extracts string from input stream
     *
     * @return Get the response as a String
     */
    public String asString()
    {
        try {
            return EntityUtils.toString( entity, PcsUtils.UTF8.name() );
        } catch ( IOException e ) {
            throw new CStorageException( "Can't get string from HTTP entity", e );
        }
    }

    /**
     * Get the response as a json object
     *
     * @return The json value
     */
    public JSONObject asJSONObject()
    {
        if ( entity == null ) {
            return null;
        }
        String str = asString();
        try {
            return new JSONObject( str );
        } catch ( JSONException ex ) {
            throw new CStorageException( "Error parsing the JSON response: " + str, ex );
        }
    }

    /**
     * Get the response as a DOM
     *
     * @return The DOM document
     */
    public Document asDom()
    {
        if ( entity == null ) {
            return null;
        }

        return XmlUtils.getDomFromString( asString() );
    }

    /**
     * Get the response as a json array
     *
     * @return The json value
     */
    public JSONArray asJSONArray()
    {
        if ( entity == null ) {
            return null;
        }
        String str = asString();
        try {
            return new JSONArray( str );
        } catch ( JSONException ex ) {
            throw new CStorageException( "Error parsing the JSON response: " + str, ex );
        }
    }

    /**
     *
     * @return entity content length, or 0 if response has no entity, or a negative number if unknown.
     */
    public long getContentLength()
    {
        if ( entity == null ) {
            return 0;
        }
        return entity.getContentLength();
    }

    /**
     * Get the response headers
     *
     * @return The headers
     */
    public Headers getHeaders()
    {
        return headers;
    }

    /**
     * Open a raw stream on the response body.
     *
     * @return The stream
     */
    public InputStream openStream()
    {
        if ( entity == null ) {
            return null;
        }

        try {
            return entity.getContent();
        } catch ( IOException ex ) {
            throw new CStorageException( "Can't open stream", ex );
        }
    }

}
