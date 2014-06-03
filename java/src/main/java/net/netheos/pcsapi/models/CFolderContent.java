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

import java.util.Collection;
import java.util.Iterator;
import java.util.Map;

/**
 * Contains the folder content.
 */
public class CFolderContent
        implements Iterable<CFile>
{

    private final Map<CPath, CFile> content;

    public CFolderContent( Map<CPath, CFile> content )
    {
        this.content = content;
    }

    /**
     * Indicates if the folder contains the given path
     *
     * @param path The path to check
     * @return true if the file path exists, fals otherwise
     */
    public boolean containsPath( CPath path )
    {
        return content.containsKey( path );
    }

    /**
     * Get the files stored in the folder
     *
     * @return The files list
     */
    public Collection<CFile> getFiles()
    {
        return content.values();
    }

    /**
     * Get the files paths stored in the folder
     *
     * @return The paths list
     */
    public Collection<CPath> getPaths()
    {
        return content.keySet();
    }

    /**
     * Get a file by its path
     *
     * @param path The file path to search
     * @return The file found or null if the path is not in the folder
     */
    public CFile getFile( CPath path )
    {
        return content.get( path );
    }

    /**
     * Indicates if the folder is empty
     *
     * @return true if the folder is empty, false otherwise
     */
    public boolean isEmpty()
    {
        return content.isEmpty();
    }

    /**
     * Get the files count in the folder
     *
     * @return The files count
     */
    public int size()
    {
        return content.size();
    }

    @Override
    public Iterator<CFile> iterator()
    {
        return content.values().iterator();
    }

    @Override
    public boolean equals( Object obj )
    {
        if ( obj == null ) {
            return false;
        }
        if ( obj instanceof CFolderContent ) {
            CFolderContent fc = ( CFolderContent ) obj;
            return content.equals( fc.content );
        }
        return false;
    }

    @Override
    public int hashCode()
    {
        int hash = 7;
        hash = 61 * hash + ( this.content != null ? this.content.hashCode() : 0 );
        return hash;
    }

    @Override
    public String toString()
    {
        return content.keySet().toString();
    }

}
