// Copyright (c) 2014-2018 Dr. Colin Hirsch and Daniel Frey
// Please see LICENSE for license or visit https://github.com/taocpp/PEGTL/

#include <cstdlib>
#include <iostream>
#include <string>

#include <tao/pegtl.hpp>

using namespace tao::TAOCPP_PEGTL_NAMESPACE;  // NOLINT

#include "double.hpp"

namespace sum
{
   struct padded_double
      : pad< double_::grammar, space >
   {
   };

   struct double_list
      : list< padded_double, one< ',' > >
   {
   };

   struct grammar
      : seq< double_list, eof >
   {
   };

   template< typename Rule >
   struct action
      : nothing< Rule >
   {
   };

   template<>
   struct action< double_::grammar >
   {
      template< typename Input >
      static void apply( const Input& in, double& sum )
      {
         // assume all values will fit into a C++ double
         sum += std::strtod( in.string().c_str(), nullptr );
      }
   };

}  // namespace sum

int main()
{
   std::cout << "Give me a comma separated list of numbers.\n";
   std::cout << "The numbers are added using the PEGTL.\n";
   std::cout << "Type [q or Q] to quit\n\n";

   std::string str;

   while( !std::getline( std::cin, str ).fail() ) {
      if( str.empty() || str[ 0 ] == 'q' || str[ 0 ] == 'Q' ) {
         break;
      }
      double d = 0.0;
      memory_input<> in( str, "std::cin" );
      if( parse< sum::grammar, sum::action >( in, d ) ) {
         std::cout << "parsing OK; sum = " << d << std::endl;
      }
      else {
         std::cout << "parsing failed" << std::endl;
      }
   }
}
