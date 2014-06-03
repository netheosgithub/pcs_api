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
 * Represents a file (folder or blob) located in the cloud
 */
public abstract class CFile
{

    final CPath path;
    protected Date modificationDate;
    protected CMetadata metadata;

    public CFile( CPath path )
    {
        this.path = path;
    }

    /**
     * Get the file path
     *
     * @return The path
     */
    public CPath getPath()
    {
        return path;
    }

    /**
     * Get the last modification date
     *
     * @return The date or null if not available
     */
    public Date getModificationDate()
    {
        return modificationDate;
    }

    public void setModificationDate( Date modificationDate )
    {
        this.modificationDate = modificationDate;
    }

    /**
     * Get the file metadata
     *
     * @return The metadata or null if not available
     */
    public CMetadata getMetadata()
    {
        return metadata;
    }

    public void setMetadata( CMetadata metadata )
    {
        this.metadata = metadata;
    }

    /**
     * Indicates if the file is a folder
     *
     * @return true if the file is a folder, false otherwise
     */
    public boolean isFolder()
    {
        return this instanceof CFolder;
    }

    /**
     * Indicates if the file is a blob
     *
     * @return true if the file is a blob, false otherwise
     */
    public boolean isBlob()
    {
        return this instanceof CBlob;
    }

    @Override
    public String toString()
    {
        return "CFile{"
               + "path=" + path
               + ", modificationDate=" + modificationDate
               + ", metadata=" + metadata
               + '}';
    }

}
