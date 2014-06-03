Byte sources and sinks
======================

Input data for an upload operation is provided by a ByteSource object. A byte source object knows data length in advance, and can provide a file like object for reading bytes. The reading must be repeatable.
Several implementations are provided:

- FileByteSource (data is read from a plain file)
- MemoryByteSource (data is read from a `bytesio` object)
- RangeByteSource (decorator: view of a range of an underlying byte source)

Data downloaded from cloud storage is written into a ByteSink object. Several implementations are provided:

- FileByteSink
- MemoryByteSink

Here is how to download a range of remote file, and be notified of download operation:

In Python
```python
bs = FileByteSink('dest_file.txt')
# downloads only 10^6 bytes, starting at offset 1000:
dr = CDownloadRequest(CPath('/remote_source_file.txt'), bs).range(1000, 1000000)
dr.progress_listener(StdoutProgressListener())  # or use your own progress listener here
storage.download(dr)
```
In Java
```java
ByteSink bs = new FileByteSink("dest_file.txt");
// downloads only 10^6 bytes, starting at offset 1000:
CDownloadRequest dr = new CDownloadRequest(new CPath("/remote_source_file.txt"), bs);
dr.setRange(1000, 1000000);
dr.setProgressListener(new StdoutProgressListener());  // or use your own progress listener here
storage.download(dr);
```

FileByteSink constructor has some options (all default to false):

- can download to a temporary filename ".part" (a la Firefox). Only when download completes successfully
  is the local file renamed to the specified name.
- can delete any partially downloaded file, in case download operation fails.

It is possible to use range downloads to download the end of a partially downloaded file.

In Python and Java implementations, progress listener callback methods are called from the thread
that has initiated the upload/download operation.

Note that during downloads, downloaded size may not be known in advance (if server uses chunked encoding),
so a progress listener does not know total progress until the end of download.

Interrupting a lengthly upload/download operation can be achieved by raising an exception from progress listener
callback.

User of pcs_api can use his/her own ByteSource and ByteSink objects in order to fulfill his/her needs.
However keep in mind that reading/writing data must be repeatable, and for reading, total length must be known in advance
(so an on-the-fly compression algorithm can not be used ; this constraint comes from providers that usually do not support
chunked encoding uploads).
Repeatability is required in case request fails temporarily.
