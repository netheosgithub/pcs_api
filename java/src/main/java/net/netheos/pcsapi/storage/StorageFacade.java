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

package net.netheos.pcsapi.storage;

import net.netheos.pcsapi.providers.cloudme.CloudMe;
import net.netheos.pcsapi.providers.dropbox.Dropbox;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.HashMap;
import java.util.Map;
import java.util.Set;
import net.netheos.pcsapi.providers.googledrive.GoogleDrive;
import net.netheos.pcsapi.providers.hubic.Hubic;

/**
 * Registers and supplies all available storage providers
 */
public final class StorageFacade
{

    private static final Logger LOGGER = LoggerFactory.getLogger( StorageFacade.class );
    private static final Map<String, Class> PROVIDERS = new HashMap<String, Class>();

    static {
        registerProvider( Dropbox.PROVIDER_NAME, Dropbox.class );
        registerProvider( Hubic.PROVIDER_NAME, Hubic.class );
        registerProvider( GoogleDrive.PROVIDER_NAME, GoogleDrive.class );
        registerProvider( CloudMe.PROVIDER_NAME, CloudMe.class );
    }

    /**
     * Get the registered {@link StorageProvider}
     *
     * @return The registered providers names
     */
    public static Set<String> getRegisteredProviders()
    {
        return PROVIDERS.keySet();
    }

    /**
     * Register a new {@link StorageProvider}
     *
     * @param providerName The name to use with the provider
     * @param providerClass The provider class
     */
    public static void registerProvider( String providerName, Class<? extends StorageProvider> providerClass )
    {
        if ( PROVIDERS.containsKey( providerName ) ) {
            throw new IllegalArgumentException( "A provider already exists with the name " + providerName );
        }
        PROVIDERS.put( providerName, providerClass );
        LOGGER.debug( "Registering new provider: {}", providerClass.getSimpleName() );
    }

    /**
     * Get a {@link StorageBuilder} by its name
     *
     * @param providerName The provider name
     * @return The builder used to configure a {@link StorageProvider}
     */
    public static StorageBuilder forProvider( String providerName )
    {
        Class<?> providerClass = PROVIDERS.get( providerName );

        if ( providerClass == null ) {
            throw new IllegalArgumentException( "No provider implementation registered for name: " + providerName );
        }

        return new StorageBuilder( providerName, providerClass );
    }

    private StorageFacade()
    {
    }

}
