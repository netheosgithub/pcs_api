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

package net.netheos.pcsapi.models;

import java.util.Date;

/**
 * Represents a blob of data as opposed to a folder
 */
public class CBlob
        extends CFile
{

    private long length;
    private String contentType;

    public CBlob( CPath path, long length, String contentType )
    {
        super( path );
        this.length = length;
        this.contentType = contentType;
    }

    public CBlob( CPath path, long length, String contentType, Date modificationDate, CMetadata metadata )
    {
        super( path );
        this.length = length;
        this.contentType = contentType;
        this.modificationDate = modificationDate;
        this.metadata = metadata;
    }

    public CBlob( CPath path )
    {
        super( path );
    }

    /**
     * Get the file content type
     *
     * @return The content type
     */
    public String getContentType()
    {
        return contentType;
    }

    /**
     * Get the file length
     *
     * @return The length in bytes
     */
    public long length()
    {
        return length;
    }
    
    @Override
    public String toString()
    {
        return "CFolder{"
               + "path=" + path
               + ", modificationDate=" + modificationDate
               + ", metadata=" + metadata
               + ", length=" + length
               + ", contentType=" + contentType
               + '}';
    }

}
