/*
 *  fibmapsort - sorts a list of files according to their first block
 *  Copyright (C) 2005, 2007  Cesar Eduardo Barros
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <iostream>
#include <map>
#include <string>
#include <sstream>
#include <exception>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/fs.h>
#include <boost/noncopyable.hpp>

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#define noreturn __attribute__((__noreturn__))

class unix_error : public std::exception
{
	int m_error;
public:
	explicit unix_error(int error) throw() : m_error(error) { }
	~unix_error() throw() { }
	int get_error() const throw() { return this->m_error; }
	const char* what() const throw();
};

const char*
unix_error::what() const throw()
{
	return strerror(this->m_error);
}

noreturn
static void
throw_unix_error()
{
	throw unix_error(errno);
}

static void
throw_unix_error_dtor()
{
	if (likely(!std::uncaught_exception()))
		throw unix_error(errno);
}

static inline int
check_unix_error(int ret)
{
	if (unlikely(ret < 0))
		throw_unix_error();
	return ret;
}

static inline int
check_unix_error_dtor(int ret)
{
	if (unlikely(ret < 0))
		throw_unix_error_dtor();
	return ret;
}

template <typename T>
static inline T*
check_unix_error_ptr(T* ret)
{
	if (unlikely(!ret))
		throw_unix_error();
	return ret;
}

template <typename T>
static inline T*
check_unix_error_ptr_dtor(T* ret)
{
	if (unlikely(!ret))
		throw_unix_error_dtor();
	return ret;
}

class scoped_fd : boost::noncopyable
{
	int m_fd;
public:
	explicit scoped_fd(int fd) : m_fd(fd) { }
	~scoped_fd() { check_unix_error_dtor(close(this->m_fd)); }
	int fd() const { return this->m_fd; }
};

int
main(int argc, char** argv)
{
	using std::cin;
	using std::cout;
	using std::cerr;
	using std::endl;
	using std::string;
	using std::stringbuf;
	using std::ios_base;
	using std::multimap;
	using std::make_pair;

	bool show_block = false;
	char delim = '\n';
	int opt;

	while ((opt = getopt (argc, argv, "0b")) != -1)
	{
		switch (opt)
		{
		case '0':
			delim = '\0';
			break;
		case 'b':
			show_block = true;
			break;
		default:
			cerr << "Usage: " << argv[0] << " [-0] [-b]" << endl;
			return 1;
		}
	}

	typedef multimap<unsigned int, string> map_type;
	map_type map;

	for (stringbuf sb(ios_base::out);
			cin.get(sb, delim);
			cin.ignore(1), sb.str(""))
	{
		string str = sb.str();

		try
		{
			scoped_fd fd(check_unix_error(open64(str.c_str(),
						O_RDONLY | O_NOCTTY
						| O_NONBLOCK | O_NOFOLLOW
						| O_NOATIME)));

			struct stat64 stat_buf;
			check_unix_error(fstat64(fd.fd(), &stat_buf));

			if (likely(S_ISREG(stat_buf.st_mode)))
			{
				unsigned int block = 0;
				check_unix_error(ioctl(fd.fd(), FIBMAP, &block));

				map.insert(make_pair(block, str));
			}
			else
				map.insert(make_pair(0, str));
		}
		catch (const unix_error& e)
		{
			cerr << argv[0] << ": " << str << ": " << e.what() << endl;
			map.insert(make_pair(0, str));
		}
	}

	for (map_type::const_iterator i = map.begin(); i != map.end(); ++i)
	{
		if (show_block)
			cout << i->first << ' ';
		cout << i->second << delim;
	}
}
