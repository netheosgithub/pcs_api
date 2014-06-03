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

package net.netheos.pcsapi.sample;

import android.content.Context;
import android.os.AsyncTask;

import net.netheos.pcsapi.bytesio.ByteSink;
import net.netheos.pcsapi.bytesio.FileByteSink;
import net.netheos.pcsapi.bytesio.MemoryByteSink;
import net.netheos.pcsapi.bytesio.MemoryByteSource;
import net.netheos.pcsapi.bytesio.StdoutProgressListener;
import net.netheos.pcsapi.credentials.AppInfoFileRepository;
import net.netheos.pcsapi.credentials.AppInfoRepository;
import net.netheos.pcsapi.credentials.UserCredentialsFileRepository;
import net.netheos.pcsapi.credentials.UserCredentialsRepository;
import net.netheos.pcsapi.models.CBlob;
import net.netheos.pcsapi.models.CDownloadRequest;
import net.netheos.pcsapi.models.CFile;
import net.netheos.pcsapi.models.CFolder;
import net.netheos.pcsapi.models.CFolderContent;
import net.netheos.pcsapi.models.CPath;
import net.netheos.pcsapi.models.CUploadRequest;
import net.netheos.pcsapi.storage.IStorageProvider;
import net.netheos.pcsapi.storage.StorageFacade;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.io.File;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Deque;
import java.util.LinkedList;
import java.util.List;

/**
 * Load all files from a given provider.
 * To execute this class, a file named "app_info_data.txt" must be written in the application sandbox.
 * This file is described in the documentation https://github.com/Netheos/pcs_api/blob/master/docs/oauth2.md#appinfofilerepository.
 *
 * This class is only for test use. DO NOT use it in a production environment.
 */
public class ProviderFilesLoader extends AsyncTask<Void, Void, List<CFile>> {

    private static final Logger LOGGER = LoggerFactory.getLogger(ProviderFilesLoader.class);

    private final Context mContext;
    private final String mProviderName;
    private final File mCredentialsFile;
    private final File mAppInfoFile;

    public ProviderFilesLoader(String providerName, File appInfoFile, File credentialsFile, Context context) {
        mProviderName = providerName;
        mContext = context;
        mAppInfoFile = appInfoFile;
        mCredentialsFile = credentialsFile;
    }

    @Override
    protected List<CFile> doInBackground(Void... params) {
        IStorageProvider storage = null;
        try {
            List<CFile> allFiles = new ArrayList<CFile>();

            // Get the application repository (containing the applications informations)
            AppInfoRepository appInfoRepo = new AppInfoFileRepository(mAppInfoFile);

            // Get the application repository (containing the user credentials)
            UserCredentialsRepository userCredentialsRepo = new UserCredentialsFileRepository(mCredentialsFile);

            // Build the provider
            try {
                storage = StorageFacade.forProvider(mProviderName)
                        .setAppInfoRepository(appInfoRepo, appInfoRepo.get(mProviderName).getAppName())
                        .setUserCredentialsRepository(userCredentialsRepo, null)
                        .build();
            } catch (IllegalStateException e){
                cancel(true);
                return null;
            }

            LOGGER.info("user_id = {}", storage.getUserId());
            LOGGER.info("quota = {}", storage.getQuota());

            // Recursively list all regular files (blobs in folders)
            CFolder root = new CFolder(CPath.ROOT);
            Deque<CFolder> foldersToProcess = new LinkedList<CFolder>(Arrays.asList(root));
            CBlob largestBlob = null;
            while (!foldersToProcess.isEmpty()) {
                CFolder f = foldersToProcess.pollLast();

                // List folder
                LOGGER.info("Content of {}:", f);
                CFolderContent content = storage.listFolder(f);
                if (content == null) {
                    LOGGER.error("No content for folder (deleted in background ?) : {}", f);
                    continue;
                }
                List<CBlob> blobs = getFilesByType(content, CBlob.class);
                List<CFolder> folders = getFilesByType(content, CFolder.class);
                allFiles.addAll(blobs);
                allFiles.addAll(folders);

                // Print blobs of this folder
                for (CBlob blob : blobs) {
                    LOGGER.info("{}", blob);
                    if (largestBlob == null || blob.length() > largestBlob.length()) {
                        largestBlob = blob;
                    }
                }
                for (CFolder folder : folders) {
                    LOGGER.info("{}", folder);
                }
                // Add folders to list
                foldersToProcess.addAll(folders);
            }

            // Other tests... deactivated for now
            //testOtherFeatures(storage, largestBlob);

            return allFiles;

        } catch(Exception ex) {
            LOGGER.error("An error occurred during execution", ex);
            return null;
        } finally {
            if (storage != null) {
                storage.close();
            }
        }
    }

    private void testOtherFeatures(IStorageProvider storage, CBlob largestBlob) throws IOException{
        // Create a folder and upload a test file
        CFolderContent rootFolderContent = storage.listRootFolder();
        // root_folder_content is a dict
        // keys are files paths, values are CFolder or CBlob providing details (length, content-type...)
        LOGGER.info("root folder content: {}", rootFolderContent);

        // Create a new folder
        CPath fpath = new CPath("/pcs_api_new_folder");
        storage.createFolder(fpath);

        // Upload a local file in this folder
        CPath bpath = fpath.add("pcs_api_new_file");
        byte[] fileContent = "this is file content...".getBytes("UTF-8");
        CUploadRequest uploadRequest = new CUploadRequest(bpath, new MemoryByteSource(fileContent));
        uploadRequest.setContentType("text/plain");
        storage.upload(uploadRequest);

        // Download back the file
        MemoryByteSink mbs = new MemoryByteSink();
        CDownloadRequest downloadRequest = new CDownloadRequest(bpath, mbs);
        storage.download(downloadRequest);
        if(!Arrays.equals(mbs.getData(), fileContent)) {
            throw new IOException("Downloaded data is different from source");
        }

        // Delete remote folder
        storage.delete(fpath);

        // Example : we'll download a range of largest blob
        if(largestBlob != null) {
            long rangeStart = largestBlob.length() / 2;
            long rangeLength = Math.min(largestBlob.length() / 2, 1000000);
            ByteSink bs = new FileByteSink(new File(mContext.getFilesDir(), "dest_file.txt"));
            LOGGER.info("Will download range from largest blob : {} to : {}", largestBlob, bs);

            CDownloadRequest dr = new CDownloadRequest(largestBlob.getPath(), bs);
            // Watch download progress (just an example, to android log) :
            dr.setProgressListener(new StdoutProgressListener());
            dr.setRange(rangeStart, rangeLength);
            storage.download(dr);
        }
    }

    private <T extends CFile> List<T> getFilesByType(CFolderContent content, Class<T> type) {
        List<T> blobs = new ArrayList<T>();
        for (CFile file : content) {
            if (type.isInstance(file)) {
                blobs.add((T) file);
            }
        }
        return blobs;
    }
}
