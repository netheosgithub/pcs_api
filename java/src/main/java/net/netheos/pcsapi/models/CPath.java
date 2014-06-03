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

import net.netheos.pcsapi.utils.URIUtil;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/**
 * Immutable remote file pathname, with methods for easier handling.
 * <p/>
 * path components are separated by a single slash. A path is always unicode and normalized so that it begins by a
 * leading slash and never ends with a trailing slash, except for root path : '/'. Anti slashes are forbidden in CPath
 * objects.
 */
public class CPath
{

    public static final CPath ROOT = new CPath( "/" );
    private static final char FORBIDDEN_CHAR = '\\';
    private final String pathName;

    public CPath( String pathName )
    {
        check( pathName );
        this.pathName = CPath.normalize( pathName );
    }

    /**
     * Replaces double slashes with single slash, remove any trailing slash except for root path '/'. Checks that given
     * pathname does not contain any backslash.
     *
     * @param pathName
     * @return
     */
    private static String normalize( String pathName )
    {
        // replaces multiple '/' by one unique '/'
        pathName = pathName.replaceAll( "/+", "/" );

        // removes a '/' if it finishes a path and is not root
        pathName = pathName.replaceAll( "(.+)/$", "$1" );

        if ( !pathName.startsWith( "/" ) ) {
            pathName = "/" + pathName;
        }
        return pathName;
    }

    /**
     * Check pathname is valid
     *
     * @param pathName
     */
    private void check( String pathName )
    {
        for ( char c : pathName.toCharArray() ) {
            if ( c < 32 || FORBIDDEN_CHAR == c ) {
                throw new IllegalArgumentException( "Pathname contains invalid char " + c + " : " + pathName );
            }
        }
        // Check each component :
        for ( String comp : pathName.split( "/" ) ) {
            if ( !comp.trim().equals( comp ) ) {
                throw new IllegalArgumentException( "Pathname contains leading or trailing spaces : " + pathName );
            }
        }
    }

    /**
     * URL encode the path
     *
     * @return this full path string encoded for URL path (slashes are not encoded)
     */
    public String getUrlEncoded()
    {
        try {
            return URIUtil.encodePath( pathName );
        } catch ( Exception ex ) {
            throw new UnsupportedOperationException( "Error getting encoded path", ex );
        }
    }

    /**
     * Return last element of this path as string (empty string if this object is root path)
     *
     * @return last element of this path as string (empty string if this object is root path)
     */
    public String getBaseName()
    {
        int lastIndex = pathName.lastIndexOf( '/' );
        return pathName.substring( lastIndex + 1 );
    }

    /**
     * Indicates if this path is the root in the filesystem
     *
     * @return true if this path is the root path
     */
    public boolean isRoot()
    {
        return pathName.equals( "/" );
    }

    /**
     * Split this path : "a/b/c" --> "a", "b", "c".
     *
     * @return a list of segments for this path (empty list for root path)
     */
    public List<String> split()
    {
        if ( isRoot() ) {
            return Collections.EMPTY_LIST;
        }
        String path = pathName;
        if ( pathName.startsWith( "/" ) ) {
            path = pathName.substring( 1 );
        }
        return Arrays.asList( path.split( "/" ) );
    }

    @Override
    public boolean equals( Object obj )
    {
        if ( obj == null ) {
            return false;
        }
        if ( obj instanceof CPath ) {
            CPath path = ( CPath ) obj;
            return path.pathName.equals( pathName ) || ( isRoot() && path.isRoot() );
        }
        return false;
    }

    @Override
    public int hashCode()
    {
        int hash = 7;
        hash = 89 * hash + ( this.pathName != null ? this.pathName.hashCode() : 0 );
        return hash;
    }

    /**
     * Get the path name
     *
     * @return The path name
     */
    public String getPathName()
    {
        return pathName;
    }

    @Override
    public String toString()
    {
        return pathName;
    }

    /**
     * @return CPath of this parent folder, or root folder if this path is root folder
     */
    public CPath getParent()
    {
        int lastIndex = pathName.lastIndexOf( '/' );

        if ( lastIndex == 0 ) {
            return ROOT;

        } else {
            return new CPath( pathName.substring( 0, lastIndex ) );
        }
    }

    /**
     * Adds a component to the current path
     *
     * @param baseName
     * @return the new path with the
     */
    public CPath add( String baseName )
    {
        return new CPath( pathName + '/' + baseName );
    }

}
