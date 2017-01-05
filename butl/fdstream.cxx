// file      : butl/fdstream.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <butl/fdstream>

#ifndef _WIN32
#  include <fcntl.h>     // open(), O_*, fcntl()
#  include <unistd.h>    // close(), read(), write(), lseek(), dup(), pipe(),
                         // ssize_t, STD*_FILENO
#  include <sys/uio.h>   // writev(), iovec
#  include <sys/stat.h>  // S_I*
#  include <sys/types.h> // off_t
#else
#  include <io.h>       // _close(), _read(), _write(), _setmode(), _sopen(),
                        // _lseek(), _dup(), _pipe()
#  include <share.h>    // _SH_DENYNO
#  include <stdio.h>    // _fileno(), stdin, stdout, stderr
#  include <fcntl.h>    // _O_*
#  include <sys/stat.h> // S_I*
#endif

#include <errno.h> // errno, E*

#include <ios>          // ios_base::openmode, ios_base::failure
#include <new>          // bad_alloc
#include <limits>       // numeric_limits
#include <cassert>
#include <cstring>      // memcpy(), memmove()
#include <exception>    // uncaught_exception()
#include <stdexcept>    // invalid_argument
#include <type_traits>
#include <system_error>

using namespace std;

namespace butl
{
  // throw_ios_failure
  //
  template <bool v>
  static inline void
  throw_ios_failure (error_code e, typename enable_if<v, const char*>::type m)
  {
    // The idea here is to make an error code to be saved into failure
    // exception and to make a string returned by what() to contain the error
    // description plus an optional custom message if provided. Unfortunatelly
    // there is no way to say that the custom message is absent. Passing an
    // empty string results for GCC (as of version 5.3.1) with a description
    // like this (note the ugly ": " prefix): ": No such file or directory".
    //
    throw ios_base::failure (m != nullptr ? m : "", e);
  }

  template <bool v>
  static inline void
  throw_ios_failure (error_code ec,
                     typename enable_if<!v, const char*>::type m)
  {
    throw ios_base::failure (m != nullptr ? m : ec.message ().c_str ());
  }

  inline void
  throw_ios_failure (int ev, const char* m = nullptr)
  {
    error_code ec (ev, system_category ());
    throw_ios_failure<is_base_of<system_error, ios_base::failure>::value> (
      ec, m);
  }

  // auto_fd
  //
  void auto_fd::
  close ()
  {
    if (fd_ >= 0)
    {
      bool r (fdclose (fd_));

      // If fdclose() failed then no reason to expect it to succeed the next
      // time.
      //
      fd_ = -1;

      if (!r)
        throw_ios_failure (errno);
    }
  }

  // fdbuf
  //
  fdbuf::
  fdbuf (auto_fd&& fd)
  {
    if (fd.get () >= 0)
      open (move (fd));
  }

  void fdbuf::
  open (auto_fd&& fd)
  {
    close ();

#ifndef _WIN32
    int flags (fcntl (fd.get (), F_GETFL));

    if (flags == -1)
      throw_ios_failure (errno);

    non_blocking_ = (flags & O_NONBLOCK) == O_NONBLOCK;
#endif

    setg (buf_, buf_, buf_);
    setp (buf_, buf_ + sizeof (buf_) - 1); // Keep space for overflow's char.

    fd_ = move (fd);
  }

  void fdbuf::
  close ()
  {
    // Before we invented auto_fd into fdstreams we keept fdbuf opened on
    // faulty close attempt. Now fdbuf is always closed by close() function.
    // This semantics change seems to be the right one as there is no reason to
    // expect fdclose() to succeed after it has already failed once.
    //
    fd_.close ();
  }

  streamsize fdbuf::
  showmanyc ()
  {
    if (!is_open ())
      return -1;

    streamsize n (egptr () - gptr ());

    if (n > 0)
      return n;

#ifndef _WIN32
    if (non_blocking_)
    {
      ssize_t n (read (fd_.get (), buf_, sizeof (buf_)));

      if (n == -1)
      {
        if (errno == EAGAIN || errno == EINTR)
          return 0;

        throw_ios_failure (errno);
      }

      if (n == 0) // EOF.
        return -1;

      setg (buf_, buf_, buf_ + n);
      return n;
    }
#endif

    return 0;
  }

  fdbuf::int_type fdbuf::
  underflow ()
  {
    int_type r (traits_type::eof ());

    if (is_open ())
    {
      // The underflow() function interface doesn't support the non-blocking
      // semantics as it must return either the next character or EOF. In the
      // future we may implement the blocking behavior for a non-blocking file
      // descriptor.
      //
      if (non_blocking_)
        throw_ios_failure (ENOTSUP);

      if (gptr () < egptr () || load ())
        r = traits_type::to_int_type (*gptr ());
    }

    return r;
  }

  bool fdbuf::
  load ()
  {
    // Doesn't handle blocking mode and so should not be called.
    //
    assert (!non_blocking_);

#ifndef _WIN32
    ssize_t n (read (fd_.get (), buf_, sizeof (buf_)));
#else
    int n (_read (fd_.get (), buf_, sizeof (buf_)));
#endif

    if (n == -1)
      throw_ios_failure (errno);

    setg (buf_, buf_, buf_ + n);
    return n != 0;
  }

  fdbuf::int_type fdbuf::
  overflow (int_type c)
  {
    int_type r (traits_type::eof ());

    if (is_open () && c != traits_type::eof ())
    {
      // The overflow() function interface doesn't support the non-blocking
      // semantics since being unable to serialize the character is supposed
      // to be an error. In the future we may implement the blocking behavior
      // for a non-blocking file descriptor.
      //
      if (non_blocking_)
        throw_ios_failure (ENOTSUP);

      // Store last character in the space we reserved in open(). Note
      // that pbump() doesn't do any checks.
      //
      *pptr () = traits_type::to_char_type (c);
      pbump (1);

      if (save ())
        r = c;
    }

    return r;
  }

  int fdbuf::
  sync ()
  {
    if (!is_open ())
      return -1;

    // The sync() function interface doesn't support the non-blocking
    // semantics since it should either completely sync the data or fail. In
    // the future we may implement the blocking behavior for a non-blocking
    // file descriptor.
    //
    if (non_blocking_)
      throw_ios_failure (ENOTSUP);

    return save () ? 0 : -1;
  }

  bool fdbuf::
  save ()
  {
    size_t n (pptr () - pbase ());

    if (n != 0)
    {
      // Note that for MinGW GCC (5.2.0) _write() returns 0 for a file
      // descriptor opened for read-only access (while -1 with errno EBADF is
      // expected). This is in contrast with VC's _write() and POSIX's write().
      //
#ifndef _WIN32
      ssize_t m (write (fd_.get (), buf_, n));
#else
      int m (_write (fd_.get (), buf_, n));
#endif

      if (m == -1)
        throw_ios_failure (errno);

      if (n != static_cast<size_t> (m))
        return false;

      setp (buf_, buf_ + sizeof (buf_) - 1);
    }

    return true;
  }

  streamsize fdbuf::
  xsputn (const char_type* s, streamsize sn)
  {
    // The xsputn() function interface doesn't support the non-blocking
    // semantics since the only excuse not to fully serialize the data is
    // encountering EOF (the default behaviour is defined as a sequence of
    // sputc() calls which stops when either sn characters are written or a
    // call would have returned EOF). In the future we may implement the
    // blocking behavior for a non-blocking file descriptor.
    //
    if (non_blocking_)
      throw_ios_failure (ENOTSUP);

    // To avoid futher 'signed/unsigned comparison' compiler warnings.
    //
    size_t n (static_cast<size_t> (sn));

    // Buffer the data if there is enough space.
    //
    size_t an (epptr () - pptr ()); // Amount of free space in the buffer.
    if (n <= an)
    {
      memcpy (pptr (), s, n);
      pbump (n);
      return n;
    }

    size_t bn (pptr () - pbase ()); // Buffered data size.

#ifndef _WIN32

    ssize_t r;
    if (bn > 0)
    {
      // Write both buffered and new data with a single system call.
      //
      iovec iov[2] = {{pbase (), bn}, {const_cast<char*> (s), n}};
      r = writev (fd_.get (), iov, 2);
    }
    else
      r = write (fd_.get (), s, n);

    if (r == -1)
      throw_ios_failure (errno);

    size_t m (static_cast<size_t> (r));

    // If the buffered data wasn't fully written then move the unwritten part
    // to the beginning of the buffer.
    //
    if (m < bn)
    {
      memmove (pbase (), pbase () + m, bn - m);
      pbump (-m); // Note that pbump() accepts negatives.
      return 0;
    }

    setp (buf_, buf_ + sizeof (buf_) - 1);
    return m - bn;

#else

    // On Windows there is no writev() available so sometimes we will make two
    // system calls. Fill and flush the buffer, then try to fit the data tail
    // into the empty buffer. If the data tail is too long then just write it
    // to the file and keep the buffer empty.
    //
    // We will end up with two _write() calls if the total data size to be
    // written exceeds double the buffer size. In this case the buffer filling
    // is redundant so let's pretend there is no free space in the buffer, and
    // so buffered and new data will be written separatelly.
    //
    if (bn + n > 2 * (bn + an))
      an = 0;
    else
    {
      memcpy (pptr (), s, an);
      pbump (an);
    }

    // Flush the buffer.
    //
    size_t wn (bn + an);
    int r (wn > 0 ? _write (fd_.get (), buf_, wn) : 0);

    if (r == -1)
      throw_ios_failure (errno);

    size_t m (static_cast<size_t> (r));

    // If the buffered data wasn't fully written then move the unwritten part
    // to the beginning of the buffer.
    //
    if (m < wn)
    {
      memmove (pbase (), pbase () + m, wn - m);
      pbump (-m); // Note that pbump() accepts negatives.
      return m < bn ? 0 : m - bn;
    }

    setp (buf_, buf_ + sizeof (buf_) - 1);

    // Now 'an' holds the size of the data portion written as a part of the
    // buffer flush.
    //
    s += an;
    n -= an;

    // Buffer the data tail if it fits the buffer.
    //
    if (n <= static_cast<size_t> (epptr () - pbase ()))
    {
      memcpy (pbase (), s, n);
      pbump (n);
      return sn;
    }

    // The data tail doesn't fit the buffer so write it to the file.
    //
    r = _write (fd_.get (), s, n);

    if (r == -1)
      throw_ios_failure (errno);

    return an + r;
#endif
  }

  inline static bool
  flag (fdstream_mode m, fdstream_mode flag)
  {
    return (m & flag) == flag;
  }

  inline static auto_fd
  mode (auto_fd fd, fdstream_mode m)
  {
    if (fd.get () >= 0 &&
        (flag (m, fdstream_mode::text) ||
         flag (m, fdstream_mode::binary) ||
         flag (m, fdstream_mode::blocking) ||
         flag (m, fdstream_mode::non_blocking)))
      fdmode (fd.get (), m);

    return fd;
  }

  // fdstream_base
  //
  fdstream_base::
  fdstream_base (auto_fd&& fd, fdstream_mode m)
      : fdstream_base (mode (move (fd), m)) // Delegate.
  {
  }

  static fdopen_mode
  translate_mode (ios_base::openmode m)
  {
    enum
    {
      in    = ios_base::in,
      out   = ios_base::out,
      app   = ios_base::app,
      bin   = ios_base::binary,
      trunc = ios_base::trunc,
      ate   = ios_base::ate
    };

    const fdopen_mode fd_in     (fdopen_mode::in);
    const fdopen_mode fd_out    (fdopen_mode::out);
    const fdopen_mode fd_inout  (fdopen_mode::in | fdopen_mode::out);
    const fdopen_mode fd_app    (fdopen_mode::append);
    const fdopen_mode fd_trunc  (fdopen_mode::truncate);
    const fdopen_mode fd_create (fdopen_mode::create);
    const fdopen_mode fd_bin    (fdopen_mode::binary);
    const fdopen_mode fd_ate    (fdopen_mode::at_end);

    fdopen_mode r;
    switch (m & ~(ate | bin))
    {
    case in               : r = fd_in                           ; break;
    case out              :
    case out | trunc      : r = fd_out   | fd_trunc | fd_create ; break;
    case app              :
    case out | app        : r = fd_out   | fd_app   | fd_create ; break;
    case out | in         : r = fd_inout                        ; break;
    case out | in | trunc : r = fd_inout | fd_trunc | fd_create ; break;
    case out | in | app   :
    case in  | app        : r = fd_inout | fd_app   | fd_create ; break;

    default: throw invalid_argument ("invalid open mode");
    }

    if (m & ate)
      r |= fd_ate;

    if (m & bin)
      r |= fd_bin;

    return r;
  }

  // ifdstream
  //
  ifdstream::
  ifdstream (const char* f, openmode m, iostate e)
      : ifdstream (f, translate_mode (m | in), e) // Delegate.
  {
  }

  ifdstream::
  ifdstream (const char* f, fdopen_mode m, iostate e)
      : ifdstream (fdopen (f, m | fdopen_mode::in), e) // Delegate.
  {
  }

  ifdstream::
  ~ifdstream ()
  {
    if (skip_ && is_open () && good ())
    {
      // Clear the exception mask to prevent ignore() from throwing.
      //
      exceptions (goodbit);
      ignore (numeric_limits<streamsize>::max ());
    }

    // Underlying file descriptor is closed by fdbuf dtor with errors (if any)
    // being ignored.
    //
  }

  void ifdstream::
  open (const char* f, openmode m)
  {
    open (f, translate_mode (m | in));
  }

  void ifdstream::
  open (const char* f, fdopen_mode m)
  {
    open (fdopen (f, m | fdopen_mode::in));
  }

  void ifdstream::
  close ()
  {
    if (skip_ && is_open () && good ())
      ignore (numeric_limits<streamsize>::max ());

    buf_.close ();
  }

  ifdstream&
  getline (ifdstream& is, string& s, char delim)
  {
    ifdstream::iostate eb (is.exceptions ());
    assert (eb & ifdstream::badbit);

    // Amend the exception mask to prevent exceptions being thrown by the C++
    // IO runtime to avoid incompatibility issues due to ios_base::failure ABI
    // fiasco (#66145). We will not restore the mask when ios_base::failure is
    // thrown by fdbuf since there is no way to "silently" restore it if the
    // corresponding bits are in the error state without the exceptions() call
    // throwing ios_base::failure. Not restoring exception mask on throwing
    // because of badbit should probably be ok since the stream is no longer
    // usable.
    //
    if (eb != ifdstream::badbit)
      is.exceptions (ifdstream::badbit);

    std::getline (is, s, delim);

    // Throw if any of the newly set bits are present in the exception mask.
    //
    if ((is.rdstate () & eb) != ifdstream::goodbit)
      throw_ios_failure (EIO, "getline failure");

    if (eb != ifdstream::badbit)
      is.exceptions (eb); // Restore exception mask.

    return is;
  }

  // ofdstream
  //
  ofdstream::
  ofdstream (const char* f, openmode m, iostate e)
      : ofdstream (f, translate_mode (m | out), e) // Delegate.
  {
  }

  ofdstream::
  ofdstream (const char* f, fdopen_mode m, iostate e)
      : ofdstream (fdopen (f, m | fdopen_mode::out), e) // Delegate.
  {
  }

  ofdstream::
  ~ofdstream ()
  {
    // Enforce explicit close(). Note that we may have false negatives but not
    // false positives. Specifically, we will fail to enforce if someone is
    // using ofdstream in a dtor being called while unwinding the stack due to
    // an exception.
    //
    assert (!is_open () || !good () || uncaught_exception ());
  }

  void ofdstream::
  open (const char* f, openmode m)
  {
    open (f, translate_mode (m | out));
  }

  void ofdstream::
  open (const char* f, fdopen_mode m)
  {
    open (fdopen (f, m | fdopen_mode::out));
  }

  // Utility functions
  //
  auto_fd
  fdopen (const char* f, fdopen_mode m, permissions p)
  {
    mode_t pf (S_IREAD | S_IWRITE | S_IEXEC);

#ifdef S_IRWXG
    pf |= S_IRWXG;
#endif

#ifdef S_IRWXO
    pf |= S_IRWXO;
#endif

    pf &= static_cast<mode_t> (p);

    // Return true if the open mode contains a specific flag.
    //
    auto mode = [m](fdopen_mode flag) -> bool {return (m & flag) == flag;};

    int of (0);
    bool in (mode (fdopen_mode::in));
    bool out (mode (fdopen_mode::out));

#ifndef _WIN32

    if (in && out)
      of |= O_RDWR;
    else if (in)
      of |= O_RDONLY;
    else if (out)
      of |= O_WRONLY;

    if (out)
    {
      if (mode (fdopen_mode::append))
        of |= O_APPEND;

      if (mode (fdopen_mode::truncate))
        of |= O_TRUNC;
    }

    if (mode (fdopen_mode::create))
    {
      of |= O_CREAT;

      if (mode (fdopen_mode::exclusive))
        of |= O_EXCL;
    }

#ifdef O_LARGEFILE
    of |= O_LARGEFILE;
#endif

    int fd (open (f, of, pf));

#else

    if (in && out)
      of |= _O_RDWR;
    else if (in)
      of |= _O_RDONLY;
    else if (out)
      of |= _O_WRONLY;

    if (out)
    {
      if (mode (fdopen_mode::append))
        of |= _O_APPEND;

      if (mode (fdopen_mode::truncate))
        of |= _O_TRUNC;
    }

    if (mode (fdopen_mode::create))
    {
      of |= _O_CREAT;

      if (mode (fdopen_mode::exclusive))
        of |= _O_EXCL;
    }

    of |= mode (fdopen_mode::binary) ? _O_BINARY : _O_TEXT;

    // According to Microsoft _sopen() should not change the permissions of an
    // existing file. However it does if we pass them (reproduced on Windows
    // XP, 7, and 8). And we must pass them if we have _O_CREATE. So we need
    // to take care of preserving the permissions ourselves. Note that Wine's
    // implementation of _sopen() works properly.
    //
    bool pass_perm (of & _O_CREAT);

    if (pass_perm && file_exists (path (f)))
    {
      // If the _O_CREAT flag is set then we need to clear it so that we can
      // omit the permissions. But if the _O_EXCL flag is set as well we can't
      // do that as fdopen() wouldn't fail as expected.
      //
      if (of & _O_EXCL)
        throw_ios_failure (EEXIST);

      of &= ~_O_CREAT;
      pass_perm = false;
    }

    int fd (pass_perm
            ? _sopen (f, of, _SH_DENYNO, pf)
            : _sopen (f, of, _SH_DENYNO));

#endif

    if (fd == -1)
      throw_ios_failure (errno);

    if (mode (fdopen_mode::at_end))
    {
#ifndef _WIN32
      bool r (lseek (fd, 0, SEEK_END) != static_cast<off_t> (-1));
#else
      bool r (_lseek (fd, 0, SEEK_END) != -1);
#endif

      // Note that in the case of an error we don't delete the newly created
      // file as we have no indication if it is a new one.
      //
      if (!r)
      {
        int e (errno);
        fdclose (fd); // Doesn't throw, but can change errno.
        throw_ios_failure (e);
      }
    }

    return auto_fd (fd);
  }

  auto_fd
  fddup (int fd)
  {
#ifndef _WIN32
    int nfd (dup (fd));
#else
    int nfd (_dup (fd));
#endif

    if (nfd == -1)
      throw_ios_failure (errno);

    return auto_fd (nfd);
  }

#ifndef _WIN32

  bool
  fdclose (int fd) noexcept
  {
    return close (fd) == 0;
  }

  int
  fdnull () noexcept
  {
    return open ("/dev/null", O_RDWR);
  }

  fdstream_mode
  fdmode (int fd, fdstream_mode m)
  {
    int flags (fcntl (fd, F_GETFL));

    if (flags == -1)
      throw_ios_failure (errno);

    if (flag (m, fdstream_mode::blocking) ||
        flag (m, fdstream_mode::non_blocking))
    {
      m &= fdstream_mode::blocking | fdstream_mode::non_blocking;

      // Should be exactly one blocking mode flag specified.
      //
      if (m != fdstream_mode::blocking && m != fdstream_mode::non_blocking)
        throw invalid_argument ("invalid blocking mode");

      int new_flags (
        m == fdstream_mode::non_blocking
        ? flags | O_NONBLOCK
        : flags & ~O_NONBLOCK);

      if (fcntl (fd, F_SETFL, new_flags) == -1)
        throw_ios_failure (errno);
    }

    return fdstream_mode::binary |
      ((flags & O_NONBLOCK) == O_NONBLOCK
       ? fdstream_mode::non_blocking
       : fdstream_mode::blocking);
  }

  fdstream_mode
  stdin_fdmode (fdstream_mode m)
  {
    return fdmode (STDIN_FILENO, m);
  }

  fdstream_mode
  stdout_fdmode (fdstream_mode m)
  {
    return fdmode (STDOUT_FILENO, m);
  }

  fdstream_mode
  stderr_fdmode (fdstream_mode m)
  {
    return fdmode (STDERR_FILENO, m);
  }

  fdpipe
  fdopen_pipe (fdopen_mode m)
  {
    assert (m == fdopen_mode::none || m == fdopen_mode::binary);

    int pd[2];
    if (pipe (pd) == -1)
      throw_ios_failure (errno);

    return {auto_fd (pd[0]), auto_fd (pd[1])};
  }

#else

  bool
  fdclose (int fd) noexcept
  {
    return _close (fd) == 0;
  }

  int
  fdnull (bool temp) noexcept
  {
    // No need to translate \r\n before sending it to void.
    //
    if (!temp)
      return _sopen ("nul", _O_RDWR | _O_BINARY, _SH_DENYNO);

    try
    {
      // We could probably implement a Windows-specific version of getting
      // the temporary file that avoid any allocations and exceptions.
      //
      path p (path::temp_path ("null")); // Can throw.
      return _sopen (p.string ().c_str (),
                     (_O_CREAT      |
                      _O_RDWR       |
                      _O_BINARY     |  // Don't translate.
                      _O_TEMPORARY  |  // Remove on close.
                      _O_SHORT_LIVED), // Don't flush to disk.
                     _SH_DENYNO,
                     _S_IREAD | _S_IWRITE);
    }
    catch (const bad_alloc&)
    {
      errno = ENOMEM;
      return -1;
    }
    catch (const system_error& e)
    {
      errno = e.code ().value ();
      return -1;
    }
  }

  fdstream_mode
  fdmode (int fd, fdstream_mode m)
  {
    m &= fdstream_mode::text | fdstream_mode::binary;

    // Should be exactly one translation flag specified.
    //
    // It would have been natural not to change translation mode if none of
    // text or binary flags are passed. Unfortunatelly there is no (easy) way
    // to obtain the current mode for the file descriptor without setting a
    // new one. This is why not specifying one of the modes is an error.
    //
    if (m != fdstream_mode::binary && m != fdstream_mode::text)
      throw invalid_argument ("invalid translation mode");

    int r (_setmode (fd, m == fdstream_mode::binary ? _O_BINARY : _O_TEXT));
    if (r == -1)
      throw_ios_failure (errno);

    return fdstream_mode::blocking |
      ((r & _O_BINARY) == _O_BINARY
       ? fdstream_mode::binary
       : fdstream_mode::text);
  }

  fdstream_mode
  stdin_fdmode (fdstream_mode m)
  {
    int fd (_fileno (stdin));
    if (fd == -1)
      throw_ios_failure (errno);

    return fdmode (fd, m);
  }

  fdstream_mode
  stdout_fdmode (fdstream_mode m)
  {
    int fd (_fileno (stdout));
    if (fd == -1)
      throw_ios_failure (errno);

    return fdmode (fd, m);
  }

  fdstream_mode
  stderr_fdmode (fdstream_mode m)
  {
    int fd (_fileno (stderr));
    if (fd == -1)
      throw_ios_failure (errno);

    return fdmode (fd, m);
  }

  fdpipe
  fdopen_pipe (fdopen_mode m)
  {
    assert (m == fdopen_mode::none || m == fdopen_mode::binary);

    int pd[2];
    if (_pipe (
          pd,
          64 * 1024, // Set buffer size to 64K.
          _O_NOINHERIT | (m == fdopen_mode::none ? _O_TEXT : _O_BINARY)) == -1)
      throw_ios_failure (errno);

    return {auto_fd (pd[0]), auto_fd (pd[1])};
  }
#endif
}
