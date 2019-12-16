#pragma once

#include <glm/glm.hpp>
#include <GL/glew.h>
#include <GL/glu.h>

#include <algorithm>
#include <vector>
#include <string>
#include <stdarg.h>
#include <stdio.h>

#include <inttypes.h>


#include <vector>

// We use the set of flags shown here to group/categorize
// arbitrary log function calls. For any flag defined, simply add it 
// as a bitwise OR to the variable g_log_mask (initialized in base/util.cpp).
// Then invoke the CLOG() macro defined below, with the flag set as its first parameter.
 
extern unsigned long long g_log_mask;

enum {
  logflag_programs_load = 1 << 0,
  logflag_programs_use_program = 1 << 1,
  logflag_textures_bind = 1 << 2,
  logflag_render_pipeline_pass_info_apply = 1 << 3
};

#define write_logf(...) logf_impl(__LINE__, __func__, __FILE__, __VA_ARGS__) 

#define CLOG(flag, ...) do { if ((g_log_mask & (flag)) != 0) { write_logf("CLOG|" __VA_ARGS__); } } while (0)
#define CLOG_CODE(code) code

// Macro'd out incase non-c++17 compilers are used.
#define STATIC_IF(cond) if constexpr ((cond))

#define __FATAL__(...) do { write_logf(__VA_ARGS__); ASSERT(false); } while (0)

#define GLSL(src) "#version 450 core\n" #src

#define GL_FN(expr)                                     \
  do {                                                  \
  expr;                                                 \
  report_gl_error(glGetError(), __LINE__, __func__, __FILE__, #expr); \
 } while (0)

#define ASSERT(cond) assert_impl((cond), __LINE__, __func__, __FILE__, #cond)

void assert_impl(bool cond,
                 int line,
                 const char* func,
                 const char* file,
                 const char* expr);

void logf_impl(int line,
               const char* func,
               const char* file,
               const char* fmt, ...);


void report_gl_error(GLenum err,
                     int line,
                     const char* func,
                     const char* file,
                     const char* expr);

std::vector<uint8_t> read_file(const std::string& path); 

// https://stackoverflow.com/a/8498694
//
// This is a class which acts as a kind of "pseudo-reflection" for enums:
// the goal is to be able iterate over enum class values in a for loop.
// As an example, if we have the given enum class:
// 
// enum class my_enum {
//    a,
//    b,
//    c,
//    d,
//    enum_type_first = a,
//    enum_type_last = d
// };
// 
// we can then use that class in a for loop like so:
//
// for (auto enum_value: enum_type<my_enum> ) {
//     < do something with enum value >
// }
// 
// Thus, we're iterating through {a, b, c, d} values with this class.
// This is useful in a few niche situations.
//
// Note that enum_type_first and enum_type_last must be defined.

template< typename T >
class enum_type
{
public:
   class iterator
   {
   public:
      iterator( int value ) :
         m_value( value )
      { }

      T operator*( void ) const
      {
         return (T)m_value;
      }

      void operator++( void )
      {
         ++m_value;
      }

      bool operator!=( iterator rhs )
      {
         return m_value != rhs.m_value;
      }

   private:
      int m_value;
   };

};

template< typename T >
typename enum_type<T>::iterator begin( enum_type<T> )
{
   return typename enum_type<T>::iterator( (int)T::enum_type_first );
}

template< typename T >
typename enum_type<T>::iterator end( enum_type<T> )
{
   return typename enum_type<T>::iterator( ((int)T::enum_type_last) + 1 );
}
