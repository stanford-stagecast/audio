#ifndef HELPERS_HH
#define HELPERS_HH

#include <system_error>
#include <vector>
#include <complex>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <boost/align/aligned_allocator.hpp>

class unix_error : public std::system_error {
    std::string what_;

public:
    unix_error( const std::string & context )
        : system_error( errno, std::system_category() ),
          what_( context + ": " + std::system_error::what() )
    {}

    const char *what() const noexcept override { return what_.c_str(); }
};

inline int Check( const char * context, const int return_value )
{
  if ( return_value >= 0 ) { return return_value; }
  throw unix_error( context );
}

inline int Check( const std::string & context, const int return_value )
{
    return Check( context.c_str(), return_value );
}

class FileDescriptor
{
    int fd_;

public:
    FileDescriptor( const int fd ) : fd_( fd ) {}

    ~FileDescriptor() { Check( "close", ::close( fd_ ) ); }

    int fd_num() const { return fd_; }

    uint64_t size() const
    {
        struct stat file_info;
        Check( "fstat", fstat( fd_, &file_info ) );
        return file_info.st_size;
    }

    FileDescriptor( const FileDescriptor & other ) = delete;
    FileDescriptor & operator=( const FileDescriptor & other ) = delete;
};

template <typename T>
using AlignedSignal = std::vector<T, boost::alignment::aligned_allocator<T, 64>>;

using RealSignal = AlignedSignal<float>;
using ComplexSignal = AlignedSignal<std::complex<float>>;

#endif /* HELPERS_HH */
