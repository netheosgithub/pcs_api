# -*- coding: utf-8 -*-
#
# Copyright (c) 2014 Netheos (http://www.netheos.net)
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#


from __future__ import absolute_import, unicode_literals, print_function
import io
import os
import logging
import sys

LOGGER = logging.getLogger(__name__)


class ProgressListener(object):

    def set_progress_total(self, total):
        """Called when total number of elements to process is known
        (usually at start time, but may be at the end of the process if chunked download).
        May be called several times (in case an upload or download fails and is restarted).

        :param total number of elements to process, or None if unknown (chunked encoding)"""
        pass

    def progress(self, current):
        """Called when observed lengthly operation has made some progress.

        Called once with current=0 to indicate process is starting.
        Note that progress may restart from 0 (in case an upload or download fails and is restarted).

        :param current Number of elements processed so far"""
        pass

    def aborted(self):
        """Called when current operation is aborted (may be retried)"""
        pass


class StdoutProgressListener(ProgressListener):
    """A simple progress listener that outputs to stdout"""
    def __init__(self):
        super(StdoutProgressListener, self).__init__()
        self.total = None
        self.current = 0
        self.is_aborted = False

    def set_progress_total(self, total):
        self.total = total

    def progress(self, current):
        self.current = current
        if current == 0:
            print('\n')
        print("Progress: %r / %r\r" % (current, self.total if self.total is not None else '???'), end='')
        sys.stdout.flush()
        if current == self.total:
            print('\n***** END OF PROGRESS ****')

    def aborted(self):
        self.is_aborted = True
        print("\nprocess has been aborted")


def not_implemented(method):
    raise NotImplementedError("%s is to be defined by derived classes" % (method,))


def not_supported(method):
    raise io.UnsupportedOperation("%s is not supported" % (method,))


class ByteSource(object):

    def open_stream(self):
        """Returns a ByteSourceStream object for reading data, to be closed by caller"""
        not_implemented(self.open_stream)

    def length(self):
        """Return length of stream (must be known before consuming stream)"""
        not_implemented(self.length)


class FileByteSource(ByteSource):

    def __init__(self, filename):
        self._filename = filename

    def open_stream(self):
        stream = io.FileIO(self._filename, 'rb')
        hack_stream_for_requests(stream, self.length())
        return stream

    def length(self):
        return os.path.getsize(self._filename)

    def __str__(self):
        return 'FileByteSource "%s"' % (self._filename,)


class MemoryByteSource(ByteSource):
    def __init__(self, data):
        self._data = data

    def open_stream(self):
        stream = io.BytesIO(self._data)
        hack_stream_for_requests(stream, self.length())
        return stream

    def length(self):
        return len(self._data)

class RangeByteSource(ByteSource):
    """A byte source view of a range of bytes from an underlying byte source.

    TODO only 1 RangeByteSource is acceptable in pipeline, because of seek()"""

    def __init__(self, byte_source, start_offset=0, length=None):
        """The byte source must be seekable"""
        source_length = byte_source.length()
        if start_offset >= source_length:
            raise ValueError("start_offset is past source length: %r >= %r" % (start_offset, source_length))
        if length:
            if length < 0:
                raise ValueError("Invalid length value: %r" % length)
            end_of_range = start_offset + length
            if end_of_range > source_length:
                raise ValueError("Range is past source length: %r > %r" % (end_of_range, source_length))
        else:
            length = source_length - start_offset
        self._byte_source = byte_source
        self._start = start_offset
        self._length = length

    def open_stream(self):
        in_stream = self._byte_source.open_stream()
        in_stream.seek(self._start)
        stream = LimitedInputStream(in_stream, self._length)
        hack_stream_for_requests(stream, self.length(), in_stream)
        return stream

    def length(self):
        return self._length

    def __str__(self):
        return 'Range [start=%d, len=%d] of %s: ' % (self._start, self._length, self._byte_source)


class ProgressByteSource(ByteSource):
    """A byte source that notifies a ProgressListener ;
    data is read from underlying ByteSource.

    For pcsapi internal use only; use CUploadRequest.progress_listener(pl) instead."""

    def __init__(self, byte_source, progress_listener):
        self._byte_source = byte_source
        self._progress_listener = progress_listener

    def open_stream(self):
        in_stream = self._byte_source.open_stream()
        stream = ProgressInputStream(in_stream, self._progress_listener)
        hack_stream_for_requests(stream, self.length(), in_stream)
        # notify 0 to indicate process is starting:
        self._progress_listener.set_progress_total(self.length())
        self._progress_listener.progress(0)
        return stream

    def length(self):
        return self._byte_source.length()

    def __str__(self):
        return 'Progress of %s' % (self._byte_source,)


class LimitedInputStream(object):
    """A stream that reads data from an underlying stream,
    but stops reading after a limit has been reached"""

    def __init__(self, in_stream, limit):
        self.in_stream = in_stream
        self._remaining = limit
        # directly forwarded methods:
        self.close = in_stream.close
        self.flush = in_stream.flush
        self.isatty = in_stream.isatty
        self.readable = in_stream.readable

    def __getattr__(self, name):
        if name == "closed":
            return self.in_stream.closed
        raise AttributeError("%r object has no attribute %r" % (self.__class__, name))

    def read(self, n=-1):
        # We shall not read past end of range:
        if n >= 0:
            n = min(n, self._remaining)
        else:
            n = self._remaining
        data = self.in_stream.read(n)
        if data:
            self._remaining -= len(data)
        return data

    def readall(self):
        # We have to loop here:
        b = bytearray(self._remaining)
        mv = memoryview(b)
        while self._remaining > 0:
            n = self.in_stream.readinto(mv[len(b)-self._remaining:])
            if n == 0:
                break
            self._remaining -= n
        return b

    def readinto(self, b):
        # We shall not read past end of range:
        toread = len(b)
        if toread > self._remaining:
            mv = memoryview(b)
            ret = self.in_stream.readinto(mv[0:self._remaining])
        else:
            ret = self.in_stream.readinto(b)
        self._remaining -= ret
        return ret

    def readline(self, limit=-1):
        raise io.UnsupportedOperation("readline is not supported by %r" % self.__class__)

    def readlines(self, hint=-1):
        raise io.UnsupportedOperation("readlines is not supported by %r" % self.__class__)

    def seekable(self):
        return False

    def seek(self, offset, whence):
        not_supported(self.seek)

    def tell(self):
        not_supported(self.tell)

    def fileno(self):
        # We can not forward fileno because this object is not a direct view of the system file (length differ):
        not_supported(self.fileno)

    def writable(self):
        return False

    def write(self, b):
        not_supported(self.write)

    def truncate(self, size=None):
        not_supported(self.truncate)

    def writelines(self, lines):
        not_supported(self.writelines)


class ProgressInputStream(object):
    """A stream that counts data read from an underlying stream.

    seek() should not be called on such streams"""

    def __init__(self, stream, progress_listener):
        """
        :param stream: the stream to watch
        :param progress_listener: will be called during stream reading"""
        self.stream = stream
        self.progress_listener = progress_listener
        self.current_bytes = 0
        # directly forwarded methods:
        self.close = stream.close
        self.flush = stream.flush
        self.isatty = stream.isatty
        self.seekable = stream.seekable
        self.seek = stream.seek
        self.tell = stream.tell
        self.fileno = stream.fileno
        self.readable = stream.readable
        self.writable = stream.writable
        self.truncate = stream.truncate

    def get_current_bytes(self):
        return self.current_bytes

    def _add_bytes(self, n):
        self.current_bytes += n
        self.progress_listener.progress(self.current_bytes)

    def __getattr__(self, name):
        if name == "closed":
            return self.stream.closed
        raise AttributeError("%r object has no attribute %r" % (self.__class__, name))

    def read(self, n=-1):
        data = self.stream.read(n)
        if data:
            self._add_bytes(len(data))
        return data

    def readall(self):
        data = self.stream.readall()
        if data:
            self._add_bytes(len(data))
        return data

    def readinto(self, b):
        n = self.stream.readinto(b)
        self._add_bytes(n)
        return n

    def write(self, b):
        not_supported(self.write)

    def readline(self, limit=-1):
        not_supported(self.readline)

    def readlines(self, hint=-1):
        not_supported(self.readlines)

    def writelines(self, lines):
        not_supported(self.writelines)


def hack_stream_for_requests(stream, length, decorated_stream=None):
    """Stream will be given to python requests as 'data'.
    requests has some heuristics to determine if it is a file-like object
    and also to determine content length.
    See models.py / prepare_body() and utils.py / super_len()."""
    stream.len = length
    if decorated_stream is not None:
        try:
            stream.__iter__ = decorated_stream.__iter__
        except AttributeError:
            pass


class ByteSink(object):

    def open_stream(self):
        """Return a ByteSinkStream object for writing data, to be closed by caller.

        This method may be called several times (in case of retries)"""
        raise not_implemented(self, "open_stream")

    def set_expected_length(self, length):
        """Define the number of bytes that are expected to be written to the stream.
        This value may be defined lately (after stream creation).

        Note that this length may differ from the final data size, for example if
        bytes are appended to an already existing file."""
        pass


class FileByteSink(ByteSink):

    def __init__(self, filename,
                 temp_name_during_writes=False,
                 delete_on_abort=False,
                 mode='wb'):
        """Create a byte sink where bytes are written to a file.

        :param filename: name of output file
        :param temp_name_during_writes: if True, data will first be written to tempfile 'filename.part'
               and when this file stream is closed properly (without having been aborted) it is renamed to 'filename'
        :param delete_on_abort: if True, created file is deleted if stream is aborted or not closed properly.
        :param mode: mode for opening file
        """
        self._filename = filename
        self._temp_name_during_writes = temp_name_during_writes
        self._delete_on_abort = delete_on_abort
        self._mode = mode
        self._expected_length = None
        self._aborted = False

    def open_stream(self):
        actual_filename = self._get_actual_filename()
        stream = io.FileIO(actual_filename, self._mode)
        stream.close = self._close_stream_handle_file(stream.close)
        stream.abort = self._abort
        self._aborted = False
        return stream

    def set_expected_length(self, length):
        self._expected_length = length

    def _get_actual_filename(self):
        if self._temp_name_during_writes:
            return self._filename + ".part"
        else:
            return self._filename

    def _close_stream_handle_file(self, orig_stream_close):
        def close():
            # Always close stream:
            closed_properly = False
            try:
                orig_stream_close()
                closed_properly = True
            finally:
                if self._aborted:
                    LOGGER.info("sink process has been aborted")
                if self._delete_on_abort and not closed_properly:
                    LOGGER.warn("sink file not closed properly: will be deleted")
                # Now two cases: it worked fine or it was aborted (or close failed)
                if self._aborted or not closed_properly:
                    actual_filename = self._get_actual_filename()
                    if self._delete_on_abort:
                        LOGGER.info("will delete sink file: %s", actual_filename)
                        os.remove(actual_filename)
                    else:
                        # TODO transferred length is not always equals to file length (mode=append)
                        actual_file_length = os.path.getsize(actual_filename)
                        if self._expected_length is not None:
                            actual_file_length = os.path.getsize(actual_filename)
                            if actual_file_length == self._expected_length:
                                LOGGER.info("sink file is complete: %s (%d bytes)",
                                            actual_filename, actual_file_length)
                            elif actual_file_length < self._expected_length:
                                LOGGER.info("sink file is too short: %s (%d bytes < %d expected)",
                                            actual_filename, actual_file_length, self._expected_length)
                            else:
                                LOGGER.info("sink file is too long: %s (%d bytes > %d expected)",
                                            actual_filename, actual_file_length, self._expected_length)
                        else:
                            LOGGER.info("sink file may be incomplete: %s (%d bytes)",
                                        actual_filename, actual_file_length)
                elif self._temp_name_during_writes:
                    # Everything went fine: we rename temp file to its final name
                    if os.path.exists(self._filename):
                        os.unlink(self._filename)  # required under windows
                    os.rename(self._get_actual_filename(), self._filename)
        return close

    def _abort(self):
        self._aborted = True

    def __str__(self):
        return 'FileByteSink "%s" mode=%s' % (self._filename, self._mode)


class MemoryByteSink(ByteSink):

    def __init__(self):
        self._stream = None
        self._data = None

    def open_stream(self):
        self._stream = io.BytesIO()
        self._stream.close = self.get_stream_close(self._stream.close)
        return self._stream

    def get_bytes(self):
        return self._data

    def set_expected_length(self, length):
        pass

    def get_stream_close(self, orig_close):
        def close():
            # Save data before closing stream:
            self._data = self._stream.getvalue()
            self._stream = None
            orig_close()
        return close


class ProgressByteSink(ByteSink):
    """A byte sink that notifies a ProgressListener ; data is written to underlying ByteSink

    For pcsapi internal use only; use CDownloadRequest.progress_listener(pl) instead."""
    def __init__(self, byte_sink, progress_listener):
        self._byte_sink = byte_sink
        self._progress_listener = progress_listener

    def open_stream(self):
        stream = self._byte_sink.open_stream()
        stream = ProgressOutputStream(stream, self._progress_listener)
        return stream

    def set_expected_length(self, length):
        self._progress_listener.set_progress_total(length)
        self._byte_sink.set_expected_length(length)


class ByteSinkStream(object):
    """A stream returned by ByteSink.open_stream()"""

    def abort(self):
        """Abort operations with this stream (writes will stop before expected end).
        Implementations may use this method to delete temporary file or indicate some warning.

        abort() must be called before close()"""
        pass


class ProgressOutputStream(ByteSinkStream):
    """A stream that counts data written to an underlying stream.

    seek() or truncate() should not be called on such streams."""

    def __init__(self, stream, progress_listener):
        """
        :param byte_sink: the object that creates this stream
        :param stream: the underlying stream to watch
        :param progress_listener: will be called during stream writings"""
        self.stream = stream
        self.progress_listener = progress_listener
        self.current_bytes = 0
        # directly forwarded methods:
        self.close = stream.close
        self.flush = stream.flush
        self.isatty = stream.isatty
        self.seekable = stream.seekable
        self.seek = stream.seek
        self.tell = stream.tell
        self.fileno = stream.fileno
        self.readable = stream.readable
        self.writable = stream.writable
        self.truncate = stream.truncate


    def get_current_bytes(self):
        return self.current_bytes

    def _add_bytes(self, n):
        self.current_bytes += n
        self.progress_listener.progress(self.current_bytes)

    def __getattr__(self, name):
        if name == "closed":
            return self.stream.closed
        raise AttributeError("%r object has no attribute %r" % (self.__class__, name))

    def write(self, b):
        n = self.stream.write(b)
        if n is not None:
            self._add_bytes(n)
        return n

    def read(self, n=-1):
        not_supported(self.read)

    def readall(self):
        not_supported(self.readall)

    def readinto(self, b):
        not_supported(self.readinto)

    def readline(self, limit=-1):
        not_supported(self.readline)

    def readlines(self, hint=-1):
        not_supported(self.readlines)

    def writelines(self, lines):
        not_supported(self.writelines)

    def abort(self):
        self.progress_listener.aborted()
        try:
            # forward if supported:
            self.stream.abort()
        except AttributeError:
            pass

