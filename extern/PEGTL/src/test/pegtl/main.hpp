// Copyright (c) 2014-2018 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#ifndef TAOCPP_PEGTL_INCLUDE_TEST_MAIN_HPP
#define TAOCPP_PEGTL_INCLUDE_TEST_MAIN_HPP

#include <cstdlib>
#include <iostream>

int main( int /*unused*/, char** argv )
{
   tao::TAOCPP_PEGTL_NAMESPACE::unit_test();

   if( tao::TAOCPP_PEGTL_NAMESPACE::failed != 0 ) {
      std::cerr << "pegtl: unit test " << argv[ 0 ] << " failed " << tao::TAOCPP_PEGTL_NAMESPACE::failed << std::endl;
   }
   return ( tao::TAOCPP_PEGTL_NAMESPACE::failed == 0 ) ? EXIT_SUCCESS : EXIT_FAILURE;
}

#endif
