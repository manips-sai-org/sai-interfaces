// Glaze Library
// For the license information refer to glaze.hpp

#pragma once

#include "glaze/core/opts.hpp"
#include "glaze/core/read.hpp"
#include "glaze/core/write.hpp"
#include "glaze/json/json_ptr.hpp"
#include "glaze/util/for_each.hpp"

namespace glz
{
   // Given a JSON pointer path, reads from the buffer into the object
   template <opts Opts, class T, class B>
   [[nodiscard]] parse_error read_as(T&& root_value, const sv json_ptr, B&& buffer)
   {
      parse_error pe{};
      bool b =
         detail::seek_impl([&](auto&& val) { pe = read<Opts>(val, buffer); }, std::forward<T>(root_value), json_ptr);
      if (b) {
         return pe;
      }
      pe.ec = error_code::seek_failure;
      return pe;
   }

   // Given a JSON pointer path, writes into a buffer the specified value
   template <opts Opts, class T, class B>
   [[nodiscard]] bool write_as(T&& root_value, const sv json_ptr, B&& buffer)
   {
      return detail::seek_impl(
         [&](auto&& val) {
            if constexpr (raw_buffer<B>) {
               std::ignore = write<Opts>(
                  val, buffer); // We assume the user has sufficient null characters at the end of the buffer
            }
            else {
               write<Opts>(val, buffer);
            }
         },
         std::forward<T>(root_value), json_ptr);
   }
}
