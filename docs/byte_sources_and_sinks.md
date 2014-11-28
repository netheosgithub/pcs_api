Byte sources and sinks
======================

Input data for an upload operation is provided by a `ByteSource` object.
A byte source object knows data length in advance, and can provide a file like object
(or `java.io.InputStream`, or `std::istream`) for reading bytes. The reading must be repeatable.
Several implementations are provided:

- `FileByteSource` (data is read from a plain file)
- `MemoryByteSource` (data is read from a `bytesio` object)

Data downloaded from cloud storage is written into a `ByteSink` object. Several implementations are provided:

- `FileByteSink`
- `MemoryByteSink`

Here is how to download a range of remote file, and be notified of download operation:

In Python
```python
# Prerequisite: you have instantiated a 'storage' object
bs = FileByteSink('dest_file.txt')
# downloads only 10^6 bytes, starting at offset 1000:
dr = CDownloadRequest(CPath('/remote_source_file.txt'), bs).range(1000, 1000000)
dr.progress_listener(StdoutProgressListener())  # or use your own progress listener here
storage.download(dr)
```
In Java
```java
// Prerequisite: you have instantiated a 'storage' object
ByteSink bs = new FileByteSink("dest_file.txt");
// downloads only 10^6 bytes, starting at offset 1000:
CDownloadRequest dr = new CDownloadRequest(new CPath("/remote_source_file.txt"), bs);
dr.setRange(1000, 1000000);
dr.setProgressListener(new StdoutProgressListener());  // or use your own progress listener here
storage.download(dr);
```
In C++
```C++
#include <memory>
#include "pcs_api/model.h"
#include "pcs_api/stdout_progress_listener.h"
#include "pcs_api/file_byte_sink.h"

// Prerequisite: you have instantiated a p_storage object
std::shared_ptr<pcs_api::FileByteSink> bs =
    std::make_shared<FileByteSink>(boost::filesystem::path("dest_file.txt"));
// downloads only 10^6 bytes, starting at offset 1000:
pcs_api::CDownloadRequest download_request(pcs_api::CPath(PCS_API_STRING_T("/remote_source_file.txt")), bs);
download_request.SetRange(1000, 1000000);
p_storage->Download(download_request);
```

In C++ the PCS_API_STRING_T macro expands to U (for 16 bits wide chars on Windows platforms) or nothing
(for narrow chars on other platforms: all paths are UTF-8 encoded).


FileByteSink constructor has some options (all default to false):

- can download to a temporary filename ".part" (a la Firefox). Only when download completes successfully
  is the local file renamed to the specified name.
- can delete any partially downloaded file, in case download operation fails.

As shown above, it is possible to use range downloads to download the end of a partially downloaded file.

In Python and Java implementations, progress listener callback methods are called from the thread
that has initiated the upload/download operation.
In C++, this is valid only for download operations. Uploads are different and use different threads
(active thread may even change during upload).

Note that during downloads, downloaded size might not be known in advance (if server uses chunked encoding),
so a progress listener does not know total progress until the end of download.

Interrupting a lengthly upload/download operation can be achieved by raising an exception from progress listener
callback. However in C++ the raised exception is lost, and the current upload fails always with a CStorageException.
See limitations of Linux C++ implementation in page [advanced](advanced.md).

User of pcs_api can use his/her own `ByteSource` and `ByteSink` objects in order to fulfill his/her needs.
However keep in mind that reading/writing data must be repeatable, and for reading, total length must be known in advance
(so an on-the-fly compression algorithm can not be used ; this constraint comes from providers that usually do not support
chunked encoding uploads).
Repeatability of `ByteSource` is required in case request fails temporarily.
