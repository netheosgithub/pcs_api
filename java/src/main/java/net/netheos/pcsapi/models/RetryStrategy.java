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

import java.util.concurrent.Callable;
import net.netheos.pcsapi.exceptions.CRetriableException;
import net.netheos.pcsapi.exceptions.CStorageException;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

/**
 * Manages retriable requests and sets retry strategy: number of attempts and delay between attempts.
 */
public class RetryStrategy
{

    private static final Logger LOGGER = LoggerFactory.getLogger( RetryStrategy.class );
    private final int nbTriesMax;
    private final long firstSleep_ms;

    public RetryStrategy( int nbTriesMax, long firstSleep_ms )
    {
        this.nbTriesMax = nbTriesMax;
        this.firstSleep_ms = firstSleep_ms;
    }

    /**
     * Main method to be called by user of this class : calls {@link Callable#call()} until success, non retriable error
     * or max trials has been reached.
     *
     * @param <T> The request result type
     * @param invoker The request invoker which execute and validate the request
     * @return The request result
     * @throws CStorageException Request execution error
     */
    public <T> T invokeRetry( Callable<T> invoker )
            throws CStorageException
    {
        int currentTries = 0;

        while ( true ) {

            currentTries++;

            try {
                LOGGER.debug( "Invokation #{}", currentTries );
                return invoker.call();

            } catch ( CRetriableException ex ) {
                if ( currentTries >= nbTriesMax ) {
                    LOGGER.debug( "Aborting invocation after {} attempts", nbTriesMax );
                    Throwable cause = ex.getCause();
                    if ( cause instanceof CStorageException ) {
                        throw ( CStorageException ) cause;
                    }
                    throw new CStorageException( "Invocation failure", cause );
                }

                LOGGER.debug( "Catching a CRetriableException, {} out of {} max attempts", currentTries, nbTriesMax );
                doWait( currentTries, ex.getDelay() );

            } catch ( CStorageException ex ) {
                throw ex;

            } catch ( Exception ex ) {
                throw new CStorageException( "Invocation failure", ex );
            }
        }
    }

    /**
     * Sleeps before retry ; default implementation is exponential back-off, or the specified duration
     *
     * @param currentTries 1 after first fail, then 2, 3 ... up to nbTriesMax-1.
     * @param optDuration_ms if positive, the delay to apply
     */
    private void doWait( int currentTries, long optDuration_ms )
    {
        if ( optDuration_ms < 0 ) {
            optDuration_ms = ( long ) ( firstSleep_ms * ( Math.random() + 0.5 ) * ( 1L << ( currentTries - 1 ) ) );
        }

        LOGGER.debug( "Will retry request after {} millis", optDuration_ms );

        try {
            Thread.sleep( optDuration_ms );
        } catch ( InterruptedException ex ) {
            throw new CStorageException( "Retry waiting interrupted", ex );
        }
    }

}
