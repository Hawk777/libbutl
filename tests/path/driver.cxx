// file      : tests/path/driver.cxx -*- C++ -*-
// copyright : Copyright (c) 2014-2016 Code Synthesis Ltd
// license   : MIT; see accompanying LICENSE file

#include <cassert>
#include <iostream>
#include <type_traits>

#include <butl/path>
#include <butl/path-io>

using namespace std;
using namespace butl;

int
main ()
{
  // Make sure we have nothrow destructor and move constructor so that
  // storage in containers is not pessimized.
  //
  static_assert (is_nothrow_destructible<path>::value, "");
  static_assert (is_nothrow_destructible<dir_path>::value, "");

  // MINGW GCC 4.9 std::string is not nothrow-move-constructible, so path and
  // dir_path (which have a member of std::string type) are not as such as
  // well.
  //
#if !defined(_WIN32) || !defined(__GNUC__) || __GNUC__ > 4
  static_assert (is_nothrow_move_constructible<path>::value, "");
  static_assert (is_nothrow_move_constructible<dir_path>::value, "");
#endif

  auto test = [] (const char* p, const char* s, const char* r)
  {
    path x (p);
    return x.string () == s && x.representation () == r;
  };

  auto dir_test = [] (const char* p, const char* s, const char* r)
  {
    dir_path x (p);
    return x.string () == s && x.representation () == r;
  };

  assert (test ("/", "/", "/"));
  assert (test ("//", "/", "/"));
  assert (test ("/tmp/foo", "/tmp/foo", "/tmp/foo"));
  assert (test ("/tmp/foo/", "/tmp/foo", "/tmp/foo/"));
  assert (test ("/tmp/foo//", "/tmp/foo", "/tmp/foo/"));

#ifdef _WIN32
  assert (test ("/\\", "/", "/"));
  assert (test ("C:", "C:", "C:"));
  assert (test ("C:\\", "C:", "C:\\"));
  assert (test ("c:/", "c:", "c:/"));
  assert (test ("C:\\tmp\\foo\\", "C:\\tmp\\foo", "C:\\tmp\\foo\\"));
  assert (test ("C:\\tmp\\foo\\/\\", "C:\\tmp\\foo", "C:\\tmp\\foo\\"));
#endif

  assert (dir_test ("/", "/", "/"));
  assert (dir_test ("/tmp/foo/", "/tmp/foo", "/tmp/foo/"));
#ifndef _WIN32
  assert (dir_test ("tmp/foo", "tmp/foo", "tmp/foo/"));
#else
  assert (dir_test ("tmp\\foo", "tmp\\foo", "tmp\\foo\\"));

  assert (dir_test ("C:\\", "C:", "C:\\"));
  assert (dir_test ("C:\\tmp/foo\\", "C:\\tmp/foo", "C:\\tmp/foo\\"));
  assert (dir_test ("c:/tmp\\foo", "c:/tmp\\foo", "c:/tmp\\foo\\"));
#endif

  // absolute/relative/root
  //
#ifndef _WIN32
  assert (path ("/").root ());
  assert (path ("//").root ());
  assert (!path ("/foo").root ());
  assert (path ("/").absolute ());
  assert (path ("/foo/bar").absolute ());
  assert (path ("bar/baz").relative ());

  assert (path ("/").root_directory ().representation () == "/");
  assert (path ("/bar/baz").root_directory ().representation () == "/");
#else
  assert (path ("C:").root ());
  assert (path ("C:\\").root ());
  assert (!path ("C:\\foo").root ());
  assert (path ("C:").absolute ());
  assert (path ("C:\\").absolute ());
  assert (path ("C:\\foo\\bar").absolute ());
  assert (path ("bar\\baz").relative ());

  assert (path ("C:").root_directory ().representation () == "C:\\");
  assert (path ("c:/").root_directory ().representation () == "c:/");
  assert (path ("C:\\bar\\baz").root_directory ().representation () == "C:\\");
#endif

  // leaf
  //
  assert (path ().leaf ().empty ());
#ifndef _WIN32
  assert (path ("/").leaf ().representation () == "/");
  assert (path ("/tmp").leaf ().representation () == "tmp");
  assert (path ("/tmp/").leaf ().representation () == "tmp/");
  assert (path ("//tmp").leaf ().representation () == "tmp");
#else
  assert (path ("C:\\").leaf ().representation () == "C:\\");
  assert (path ("C:\\tmp").leaf ().representation () == "tmp");
  assert (path ("C:\\tmp\\").leaf ().representation () == "tmp\\");
  assert (path ("C:\\tmp/").leaf ().representation () == "tmp/");
  assert (path ("C:\\\\tmp").leaf ().representation () == "tmp");
#endif

  // directory
  //
  assert (path ().directory ().empty ());
#ifndef _WIN32
  assert (path ("/").directory ().representation () == "");
  assert (path ("/tmp").directory ().representation () == "/");
  assert (path ("/tmp/").directory ().representation () == "/");
  assert (path ("//tmp").directory ().representation () == "//");
  assert (path ("/tmp/foo").directory ().representation () == "/tmp/");
  assert (path ("/tmp/foo/").directory ().representation () == "/tmp/");
#else
  assert (path ("C:").directory ().representation () == "");
  assert (path ("C:\\tmp").directory ().representation () == "C:\\");
  assert (path ("C:\\\\tmp").directory ().representation () == "C:\\\\");
  assert (path ("C:\\tmp\\foo").directory ().representation () == "C:\\tmp\\");
  assert (path ("C:\\tmp/foo\\").directory ().representation () == "C:\\tmp/");
#endif

  // base
  //
  assert (path ("/").base ().representation () == "/");
  assert (path ("/foo.txt").base ().representation () == "/foo");
  assert (path ("/foo.txt/").base ().representation () == "/foo/");
  assert (path (".txt").base ().representation () == ".txt");
  assert (path ("/.txt").base ().representation () == "/.txt");
  assert (path ("foo.txt.orig").base ().representation () == "foo.txt");
#ifdef _WIN32
  assert (path ("C:").base ().representation () == "C:");
  assert (path ("C:\\foo.txt").base ().representation () == "C:\\foo");
  assert (path ("C:\\foo.txt\\").base ().representation () == "C:\\foo\\");
#endif

  // iteration
  //
  {
    path p;
    assert (p.begin () == p.end ());
  }
  {
    path p;
    assert (p.rbegin () == p.rend ());
  }
  {
    path p ("foo");
    path::iterator i (p.begin ());
    assert (i != p.end () && *i == "foo");
    assert (++i == p.end ());
  }
  {
    path p ("foo");
    path::reverse_iterator i (p.rbegin ());
    assert (i != p.rend () && *i == "foo");
    assert (++i == p.rend ());
  }
  {
    path p ("foo/bar");
    path::iterator i (p.begin ());
    assert (i != p.end () && *i == "foo" && i.separator () == '/');
    assert (++i != p.end () && *i == "bar" && i.separator () == '\0');
    assert (++i == p.end ());
  }
  {
    path p ("foo/bar/");
    path::iterator i (p.begin ());
    assert (i != p.end () && *i == "foo" && i.separator () == '/');
    assert (++i != p.end () && *i == "bar" && i.separator () == '/');
    assert (++i == p.end ());
  }
  {
    path p ("foo/bar");
    path::reverse_iterator i (p.rbegin ());
    assert (i != p.rend () && *i == "bar");
    assert (++i != p.rend () && *i == "foo");
    assert (++i == p.rend ());
  }
  {
    path p ("/foo/bar");
    path::iterator i (p.begin ());
    assert (i != p.end () && *i == "");
    assert (++i != p.end () && *i == "foo");
    assert (++i != p.end () && *i == "bar");
    assert (++i == p.end ());
  }
  {
    path p ("/foo/bar");
    path::reverse_iterator i (p.rbegin ());
    assert (i != p.rend () && *i == "bar");
    assert (++i != p.rend () && *i == "foo");
    assert (++i != p.rend () && *i == "");
    assert (++i == p.rend ());
  }
#ifndef _WIN32
  {
    path p ("/");
    path::iterator i (p.begin ());
    assert (i != p.end () && *i == "" && i.separator () == '/');
    assert (++i == p.end ());
  }
  {
    path p ("/");
    path::reverse_iterator i (p.rbegin ());
    assert (i != p.rend () && *i == "");
    assert (++i == p.rend ());
  }
#endif

  // iterator range construction
  //
  {
    auto test = [] (const path::iterator& b, const path::iterator& e)
    {
      return path (b, e).representation ();
    };

    {
      path p;
      assert (test (p.begin (), p.end ()) == "");
    }
    {
      path p ("foo");
      assert (test (p.begin (), p.end ()) == "foo");
      assert (test (++p.begin (), p.end ()) == "");
    }
    {
      path p ("foo/");
      assert (test (p.begin (), p.end ()) == "foo/");
    }
    {
      path p ("foo/bar");
      assert (test (p.begin (), p.end ()) == "foo/bar");
      assert (test (++p.begin (), p.end ()) == "bar");
      assert (test (p.begin (), ++p.begin ()) == "foo/");
    }
    {
      path p ("/foo/bar");
      assert (test (p.begin (), p.end ()) == "/foo/bar");
      assert (test (++p.begin (), p.end ()) == "foo/bar");
      assert (test (++(++p.begin ()), p.end ()) == "bar");

#ifndef _WIN32
      assert (test (p.begin (), ++p.begin ()) == "/");
#endif

      assert (test (++p.begin (), ++(++p.begin ())) == "foo/");
      assert (test (++(++p.begin ()), ++(++(++p.begin ()))) == "bar");
    }
    {
      path p ("/foo/bar/");
      assert (test (p.begin (), p.end ()) == "/foo/bar/");
      assert (test (++p.begin (), p.end ()) == "foo/bar/");
      assert (test (++(++p.begin ()), p.end ()) == "bar/");

#ifndef _WIN32
      assert (test (p.begin (), ++p.begin ()) == "/");
#endif

      assert (test (++p.begin (), ++(++p.begin ())) == "foo/");
      assert (test (++(++p.begin ()), ++(++(++p.begin ()))) == "bar/");
    }
#ifndef _WIN32
    {
      path p ("/");
      assert (test (p.begin (), p.end ()) == "/");
      assert (test (++p.begin (), p.end ()) == "");
    }
#endif
  }

  // operator/
  //
#ifndef _WIN32
  assert ((path ("/") / path ("tmp")).representation () == "/tmp");
  assert ((path ("foo/") / path ("bar")).representation () == "foo/bar");
  assert ((path ("foo/") / path ("bar/")).representation () == "foo/bar/");
  assert ((path ("foo/") / path ()).representation () == "foo/");
#else
  assert ((path ("\\") / path ("tmp")).representation () == "\\tmp");
  assert ((path ("C:\\") / path ("tmp")).representation () == "C:\\tmp");
  assert ((path ("foo\\") / path ("bar")).representation () == "foo\\bar");
  assert ((path ("foo\\") / path ("bar\\")).representation () == "foo\\bar\\");
  assert ((path ("foo\\") / path ("bar/")).representation () == "foo\\bar/");
  assert ((path ("foo/") / path ("bar")).representation () == "foo/bar");
  assert ((path ("foo\\") / path ()).representation () == "foo\\");
#endif

  // normalize
  //
#ifndef _WIN32
  assert (path ("../foo").normalize ().representation () == "../foo");
  assert (path ("..///foo").normalize ().representation () == "../foo");
  assert (path ("../../foo").normalize ().representation () == "../../foo");
  assert (path (".././foo").normalize ().representation () == "../foo");
  assert (path (".").normalize ().representation () == "");
  assert (path ("././").normalize ().representation () == "");
  assert (path ("./..").normalize ().representation () == "../");
  assert (path ("./../").normalize ().representation () == "../");
  assert (path ("../.").normalize ().representation () == "../");
  assert (path (".././").normalize ().representation () == "../");
  assert (path ("foo/./..").normalize ().representation () == "");
  assert (path ("/foo/./..").normalize ().representation () == "/");
  assert (path ("/foo/./../").normalize ().representation () == "/");
  assert (path ("./foo").normalize ().representation () == "foo");
  assert (path ("./foo/").normalize ().representation () == "foo/");
#else
  assert (path ("../foo").normalize ().representation () == "..\\foo");
  assert (path ("..///foo").normalize ().representation () == "..\\foo");
  assert (path ("..\\../foo").normalize ().representation () == "..\\..\\foo");
  assert (path (".././foo").normalize ().representation () == "..\\foo");
  assert (path (".").normalize ().representation () == "");
  assert (path (".\\.\\").normalize ().representation () == "");
  assert (path ("./..").normalize ().representation () == "..\\");
  assert (path ("../.").normalize ().representation () == "..\\");
  assert (path ("foo/./..").normalize ().representation () == "");
  assert (path ("C:/foo/./..").normalize ().representation () == "C:\\");
  assert (path ("C:/foo/./../").normalize ().representation () == "C:\\");
  assert (path ("./foo").normalize ().representation () == "foo");
  assert (path ("./foo\\").normalize ().representation () == "foo\\");

  assert (path ("C:\\").normalize ().representation () == "C:\\");
  assert (path ("C:\\Foo12//Bar").normalize ().representation () == "C:\\Foo12\\Bar");
#endif

  // comparison
  //
  assert (path ("/") == path ("/"));
  assert (path ("./foo") == path ("./foo"));
  assert (path ("./foo/") == path ("./foo"));
  assert (path ("./boo") < path ("./foo"));
#ifdef _WIN32
  assert (path (".\\foo") == path ("./FoO"));
  assert (path (".\\foo") == path ("./foo\\"));
  assert (path (".\\boo") < path (".\\Foo"));
#endif

  // posix_string
  //
  assert (path ("foo/bar/../baz").posix_string () == "foo/bar/../baz");
#ifdef _WIN32
  assert (path ("foo\\bar\\..\\baz").posix_string () == "foo/bar/../baz");
  try
  {
    path ("c:\\foo\\bar\\..\\baz").posix_string ();
    assert (false);
  }
  catch (const invalid_path&) {}
#endif

  // sub
  //
  {
    auto test = [] (const char* p, const char* pfx)
    {
      return path (p).sub (path (pfx));
    };

    assert (test ("foo", "foo"));
    assert (test ("foo/bar", "foo/bar"));
    assert (test ("foo/bar", "foo"));
    assert (test ("foo/bar", "foo/"));
    assert (!test ("foo/bar", "bar"));
    assert (!test ("/foo-bar", "/foo"));
    assert (test ("/foo/bar", "/foo"));
    assert (test ("/foo/bar/baz", "/foo/bar"));
    assert (!test ("/foo/bar/baz", "/foo/baz"));
#ifdef _WIN32
    assert (test ("c:", "c:"));
    assert (test ("c:", "c:\\"));
    assert (!test ("c:", "d:"));
    assert (test ("c:\\foo", "c:"));
    assert (test ("c:\\foo", "c:\\"));
#else
    assert (test ("/", "/"));
    assert (test ("/foo/bar/baz", "/"));
#endif
  }

  // sup
  //
  {
    auto test = [] (const char* p, const char* sfx)
    {
      return path (p).sup (path (sfx));
    };

    assert (test ("foo", "foo"));
    assert (test ("foo/bar", "foo/bar"));
    assert (test ("foo/bar", "bar"));
    assert (test ("foo/bar/", "bar/"));
    assert (!test ("foo/bar", "foo"));
    assert (!test ("/foo-bar", "bar"));
    assert (test ("/foo/bar", "bar"));
    assert (test ("/foo/bar/baz", "bar/baz"));
    assert (!test ("/foo/bar/baz", "bar"));

#ifdef _WIN32
    assert (test ("c:", "c:"));
    assert (test ("c:\\", "c:"));
    assert (!test ("d:", "c:"));
    assert (test ("c:\\foo", "foo"));
    assert (test ("c:\\foo\\", "foo\\"));
#else
    assert (test ("/", "/"));
#endif
  }

  // leaf(path)
  //
  {
    auto test = [] (const char* p, const char* d)
    {
      return path (p).leaf (path (d)).representation ();
    };

    assert (test ("/foo", "/") == "foo");
    //assert (test ("foo/bar", "foo") == "bar");
    assert (test ("foo/bar", "foo/") == "bar");
    assert (test ("foo/bar/", "foo/") == "bar/");
    assert (test ("/foo/bar", "/foo/") == "bar");
  }

  // directory(path)
  //
  {
    auto test = [] (const char* p, const char* l)
    {
      return path (p).directory (path (l)).representation ();
    };

    assert (test ("/foo", "foo") == "/");
    assert (test ("foo/bar", "bar") == "foo/");
    assert (test ("foo/bar/", "bar/") == "foo/");
    assert (test ("foo/bar/", "bar") == "foo/");
    assert (test ("foo/bar/baz", "bar/baz") == "foo/");
    assert (test ("/foo/bar/baz", "bar/baz") == "/foo/");
  }

  // relative
  //
  assert (path ("foo/").relative (path ("foo/")) == path ());
  assert (path ("foo/bar/").relative (path ("foo/bar/")) == path ());
  assert (path ("foo/bar/baz").relative (path ("foo/bar/")) == path ("baz"));
  assert (path ("foo/bar/baz").relative (path ("foo/bar/buz")).
    posix_string () == "../baz");
  assert (path ("foo/bar/baz").relative (path ("foo/biz/baz/")).
    posix_string () == "../../bar/baz");
  assert (path ("foo/bar/baz").relative (path ("fox/bar/baz")).
    posix_string () == "../../../foo/bar/baz");
#ifdef _WIN32
  assert (path ("c:\\foo\\bar").relative (path ("c:\\fox\\bar")) ==
          path ("..\\..\\foo\\bar"));
  try
  {
    path ("c:\\foo\\bar").relative (path ("d:\\fox\\bar"));
    assert (false);
  }
  catch (const invalid_path&) {}
#else
  assert (path ("/foo/bar/baz").relative (path ("/")) ==
          path ("foo/bar/baz"));
#endif

  assert (path::temp_directory ().absolute ());
  //assert (wpath::temp_directory ().absolute ());

  assert (path::home ().absolute ());
  //assert (wpath::home ().absolute ());

  /*
  path p ("../foo");
  p.complete ();

  cerr << path::current () << endl;
  cerr << p << endl;
  p.normalize ();
  cerr << p << endl;
  */
}
