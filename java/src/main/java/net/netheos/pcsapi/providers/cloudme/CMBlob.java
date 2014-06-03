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

package net.netheos.pcsapi.providers.cloudme;

import net.netheos.pcsapi.models.CBlob;
import net.netheos.pcsapi.models.CPath;

import org.w3c.dom.Element;

import java.text.ParseException;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.TimeZone;

/**
 * This class represents a CloudMe File It is characterized by an id and the id of its folder
 */
public class CMBlob
{

    private final String id;
    private final CMFolder folder;
    private final String name;
    private final long length;
    private final Date updated;
    private final String contentType;
    
    public CMBlob( CMFolder folder, String id, String name, long length, Date updated, String contentType )
    {
        this.folder = folder;
        this.id = id;
        this.name = name;
        this.length = length;
        this.updated = updated;
        this.contentType = contentType;
    }

    /**
     * Converts a CloudMe Blob to a generic CBlob
     *
     * @return
     */
    public CBlob toCBlob()
    {
        CBlob cBlob = new CBlob( getPath(), length, contentType );
        cBlob.setModificationDate( updated );
        
        return cBlob;
    }

    /**
     * Gets CPath of a CloudMe Blob which is its parent folder's CPath + its base name
     *
     * @return
     */
    public CPath getPath()
    {
        return folder.getCPath().add( name );
    }

    /**
     * Factory method that builds a CMBlob based on a XML "atom:entry" element
     *
     * @param xmlEntry
     */
    public static CMBlob buildCMFile( CMFolder folder, Element xmlEntry )
            throws ParseException
    {
        final String name = xmlEntry.getElementsByTagNameNS( "*", "title" ).item( 0 ).getTextContent();
        final String id = xmlEntry.getElementsByTagNameNS( "*", "document" ).item( 0 ).getTextContent();
        
        final String updateDate = xmlEntry.getElementsByTagNameNS( "*", "updated" ).item( 0 ).getTextContent();
        SimpleDateFormat sdf = new SimpleDateFormat( "yyyy-MM-dd'T'HH:mm:ss'Z'" );
        sdf.setLenient( false );
        sdf.setTimeZone( TimeZone.getTimeZone( "GMT" ) );
        final Date updated = sdf.parse( updateDate );
        
        final Element elementLink = ( Element ) xmlEntry.getElementsByTagNameNS( "*",
                                                                                 "link" ).item( 0 );
        
        final long length = Long.parseLong( elementLink.getAttribute( "length" ) );
        final String contentType = elementLink.getAttribute( "type" );
        
        return new CMBlob( folder, id, name, length, updated, contentType );
    }
    
    public String getId()
    {
        return id;
    }
    
    public String getName()
    {
        return name;
    }
    
}
