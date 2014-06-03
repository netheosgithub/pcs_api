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

/**
 * A simple class containing used/available storage information.
 * <p/>
 * Negative values indicate that the information is not available from provider.
 */
public class CQuota
{

    private final long bytesUsed;
    private final long bytesAllowed;

    public CQuota( long bytesUsed, long bytesAllowed )
    {
        this.bytesUsed = bytesUsed;
        this.bytesAllowed = bytesAllowed;
    }

    /**
     * Get the percent used in the filesystem
     *
     * @return used space as a float percentage (or -1.0 if unknown values)
     */
    public float getPercentUsed()
    {
        if ( bytesUsed >= 0 && bytesAllowed > 0 ) {
            return bytesUsed * 100f / bytesAllowed;

        } else {
            return -1f;
        }
    }

    /**
     * Get the bytes used in the filesystem
     *
     * @return The bytes count
     */
    public long getBytesUsed()
    {
        return bytesUsed;
    }

    /**
     * Get the maximum allowed bytes in the filesystem
     *
     * @return The bytes count
     */
    public long getBytesAllowed()
    {
        return bytesAllowed;
    }

    @Override
    public String toString()
    {
        return "CQuota{"
               + "bytesUsed=" + bytesUsed
               + ", bytesAllowed=" + bytesAllowed
               + ", pctUsed=" + getPercentUsed() + "%"
               + '}';
    }

}
