/*
Copyright (c) 2019 Paul Stahr

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifndef LOGHTML_H
#define LOGHTML_H

#include <ostream>
#include <string>
#include <vector>

namespace HTML{

template <typename Callback>
class callback_ostreambuf : public std::streambuf, public std::ostream
{
public:
    callback_ostreambuf(Callback cb_, void* user_data = nullptr): std::streambuf(), std::ostream(this), _callback(cb_), _user_data(user_data) {}

protected:
    std::streamsize xsputn(const std::streambuf::char_type* s, std::streamsize n) override 
    { 
        return _callback(s, n, _user_data); // returns the number of characters successfully written.
    };

    std::streambuf::int_type overflow(std::streambuf::int_type ch) override 
    { 
        return _callback(&ch, 1, _user_data); // returns the number of characters successfully written.
    }

private:
    Callback _callback;
    void* _user_data;
};

template <typename Callback>
callback_ostreambuf<Callback> make_callback_ostreambuf(Callback cb, void* user_data = nullptr)
{
    return callback_ostreambuf<Callback>(cb, user_data);
}

struct print_out
{
    std::streamsize operator()(const void* buf, std::streamsize sz, void* user_data);
};

struct begintable{
    begintable(size_t border_, std::initializer_list<char> const & col_sep_, std::initializer_list<char> const & row_sep_, bool col_sep_block);
    begintable(size_t border_, char col_sep_, char row_sep_, bool col_sep_block);
    size_t _border;
    std::vector<char> _col_sep;
    std::vector<char> _row_sep;
    bool _col_sep_block;
};

struct new_row{};

struct endtable{};

struct beginlink{
    std::string _filename;
    beginlink(std::string const & filename_) : _filename(filename_)
    {
    }
};

struct endlink{};

struct table_status{
    size_t _new_col_at_next_char;
    bool _new_row_at_next_char;
};


struct svg{
    std::string _filename;
    size_t _maxwidth;
    size_t _maxheight;
};


class LogHtml : public callback_ostreambuf<print_out>
{
public:
    LogHtml (std::ostream & out_);
    ~LogHtml();
    void close();
    void linksvg(std::string const & file);
    void begin_table(size_t border, size_t num_cols, char column_sep, char row_sep);
    void end_table();
    void close_tables();
    void add_input();
    friend LogHtml & operator <<(std::ostream &, svg const & );
    friend LogHtml & operator <<(std::ostream &, begintable const & );
    friend LogHtml & operator <<(std::ostream &, endtable const &);
    friend LogHtml & operator <<(std::ostream &, new_row const &);
    friend LogHtml & operator <<(std::ostream &, beginlink const & );
    friend LogHtml & operator <<(std::ostream &, endlink const &);
    friend std::streamsize print_out::operator()(const void* buf, std::streamsize sz, void* user_data);
private:
    std::vector<begintable> _table_stack;
    std::vector<table_status> _table_status_stack;
    std::ostream & _out;
    bool _closed;

};
}
#endif
