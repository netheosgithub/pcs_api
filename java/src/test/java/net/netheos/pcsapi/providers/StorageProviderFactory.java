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

package net.netheos.pcsapi.providers;

import net.netheos.pcsapi.credentials.AppInfoFileRepository;
import net.netheos.pcsapi.credentials.UserCredentialsFileRepository;
import net.netheos.pcsapi.storage.StorageBuilder;
import net.netheos.pcsapi.storage.StorageFacade;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.security.GeneralSecurityException;
import java.security.KeyStore;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;

import net.netheos.pcsapi.credentials.AppInfoRepository;
import net.netheos.pcsapi.credentials.UserCredentialsRepository;
import net.netheos.pcsapi.storage.IStorageProvider;
import net.netheos.pcsapi.utils.PcsUtils;

import org.apache.http.client.HttpClient;
import org.apache.http.conn.ClientConnectionManager;
import org.apache.http.conn.scheme.PlainSocketFactory;
import org.apache.http.conn.scheme.Scheme;
import org.apache.http.conn.scheme.SchemeRegistry;
import org.apache.http.conn.ssl.SSLSocketFactory;
import org.apache.http.impl.client.DefaultHttpClient;
import org.apache.http.impl.conn.PoolingClientConnectionManager;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import javax.net.ssl.SSLContext;
import javax.net.ssl.TrustManager;
import javax.net.ssl.TrustManagerFactory;

public class StorageProviderFactory
{

    private static final Logger LOGGER = LoggerFactory.getLogger( StorageProviderFactory.class );

    // Do you enjoy static fields ? Junit does !
    private static AppInfoRepository appRepo;
    private static UserCredentialsRepository credRepo;

    /**
     * junit factory for injection of storage object into test constructors.
     *
     * @return collection of storage providers
     * @throws IOException
     */
    public static Collection<Object[]> storageProviderFactory()
            throws IOException
    {
        String path = System.getProperty( "pcsapiRepositoryDir" );
        if ( path == null ) {
            path = System.getenv( "PCS_API_REPOSITORY_DIR" );
        }
        if ( path == null ) {
            path = "../repositories";
        }

        File repository = new File( path );
        if ( !repository.isDirectory() ) {
            LOGGER.warn( "No repository defined for functional tests."
                         + " Set PCS_API_REPOSITORY_DIR environment variable,"
                         + " or set pcsapiRepositoryDir system property" );
            LOGGER.warn( "No functional test will be run" );
            return Collections.EMPTY_LIST; // list of providers to be tested
        }

        String providers = System.getProperty( "pcsapiProviders" );

        boolean allProviders = false;
        if ( providers == null ) {
            allProviders = true;
        }

        File appRepoFile = new File( repository, "app_info_data.txt" );
        appRepo = new AppInfoFileRepository( appRepoFile );

        File credRepoFile = new File( repository, "user_credentials_data.txt" );
        credRepo = new UserCredentialsFileRepository( credRepoFile );

        Collection<Object[]> values = new ArrayList<Object[]>();
        for ( String providerName : StorageFacade.getRegisteredProviders() ) {
            if ( allProviders || providers.contains( providerName ) ) {

                HttpClient httpClient = buildDedicatedHttpClient( providerName );
                addProvider( values, providerName, httpClient );
            }
        }

        return values;
    }

    /**
     * Instantiate provider storage for given name, and add it to values collection.
     *
     * A single application must exist for this provider in application repository. A single user must exist for this
     * application in user credentials repository.
     *
     * @param values destination (for unit test constructors)
     * @param providerName name of provider for which storage must be created.
     * @param optHttpClient if not null, client to use for the provider. If null default client is used.
     */
    private static void addProvider( Collection<Object[]> values, String providerName, HttpClient optHttpClient )
    {
        StorageBuilder builder = StorageFacade.forProvider( providerName )
                .setAppInfoRepository( appRepo, appRepo.get( providerName ).getAppName() )
                .setUserCredentialsRepository( credRepo, null );

        if ( optHttpClient != null ) {
            builder.setHttpClient( optHttpClient );
        }

        values.add( new Object[]{ builder.build() } );
    }

    /**
     * Builds a specific HttpClient to certain providers
     *
     * @param providerName
     * @return client to be used, or null if default should be used.
     */
    private static HttpClient buildDedicatedHttpClient( String providerName )
            throws IOException
    {
        /**
         * Basic java does not trust CloudMe CA CloudMe CA needs to be added
         */
        if ( providerName.equals( "cloudme" ) && !PcsUtils.ANDROID ) {
            try {
                KeyStore ks = KeyStore.getInstance( "JKS" );
                InputStream is = null;

                try {
                    is = StorageProviderFactory.class.getResourceAsStream( "/cloudme.jks" );
                    ks.load( is, "changeit".toCharArray() );
                } finally {
                    PcsUtils.closeQuietly( is );
                }

                SSLContext context = SSLContext.getInstance( "TLS" );
                TrustManagerFactory caTrustManagerFactory = TrustManagerFactory.getInstance( "SunX509" );
                caTrustManagerFactory.init( ks );
                context.init( null, caTrustManagerFactory.getTrustManagers(), null );

                SchemeRegistry schemeRegistry = new SchemeRegistry();
                schemeRegistry.register( new Scheme( "http", 80, new PlainSocketFactory() ) );
                schemeRegistry.register( new Scheme( "https", 443, new SSLSocketFactory( context ) ) );

                ClientConnectionManager cnxManager = new PoolingClientConnectionManager( schemeRegistry );

                return new DefaultHttpClient( cnxManager );

            } catch ( GeneralSecurityException ex ) {
                throw new UnsupportedOperationException( "Can't configure HttpClient for Cloud Me", ex );
            }
        }

        return null;
    }

}
