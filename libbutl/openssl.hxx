// file      : libbutl/openssl.hxx -*- C++ -*-
// copyright : Copyright (c) 2014-2017 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#ifndef LIBBUTL_OPENSSL_HXX
#define LIBBUTL_OPENSSL_HXX

#include <string>
#include <type_traits>

#include <libbutl/export.hxx>

#include <libbutl/process.hxx>
#include <libbutl/fdstream.hxx>
#include <libbutl/small-vector.hxx>

namespace butl
{
  // Perform a crypto operation using the openssl(1) program. Throw
  // process_error and io_error (both derive from system_error) in case of
  // errors.
  //
  // The I (in) and O (out) can be of the following types/values:
  //
  // nullfd          Signal that no input/output is expected.
  //
  // path            Read input/write output from/to a file. If the special "-"
  //                 value is used, then instead input is connected to the
  //                 openssl::out ofdstream member and output -- to the
  //                 openssl::in ifdstream member. Note that the argument type
  //                 should be path, not string (i.e., pass path("-")). Also
  //                 note that the streams are opened in the binary mode. To
  //                 change that, use fdstream_mode::text instead (see below).
  //
  // fdstream_mode   Only text and binary values are meaningful. Same as
  //                 path("-"), but also specifies the translation mode.
  //
  // other           Forwarded as is to process_start(). Normally either int or
  //                 auto_fd.
  //
  // For example:
  //
  //   openssl os (path ("key.pub.pem"), // Read key from file,
  //               path ("-"),           // Write result to openssl::in.
  //               2,
  //               "openssl", "pkey",
  //               "-pubin", "-outform", "DER");
  //
  // Typical usage:
  //
  // try
  // {
  //   openssl os (nullfd,           // No input expected.
  //               path ("-"),       // Output to openssl::in.
  //               2,                // Diagnostics to stderr.
  //               path ("openssl"), // Program path.
  //               "rand",           // Command.
  //               64);              // Command options.
  //
  //   vector<char> r (os.in.read_binary ());
  //   os.in.close ();
  //
  //   if (!os.wait ())
  //     ... // openssl returned non-zero status.
  // }
  // catch (const system_error& e)
  // {
  //   cerr << "openssl error: " << e << endl;
  // }
  //
  // Notes:
  //
  // 1. If opened, in stream is in the skip mode (see fdstream_mode).
  //
  // 2. If opened, in/out must be explicitly closed before calling wait().
  //
  // 3. Normally the order of options is not important (unless they override
  //    each other). However, openssl 1.0.1 seems to have bugs in that
  //    department (that were apparently fixed in 1.0.2). To work around these
  //    bugs pass user-supplied options first.
  //
  class LIBBUTL_EXPORT openssl: public process
  {
  public:
    ifdstream in;
    ofdstream out;

    template <typename I,
              typename O,
              typename E,
              typename... A>
    openssl (I&& in,
             O&& out,
             E&& err,
             const process_env&,
             const std::string& command,
             A&&... options);

    // Version with the command line callback (see process_run() for details).
    //
    template <typename C,
              typename I,
              typename O,
              typename E,
              typename... A>
    openssl (const C&,
             I&& in,
             O&& out,
             E&& err,
             const process_env&,
             const std::string& command,
             A&&... options);

  private:
    template <typename T>
    struct is_other
    {
      using type = typename std::remove_reference<
        typename std::remove_cv<T>::type>::type;

      static const bool value = !(std::is_same<type, nullfd_t>::value ||
                                  std::is_same<type, path>::value ||
                                  std::is_same<type, fdstream_mode>::value);
    };

    struct io_data
    {
      fdpipe pipe;
      small_vector<const char*, 2> options;
    };

    int
    map_in (nullfd_t, io_data&);

    int
    map_in (const path&, io_data&);

    int
    map_in (fdstream_mode, io_data&);

    template <typename I>
    typename std::enable_if<is_other<I>::value, I>::type
    map_in (I&&, io_data&);

    int
    map_out (nullfd_t, io_data&);

    int
    map_out (const path&, io_data&);

    int
    map_out (fdstream_mode, io_data&);

    template <typename O>
    typename std::enable_if<is_other<O>::value, O>::type
    map_out (O&&, io_data&);
  };
}

#include <libbutl/openssl.ixx>
#include <libbutl/openssl.txx>

#endif // LIBBUTL_OPENSSL_HXX
