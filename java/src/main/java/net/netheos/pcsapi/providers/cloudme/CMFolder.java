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

import net.netheos.pcsapi.models.CFolder;
import net.netheos.pcsapi.models.CPath;

import java.util.HashMap;
import java.util.Iterator;
import java.util.List;
import java.util.Map;

/**
 * This class represents a CloudMe folder
 * It is characterized by an id.
 * Its name is the base/simple name of the folder
 */
public class CMFolder implements Iterable<CMFolder>
{
    private String id;

    public String getName()
    {
        return name;
    }

    public String getId()
    {
        return id;
    }

    private String name;
    private CMFolder parent;
    private Map<String, CMFolder> children;

    public CMFolder( String id, String name )
    {
        this.id = id;
        this.name = name;
        this.children = new HashMap<String, CMFolder>();
    }

    /**
     * Adds a child folder
     *
     * @param id id of the child folder
     * @param name name of the child folder
     * @return the added folder
     */
    public CMFolder addChild( String id, String name )
    {
        CMFolder childFolder = new CMFolder( id, name );
        childFolder.parent = this;
        this.children.put( name, childFolder );
        return childFolder;
    }

    @Override
    public Iterator<CMFolder> iterator()
    {
        return children.values().iterator();
    }

    /**
     * Finds a child according to its name
     *
     * @param name of the folder to find
     * @return
     */
    public CMFolder getChildByName( String name )
    {
        return children.get( name );
    }

    /**
     * Gets the CloudMe folder corresponding to a given CPath
     * Returns null if the folder does not exist
     *
     * @param path
     * @return
     */
    public CMFolder getFolder( CPath path )
    {
        List<String> baseNames = path.split();

        if (path.isRoot() ){
            return this;
        }

        CMFolder currentFolder = this;
        CMFolder subFolder = null;

        for ( String baseName : baseNames ) {
            subFolder = currentFolder.getChildByName( baseName );

            if ( subFolder == null ) {
                return null;
            }

            currentFolder = subFolder;
        }

        return subFolder;
    }

    /**
     * Gets the CPath corresponding to this folder
     *
     * @return
     */
    public CPath getCPath()
    {
        if ( parent == null ) {
            return CPath.ROOT;
        }

        CMFolder currentFolder = this;
        StringBuilder path = new StringBuilder(  );

        while ( currentFolder.parent != null ) {
            path.insert( 0, currentFolder.name );
            path.insert( 0, '/' );

            currentFolder = currentFolder.parent;
        }

        return new CPath( path.toString() );
    }

    /**
     * Converts a CloudMe folder to a generic CFolder
     * @return
     */
    public CFolder toCFolder()
    {
        return new CFolder( getCPath() );
    }
}