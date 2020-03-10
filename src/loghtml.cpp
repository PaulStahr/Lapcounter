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

#include "loghtml.h"

#include <iostream>
#include <algorithm>
namespace HTML {

begintable::begintable(size_t border_, char col_sep_, char row_sep_, bool col_sep_block_) : _border(border_), _col_sep({col_sep_}), _row_sep({row_sep_}), _col_sep_block(col_sep_block_){}

begintable::begintable(size_t border_, const std::initializer_list< char >& col_sep_, const std::initializer_list< char >& row_sep_, bool col_sep_block_) : _border(border_), _col_sep(col_sep_), _row_sep(row_sep_), _col_sep_block(col_sep_block_){}
    
LogHtml::LogHtml (std::ostream & out_) : callback_ostreambuf<print_out>(print_out(), this), _out(out_), _closed(false)
{
    _out << "<html>" << std::endl;
}

LogHtml::~LogHtml()
{
    close();
}

void LogHtml::close()
{
    if (_closed)
    {
        return;
    }
    _closed = true;
    if (_out)
    {
        _out << "</html>" <<std::endl;
    }
}

void LogHtml::close_tables()
{
    while (!_table_stack.empty())
    {
        *this << endtable();
    }
}

std::streamsize print_out::operator()(const void* buf, std::streamsize sz, void* user_data)
{
    //std::string test = "abc def abc def";
    //test = std::regex_replace(test, std::regex("def"), "klm");
    const char *cbuf = reinterpret_cast<const char*>(buf);
    LogHtml & log = * reinterpret_cast<LogHtml*>(user_data);
    begintable *tb = log._table_stack.empty() ? nullptr : &log._table_stack.back();
    table_status *ts = log._table_status_stack.empty() ? nullptr : &log._table_status_stack.back();
    std::ostream & out =  log._out;
    for (auto iter = cbuf; iter != cbuf + sz; ++iter)
    {
        if (!log._table_stack.empty())
        {
            if (std::find(tb->_row_sep.begin(), tb->_row_sep.end(), *iter) != tb->_row_sep.end())
            {
                ts->_new_row_at_next_char = true;
                continue;
            }
            if (ts->_new_row_at_next_char)
            {
                out << "</td></tr>\n<tr><td>";
                ts->_new_row_at_next_char = false;
                ts->_new_col_at_next_char = 0;
            }
            if (std::find(tb->_col_sep.begin(), tb->_col_sep.end(), *iter) != tb->_col_sep.end())
            {
                ++ts->_new_col_at_next_char;
                continue;
            }
            if (ts->_new_col_at_next_char > 0)
            {
                if (tb->_col_sep_block)
                {
                    ts->_new_col_at_next_char = 1;
                }
                while (ts->_new_col_at_next_char != 0)
                {
                    out << "</td><td>";
                    --ts->_new_col_at_next_char;
                }
            }
        }
        if (*iter == '\n')
        {
            out << "<br />" << std::endl;
            continue;
        }
        out << *iter;
    }
    return sz;
}

void LogHtml::add_input()
{
    begintable *tb = _table_stack.empty() ? nullptr : &_table_stack.back();
    table_status *ts = _table_status_stack.empty() ? nullptr : &_table_status_stack.back();
    std::ostream & out =  _out;
    if (!_table_stack.empty())
    {
        if (ts->_new_row_at_next_char)
        {
            out << "</td></tr>\n<tr><td>";
            ts->_new_row_at_next_char = false;
            ts->_new_col_at_next_char = 0;
        }
        if (ts->_new_col_at_next_char > 0)
        {
            if (tb->_col_sep_block)
            {
                ts->_new_col_at_next_char = 1;
            }
            while (ts->_new_col_at_next_char != 0)
            {
                out << "</td><td>";
                --ts->_new_col_at_next_char;
            }
        }
    }
}

LogHtml & operator <<(std::ostream & out, svg const & image)
{
    LogHtml & loghtml = *dynamic_cast<LogHtml*>(&out);
    loghtml.add_input();
    loghtml._out << "<a href=\"" << image._filename << "\"> <img";
    if (image._maxwidth != 0 && image._maxheight != 0)
    {
        loghtml._out << " style=\"max-width:" << image._maxwidth << "px; << max-height:" << image._maxwidth << "px;\"";
    }
    loghtml._out << " src=\"" << image._filename << "\" /></a>" << std::endl;
    return loghtml;
}

LogHtml & operator <<(std::ostream & out, begintable const & table)
{
    LogHtml & loghtml = *dynamic_cast<LogHtml*>(&out);
    loghtml._table_stack.emplace_back(table);
    loghtml._table_status_stack.push_back(table_status({0, false}));
    loghtml._out << "<table border=\"" << table._border <<  "\">" << std::endl;
    loghtml._out << "<tr><td>" << std::endl;
    return loghtml;
}

LogHtml & operator <<(std::ostream & out, new_row const &)
{
    LogHtml & loghtml = *dynamic_cast<LogHtml*>(&out);
    return loghtml;
}

LogHtml & operator <<(std::ostream & out, endtable const &)
{
    LogHtml & loghtml = *dynamic_cast<LogHtml*>(&out);
    loghtml._table_stack.pop_back();
    loghtml._table_status_stack.pop_back();
    loghtml._out << "</td></tr>" << std::endl;
    loghtml._out << "</table>" << std::endl;
    return loghtml;
}

LogHtml & operator << (std::ostream & out, beginlink const & link)
{
    LogHtml & loghtml = *dynamic_cast<LogHtml*>(&out);
    loghtml.add_input();
    loghtml._out << "<a href=\"" << link._filename << "\">";
    return loghtml;
}

LogHtml & operator << (std::ostream & out, endlink const & )
{
    LogHtml & loghtml = *dynamic_cast<LogHtml*>(&out);
    loghtml._out << "</a>";
    return loghtml;
}
}
