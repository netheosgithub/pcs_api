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

package net.netheos.pcsapi.bytesio;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.InputStream;

/**
 * Implementation of ByteSource with file
 */
public class FileByteSource
        implements ByteSource
{

    private final File file;

    public FileByteSource( String fileName )
    {
        this.file = new File( fileName );
    }

    public FileByteSource( File file )
    {
        this.file = file;
    }

    @Override
    public InputStream openStream()
            throws FileNotFoundException
    {
        FileInputStream fis = new FileInputStream( file );

        return fis;
    }

    @Override
    public long length()
    {
        return file.length();
    }

    @Override
    public String toString()
    {
        return "FileByteSource {fileName='" + file + "', length='" + length() + "'}";
    }

}
