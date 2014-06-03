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

import net.netheos.pcsapi.exceptions.CStorageException;
import net.netheos.pcsapi.models.CDownloadRequest;
import net.netheos.pcsapi.models.CFile;
import net.netheos.pcsapi.models.CFolder;
import net.netheos.pcsapi.models.CPath;
import net.netheos.pcsapi.models.CQuota;
import net.netheos.pcsapi.models.CUploadRequest;

import net.netheos.pcsapi.models.CFolderContent;

/**
 * Reference interface for storage providers.
 */
public interface IStorageProvider
{

    /**
     * Get the provider name
     *
     * @return The provider name (ie. "dropbox")
     */
    String getProviderName();

    /**
     * Return user identifier (login in case of login/password, or email in case of OAuth
     *
     * @return user identifier (login in case of login/password, or email in case of OAuth
     * @throws CStorageException Error getting the user identifier
     */
    String getUserId()
            throws CStorageException;

    /**
     * Returns a CQuota object
     *
     * @return a tuple: used bytes, allowed bytes.
     * @throws CStorageException Error getting the quota
     */
    CQuota getQuota()
            throws CStorageException;

    /**
     * Equivalent to {@link IStorageProvider#listFolder(net.netheos.pcsapi.models.CPath)} with "/"
     *
     * @return TODO
     * @throws CStorageException Error getting the root folder
     */
    CFolderContent listRootFolder()
            throws CStorageException;

    /**
     * Return a map of files present in given CPath. keys of map are CPath objects, values are CFile objects (CFolder or
     * CBlob). Return None if no folder exists at given path. Raise CInvalidFileTypeError if given path is a blob. Note
     * : objects in returned map may have incomplete information
     *
     * @param path The folder path
     * @return a map of files present at given CPath. keys of map are CPath objects, values are CFile objects (CFolder
     * or CBlob). Return null if no folder exists at given path. Throws CInvalidFileTypeError if given path is a blob.
     * @throws CStorageException Error getting the files in the folder
     */
    CFolderContent listFolder( CPath path )
            throws CStorageException;

    /**
     * Return a map of files present in given CFolder. keys of map are CPath objects, values are CFile objects (CFolder
     * or CBlob). Return None if no folder exists at given path. Raise CInvalidFileTypeError if given path is a blob.
     * Note : objects in returned map may have incomplete information
     *
     * @param folder The folder to get the files in
     * @return a map of files present in given CFolder keys of map are CPath objects, values are CFile objects (CFolder
     * or CBlob). Return null if no folder exists at given path. Note : objects in returned map may have incomplete
     * information.
     * @throws CStorageException Error getting the files in the folder
     */
    CFolderContent listFolder( CFolder folder )
            throws CStorageException;

    /**
     * Create a folder at given path, with intermediary folders if needed. Throws CInvalidFileType error if a blob
     * exists at this path.
     *
     * @param path The folder path to create
     * @return true if folder has been created, false if it was already existing.
     * @throws CStorageException Error creating the folder
     */
    boolean createFolder( CPath path )
            throws CStorageException;

    /**
     * Deletes blob, or recursively delete folder at given path.
     *
     * @param path The file path to delete
     * @return true if at least one file was deleted, false if no object was existing at this path.
     * @throws CStorageException Error deleting the file
     */
    boolean delete( CPath path )
            throws CStorageException;

    /**
     * Return detailed file information at given path, or None if no object exists at this path
     *
     * @param path The file path
     * @return detailed file information at given path, or null if no object exists at this path
     * @throws CStorageException Error getting the file
     */
    CFile getFile( CPath path )
            throws CStorageException;

    /**
     * Downloads a blob from provider to a byte sink, as defined by the download_request object. Throws
     * CFileNotFoundError if no blob exists at this path. Throws CInvalidFileTypeError if a folder exists at specified
     * path.
     *
     * @param downloadRequest The download request object
     * @throws CStorageException Download error
     */
    void download( CDownloadRequest downloadRequest )
            throws CStorageException;

    /**
     * Uploads a byte source to provider, as defined by upload_request object. If a blob already exists it is replaced.
     * Throws CInvalidFileTypeError if a folder already exists at specified path.
     *
     * @param uploadRequest The upload resuest object
     * @throws CStorageException Upload error
     */
    void upload( CUploadRequest uploadRequest )
            throws CStorageException;

    /**
     * Close the provider. The internal objets will be released
     *
     * @throws CStorageException Close error
     */
    void close()
            throws CStorageException;

}
