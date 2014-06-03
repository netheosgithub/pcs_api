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

package net.netheos.pcsapi.exceptions;

import net.netheos.pcsapi.models.CPath;

/**
 * Thrown when performing an operation on a folder when a blob is expected, or when operating on a blob and a folder is
 * expected. Also thrown when downloading provider special files (eg google drive native docs).
 */
public class CInvalidFileTypeException
        extends CStorageException
{

    private final CPath cpath;
    private final boolean blobExpected;

    public CInvalidFileTypeException( String message, CPath cpath, boolean blobExpected )
    {
        super( message );
        this.cpath = cpath;
        this.blobExpected = blobExpected;
    }

    public CInvalidFileTypeException( CPath cpath, boolean blobExpected )
    {
        this( "Invalid file type at path " + cpath + "( expected " + ( blobExpected ? "blob" : "folder" ) + ")",
              cpath, blobExpected );
    }

    public CPath getPath()
    {
        return cpath;
    }

    public boolean isBlobExpected()
    {
        return blobExpected;
    }

}
