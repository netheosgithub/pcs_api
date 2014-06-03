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

package net.netheos.pcsapi.oauth;

import net.netheos.pcsapi.credentials.PasswordCredentials;
import net.netheos.pcsapi.credentials.UserCredentialsRepository;
import net.netheos.pcsapi.exceptions.CRetriableException;
import net.netheos.pcsapi.request.CResponse;
import net.netheos.pcsapi.storage.StorageBuilder;
import net.netheos.pcsapi.utils.PcsUtils;

import org.apache.http.HttpResponse;
import org.apache.http.auth.AuthScope;
import org.apache.http.auth.UsernamePasswordCredentials;
import org.apache.http.client.HttpClient;
import org.apache.http.client.methods.HttpUriRequest;
import org.apache.http.client.protocol.ClientContext;
import org.apache.http.protocol.HttpContext;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.IOException;
import java.net.URI;
import java.util.HashMap;
import java.util.Map;
import org.apache.http.HttpHost;
import org.apache.http.client.CredentialsProvider;
import org.apache.http.impl.client.BasicCredentialsProvider;
import org.apache.http.protocol.BasicHttpContext;

/**
 * Password session manager : handles basic and digest authentication.
 */
public class PasswordSessionManager
        extends SessionManager<PasswordCredentials>
{

    private static final Logger LOGGER = LoggerFactory.getLogger( PasswordSessionManager.class );

    private final UserCredentialsRepository userCredentialsRepo;
    private final HttpClient httpClient;
    private final UsernamePasswordCredentials usernamePasswordCredentials;
    /**
     * Cache of HttpContext, required to do preemptive authentications.
     *
     * These objects should not be shared between threads, so they are created and wrapped in a ThreadLocal.
     */
    private final Map<HttpHost, ThreadLocal<HttpContext>> cache = new HashMap<HttpHost, ThreadLocal<HttpContext>>();

    public PasswordSessionManager( StorageBuilder builder )
    {
        super( builder.getUserCredentials() );

        this.httpClient = builder.getHttpClient();
        this.userCredentialsRepo = builder.getUserCredentialsRepo();

        usernamePasswordCredentials = new UsernamePasswordCredentials( userCredentials.getUserId(),
                                                                       userCredentials.getCredentials().getPassword() );

        // checks if we already have user_credentials:
        if ( userCredentials != null ) {
            PasswordCredentials credentials = userCredentials.getCredentials();
            if ( credentials.getPassword() == null ) {
                throw new IllegalStateException( "User credentials do not contain any password" );
            }
        }
    }

    @Override
    public CResponse execute( HttpUriRequest request )
    {
        if ( LOGGER.isTraceEnabled() ) {
            LOGGER.trace( "{}: {}", request.getMethod(), PcsUtils.shortenUrl( request.getURI() ) );
        }

        try {
            URI uri = request.getURI();
            HttpHost host = new HttpHost( uri.getHost(), uri.getPort(), uri.getScheme() );
            HttpContext context = getHttpContext( host );

            HttpResponse httpResponse = httpClient.execute( host, request, context );
            return new CResponse( request, httpResponse );

        } catch ( IOException ex ) {
            // a low level error should be retried :
            throw new CRetriableException( ex );
        }
    }

    private synchronized HttpContext getHttpContext( HttpHost host )
    {
        ThreadLocal<HttpContext> tlContext = cache.get( host );
        if ( tlContext == null ) {
            tlContext = new ThreadLocal<HttpContext>();
            cache.put( host, tlContext );
        }
        HttpContext context = tlContext.get();
        if ( context == null ) {
            AuthScope scope = new AuthScope( host.getHostName(), host.getPort() );
            CredentialsProvider credentialsProvider = new BasicCredentialsProvider();
            credentialsProvider.setCredentials( scope, usernamePasswordCredentials );

            context = new BasicHttpContext();
            context.setAttribute( ClientContext.CREDS_PROVIDER, credentialsProvider );
            tlContext.set( context );
        }
        return context;
    }

    UserCredentialsRepository getUserCredentialsRepository()
    {
        return userCredentialsRepo;
    }

}
