Providers specifics
===================

All providers support basic operations, like folders, list, upload and downloads.

Some details change however:

####hubiC####

- Only hubiC keeps content-type specified at file upload time.
- hubiC sometimes returns already deleted files when listing a folder. The web application is also quite slow
  to show changes performed by pcs_api requests.
- hubiC storage is based on openstack Swift. This storage is 'flat', not hierarchical. Files hierarchy is
  simulated by slash separators in objects names, and by empty objects with content-type 'application/directory'
  (without these empty objects, no empty folder could exist in hubiC web interface). pcs_api takes care of these
  objets management. Deleting a folder implies one delete request per object to be deleted: this can be slow
  if folder contains many objects.
- Folders containing a large number of files (>10000) may be incompletely listed.

####CloudMe####

- Content-type is ignored during upload (content-type seems to be guessed from file extension).
- does not support quotes (") in blob names.
- CloudMe server SSL certificate is not known by Oracle JVM: CA certificate is bundled in unit tests.
  This problem does not appear on Android.
- For free accounts, upload size is limited to 150MB.

####Google Drive####

- Content-type is ignored during upload (content-type seems to be guessed from file extension).
- Google Drive documents (with content-type application/vnd.google-apps.xxx) are not downloadable per se. pcs_api lists
  them as blobs with a negative length, and a `CInvalidFileTypeError` is raised in case of download attempt.
- Google Drive sometimes still presents a deleted folder even after it has been shortly trashed.
  A freshly uploaded file may not be found for download, or, if file has been updated, the old version may be returned.

####Dropbox####

- Content-type is ignored during upload (content-type seems to be guessed from file extension).
- For free accounts, upload size is limited to 150MB.
- A blob is erased when creating a folder whose path begins with blob path (such a folder is implicitely created
  when uploading a blob to a path that begins with blob path).

  Example: Blob at path '/foo/bar' is erased if a folder with name '/foo/bar/1/2' is created:
  '/foo/bar' will become a folder.

