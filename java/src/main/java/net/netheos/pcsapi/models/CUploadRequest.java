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

import net.netheos.pcsapi.bytesio.ByteSource;
import net.netheos.pcsapi.bytesio.ProgressByteSource;
import net.netheos.pcsapi.bytesio.ProgressListener;

/**
 * Class that parametrizes an upload request: path + bytes source
 */
public class CUploadRequest
{

    private final CPath path;
    private final ByteSource byteSource;

    private String contentType;
    private CMetadata metadata;
    private ProgressListener progressListener;

    public CUploadRequest( CPath path, ByteSource byteSource )
    {
        this.path = path;
        this.byteSource = byteSource;
    }

    /**
     * Get the destination file path where the data will be uploaded
     *
     * @return The file path
     */
    public CPath getPath()
    {
        return path;
    }

    /**
     * Get the file content type
     *
     * @return The content type or null if not defined
     */
    public String getContentType()
    {
        return contentType;
    }

    /**
     * Set the content type
     *
     * @param contentType The content type (ie. "image/jpeg")
     * @return The upload request
     */
    public CUploadRequest setContentType( String contentType )
    {
        this.contentType = contentType;
        return this;
    }

    /**
     * Get the metadata setted to the file to upload
     *
     * @return The metadata or null if not defined
     */
    public CMetadata getMetadata()
    {
        return metadata;
    }

    /**
     * Set the metadata to the file to upload
     *
     * @param metadata The metadata to use
     * @return The upload request
     */
    public CUploadRequest setMetadata( CMetadata metadata )
    {
        this.metadata = metadata;
        return this;
    }

    /**
     * Defines an object that will be notified during upload.
     *
     * @param pl progress listener
     * @return The upload request
     */
    public CUploadRequest setProgressListener( ProgressListener pl )
    {
        this.progressListener = pl;
        return this;
    }

    /**
     * If no progress listener has been set, return the byte source set in constructor, otherwise decorate it for
     * progress.
     *
     * @return the byte source to be used for upload operation.
     */
    public ByteSource getByteSource()
    {
        ByteSource bs = byteSource;
        if ( progressListener != null ) {
            bs = new ProgressByteSource( bs, progressListener );
        }
        return bs;
    }

    @Override
    public String toString()
    {
        return "CUploadRequest{"
               + "path=" + path
               + ", contentType='" + contentType + '\''
               + ", metadata='" + metadata + '\''
               + '}';
    }

}
