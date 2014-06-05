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

import java.util.Collections;
import java.util.HashMap;
import java.util.Map;

/**
 *
 */
public class CMetadata
{

    private final Map<String, String> map = new HashMap<String, String>();

    public CMetadata( Map<String, String> metadata )
    {
        map.putAll( metadata );
    }

    public CMetadata()
    {
    }

    /**
     * Add a value in metadata
     *
     * @param key The unique key linked to the value (must be lower case)
     * @param value The metadata value (must contains only ASCII characters)
     * @throws IllegalArgumentException If the key or value is badly formatted
     */
    public void put( String key, String value )
            throws IllegalArgumentException
    {
        if ( !key.matches( "[a-z-]*" ) ) {
            throw new IllegalArgumentException( "The key must contains letters and dash" );
        }
        // Check value is ASCII
        for ( char c : value.toCharArray() ) {
            if ( c < 32 || c > 127 ) {
                throw new IllegalArgumentException( "The metadata value is not ASCII encoded" );
            }
        }
        map.put( key, value );
    }

    /**
     * Get a metadata value
     *
     * @param key The metadata key
     * @return The value linked to the given key
     */
    public String get( String key )
    {
        return map.get( key );
    }

    public Map<String, String> getMap()
    {
        return Collections.unmodifiableMap( map );
    }

    @Override
    public boolean equals( Object obj )
    {
        if ( obj == null ) {
            return false;
        }
        if ( obj instanceof CMetadata ) {
            CMetadata meta = ( CMetadata ) obj;
            return map.equals( meta.map );
        }
        return false;
    }

    @Override
    public int hashCode()
    {
        int hash = 7;
        hash = 71 * hash + ( this.map != null ? this.map.hashCode() : 0 );
        return hash;
    }

    @Override
    public String toString()
    {
        return map.toString();
    }

}
