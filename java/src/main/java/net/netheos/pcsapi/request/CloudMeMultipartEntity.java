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

/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
package net.netheos.pcsapi.request;

import java.nio.charset.Charset;
import net.netheos.pcsapi.utils.PcsUtils;
import org.apache.http.entity.mime.HttpMultipartMode;
import org.apache.http.entity.mime.MultipartEntity;

/**
 * CloudMe does not support charset in content-type multi part upload.
 */
public class CloudMeMultipartEntity
        extends MultipartEntity
{

    public CloudMeMultipartEntity()
    {
        super( HttpMultipartMode.BROWSER_COMPATIBLE, null, PcsUtils.UTF8 );
    }

    @Override
    protected String generateContentType(
            final String boundary,
            final Charset charset )
    {
        StringBuilder buffer = new StringBuilder();
        buffer.append( "multipart/form-data; boundary=" );
        buffer.append( boundary );
        //if (charset != null) {
        //    buffer.append("; charset=");
        //    buffer.append(charset.name());
        //}
        return buffer.toString();
    }

}
