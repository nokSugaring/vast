#ifndef VAST_VALUE_H
#define VAST_VALUE_H

#include <map>
#include "vast/address.h"
#include "vast/offset.h"
#include "vast/port.h"
#include "vast/prefix.h"
#include "vast/regex.h"
#include "vast/string.h"
#include "vast/time.h"
#include "vast/type.h"
#include "vast/value_type.h"
#include "vast/util/operators.h"

namespace vast {

/// The invalid or optional value.
struct value_invalid { };
constexpr value_invalid invalid = {};

template <typename Iterator>
trial<void> print(value_invalid, Iterator&& out)
{
  return print("<invalid>", out);
}

template <typename Iterator>
trial<void> parse(Iterator&& begin, Iterator&&)
{
  return {error{"cannot parse invalid value"}, begin};
}

namespace detail {
template <typename, typename>
struct visit_impl;
} // namespace detail

/// A discriminated union container for numerous types with value semantics.
/// A value has one of three states:
///
///     1. invalid
///     2. empty but typed
///     3. engaged with a value.
///
/// An *invalid* value is untyped and has not been set. An empty or *nil* value
/// has a type, but has not yet been set. This state exists to model
/// `optional<T>` semantics where `T` is a value type. A nil value is thus
/// equivalent to a disengaged optional value. Finally, an *engaged* type
/// contains a valid instance of some value type `T`.
class value
{
  template <typename, typename>
  friend struct detail::visit_impl;

public:
  /// Visits a value (single dispatch).
  /// @param v The value to visit.
  /// @param f The visitor to apply to *v*.
  template <typename F>
  typename F::result_type
  static visit(value const& v, F f);

  template <typename F>
  typename F::result_type
  static visit(value&, F);

  /// Visits a value (double dispatch).
  /// @param v1 The first value.
  /// @param v2 The second value.
  /// @param f The visitor to apply to *v1* and *v2*.
  template <typename F>
  typename F::result_type
  static visit(value const& v1, value const& v2, F f);

  template <typename F>
  typename F::result_type
  static visit(value&, value const&, F);

  template <typename F>
  typename F::result_type
  static visit(value const&, value&, F);

  template <typename F>
  typename F::result_type
  static visit(value&, value&, F);

  /// Default-constructs an invalid value.
  value(value_invalid = vast::invalid);

  /// Constructs a disengaged value of a given type.
  /// @param t The type of the value.
  explicit value(value_type t);

  value(bool b);
  value(int i);
  value(unsigned int i);
  value(long l);
  value(unsigned long l);
  value(long long ll);
  value(unsigned long long ll);
  value(double d);
  value(time_range r);
  value(time_point t);
  value(char c);
  value(char const* s);
  value(char const* s, size_t n);
  value(std::string const& s);
  value(string s);
  value(regex r);
  value(address a);
  value(prefix p);
  value(port p);
  value(record r);
  value(vector v);
  value(set s);
  value(table t);

  template <typename Rep, typename Period>
  value(std::chrono::duration<Rep, Period> duration)
  {
    new (&data_.time_range_) time_range{duration};
    data_.type(time_range_value);
    data_.engage();
  }

  template <typename Clock, typename Duration>
  value(std::chrono::time_point<Clock, Duration> time)
  {
    new (&data_.time_point_) time_point{time};
    data_.type(time_point_value);
    data_.engage();
  }

  ~value() = default;
  value(value const& other) = default;
  value(value&& other) = default;
  value& operator=(value const&) = default;
  value& operator=(value&&) = default;

  /// Checks whether the value is engaged.
  /// @returns `true` iff the value is engaged.
  /// @note An invalid value is always disengaged.
  explicit operator bool() const;

  /// Checks whether the value is *nil*.
  /// @returns `true` if the value has a type but has not yet been set.
  /// @note An invalid value is not *nil*.
  bool nil() const;

  /// Checks whether the value is the invalid value.
  /// @returns `true` if `*this == invalid`.
  /// @note An invalid value is not *nil*.
  bool invalid() const;

  /// Returns the type information of the value.
  /// @returns The type of the value.
  value_type which() const;

  /// Accesses the currently stored data in a type safe manner. The caller
  /// shall ensure that that value is engaged beforehand.
  ///
  /// @throws `std::bad_cast` if the contained data is not of type `T` or
  /// if the value is not engaged.
  template <typename T>
  T& get();

  template <typename T>
  T const& get() const;

  /// Disengages the value while keeping the type information. After calling
  /// this function, the value has relinquished its internal resources.
  void clear();

private:
  union data
  {
    data();
    data(data const& other);
    data(data&& other);
    ~data();
    data& operator=(data other);

    void construct(value_type t);

    value_type type() const;
    void type(value_type t);
    void engage();
    bool engaged() const;
    uint8_t tag() const;

    void serialize(serializer& sink) const;
    void deserialize(deserializer& source);

    bool bool_;
    int64_t int_;
    uint64_t uint_;
    double double_;
    time_range time_range_;
    time_point time_point_;
    string string_;
    std::unique_ptr<regex> regex_;
    address address_;
    prefix prefix_;
    port port_;
    std::unique_ptr<record> record_;
    std::unique_ptr<vector> vector_;
    std::unique_ptr<set> set_;
    std::unique_ptr<table> table_;
  };

  data data_;

private:
  friend access;

  void serialize(serializer& sink) const;
  void deserialize(deserializer& source);

  template <typename Iterator>
  struct value_printer
  {
    using result_type = trial<void>;

    value_printer(Iterator& out)
      : out_{out}
    {
    }

    trial<void> operator()(value_type vt) const
    {
      *out_++ = '<';

      auto t = print(vt, out_);
      if (! t)
        return t.error();

      *out_++ = '>';

      return nothing;
    }

    template <typename T>
    trial<void> operator()(T const& x) const
    {
      return print(x, out_);
    }

    trial<void> operator()(string const& str) const
    {
      *out_++ = '"';

      auto t = print(str, out_);
      if (! t)
        return t.error();

      *out_++ = '"';

      return nothing;
    }

    Iterator& out_;
  };

  template <typename Iterator>
  friend trial<void> print(value const& v, Iterator&& out)
  {
    return value::visit(v, value_printer<Iterator>(out));
  }
};

// Relational operators
bool operator==(value const& x, value const& y);
bool operator<(value const& x, value const& y);
bool operator!=(value const& x, value const& y);
bool operator>(value const& x, value const& y);
bool operator<=(value const& x, value const& y);
bool operator>=(value const& x, value const& y);

// Arithmetic operators
value operator+(value const& x, value const& y);
value operator-(value const& x, value const& y);
value operator*(value const& x, value const& y);
value operator/(value const& x, value const& y);
value operator%(value const& x, value const& y);
value operator-(value const& x);

// Bitwise operators
value operator&(value const& x, value const& y);
value operator|(value const& x, value const& y);
value operator^(value const& x, value const& y);
value operator<<(value const& x, value const& y);
value operator>>(value const& x, value const& y);
value operator~(value const& x);

namespace detail {

template <typename F, typename T>
struct value_bind_impl
{
  typedef typename F::result_type result_type;

  value_bind_impl(F f, T& x)
    : x(x), f(f)
  {
  }

  template <typename U>
  typename F::result_type operator()(U& y) const
  {
    return f(x, y);
  }

  template <typename U>
  typename F::result_type operator()(U const& y) const
  {
    return f(x, y);
  }

  T& x;
  F f;
};

template <typename F, typename T>
value_bind_impl<F, T const> value_bind(F f, T const& x)
{
  return value_bind_impl<F, T const>(f, x);
}

template <typename F, typename T>
value_bind_impl<F, T> value_bind(F f, T& x)
{
  return value_bind_impl<F, T>(f, x);
}

template <typename V1, typename V2 = V1>
struct visit_impl
{
  // Single dispatch.
  template <typename F>
  typename F::result_type
  static apply(V1& x, F f)
  {
    if (x.nil())
      return f(x.which());

    switch (x.which())
    {
      default:
        throw std::runtime_error("corrupt value type");
        break;
      case invalid_value:
        return f(invalid);
      case bool_value:
        return f(x.data_.bool_);
      case int_value:
        return f(x.data_.int_);
      case uint_value:
        return f(x.data_.uint_);
      case double_value:
        return f(x.data_.double_);
      case time_range_value:
        return f(x.data_.time_range_);
      case time_point_value:
        return f(x.data_.time_point_);
      case string_value:
        return f(x.data_.string_);
      case regex_value:
        return f(*x.data_.regex_);
      case address_value:
        return f(x.data_.address_);
      case prefix_value:
        return f(x.data_.prefix_);
      case port_value:
        return f(x.data_.port_);
      case record_value:
        return f(*x.data_.record_);
      case vector_value:
        return f(*x.data_.vector_);
      case set_value:
        return f(*x.data_.set_);
      case table_value:
        return f(*x.data_.table_);
    }
  }

  // Double dispatch.
  template <typename F>
  typename F::result_type
  static apply(V1& x, V2& y, F f)
  {
    if (x.nil())
      return visit_impl::apply(y, value_bind(f, x.which()));

    switch (x.which())
    {
      default:
        throw std::runtime_error("corrupt value type");
        break;
      case invalid_value:
        return visit_impl::apply(y, value_bind(f, invalid));
      case bool_value:
        return visit_impl::apply(y, value_bind(f, x.data_.bool_));
      case int_value:
        return visit_impl::apply(y, value_bind(f, x.data_.int_));
      case uint_value:
        return visit_impl::apply(y, value_bind(f, x.data_.uint_));
      case double_value:
        return visit_impl::apply(y, value_bind(f, x.data_.double_));
      case time_range_value:
        return visit_impl::apply(y, value_bind(f, x.data_.time_range_));
      case time_point_value:
        return visit_impl::apply(y, value_bind(f, x.data_.time_point_));
      case string_value:
        return visit_impl::apply(y, value_bind(f, x.data_.string_));
      case regex_value:
        return visit_impl::apply(y, value_bind(f, *x.data_.regex_));
      case address_value:
        return visit_impl::apply(y, value_bind(f, x.data_.address_));
      case prefix_value:
        return visit_impl::apply(y, value_bind(f, x.data_.prefix_));
      case port_value:
        return visit_impl::apply(y, value_bind(f, x.data_.port_));
      case record_value:
        return visit_impl::apply(y, value_bind(f, *x.data_.record_));
      case vector_value:
        return visit_impl::apply(y, value_bind(f, *x.data_.vector_));
      case set_value:
        return visit_impl::apply(y, value_bind(f, *x.data_.set_));
      case table_value:
        return visit_impl::apply(y, value_bind(f, *x.data_.table_));
    }
  }
};

} // namespace detail

template <typename F>
typename F::result_type
inline value::visit(value const& x, F f)
{
  return detail::visit_impl<value const>::apply(x, f);
}

template <typename F>
typename F::result_type
inline value::visit(value& x, F f)
{
  return detail::visit_impl<value>::apply(x, f);
}

template <typename F>
typename F::result_type
inline value::visit(value const& x, value const& y, F f)
{
  return detail::visit_impl<value const, value const>::apply(x, y, f);
}

template <typename F>
typename F::result_type
inline value::visit(value const& x, value& y, F f)
{
  return detail::visit_impl<value const, value>::apply(x, y, f);
}

template <typename F>
typename F::result_type
inline value::visit(value& x, value const& y, F f)
{
  return detail::visit_impl<value, value const>::apply(x, y, f);
}

template <typename F>
typename F::result_type
inline value::visit(value& x, value& y, F f)
{
  return detail::visit_impl<value, value>::apply(x, y, f);
}

namespace detail {

template <typename T>
struct getter
{
  typedef T* result_type;

  result_type operator()(T& val) const
  {
    return &val;
  }

  template <typename U>
  result_type operator()(U const&) const
  {
    throw std::bad_cast();
  }
};

} // namespace detail

template <typename T>
inline T& value::get()
{
  return *value::visit(*this, detail::getter<T>());
}

template <typename T>
inline T const& value::get() const
{
  return *value::visit(*this, detail::getter<T const>());
}

/// A vector of values with arbitrary value types.
class record : public std::vector<value>,
               util::totally_ordered<record>
{
  using super = std::vector<value>;

public:
  record() = default;

  template <
    typename... Args,
    typename = DisableIfSameOrDerived<record, Args...>
  >
  record(Args&&... args)
    : super(std::forward<Args>(args)...)
  {
  }

  record(std::initializer_list<value> list)
    : super(std::move(list))
  {
  }

  /// Recursively accesses a vector via a list of offsets serving as indices.
  ///
  /// @param o The list of offset.
  ///
  /// @returns A pointer to the value given by *o* or `nullptr` if
  /// *o* does not resolve.
  value const* at(offset const& o) const;

  /// Recursively access a value at a given index.
  ///
  /// @param i The recursive index.
  ///
  /// @returns A pointer to the value at position *i* as if the record was
  /// flattened or `nullptr` if *i* i exceeds the flat size of the record.
  value const* flat_at(size_t i) const;

  /// Computes the size of the flat record in *O(n)* time with *n* being the
  /// number of leaf elements in the record..
  ///
  /// @returns The size of the flattened record.
  size_t flat_size() const;

  void each(std::function<void(value const&)> f, bool recurse = true) const;
  bool any(std::function<bool(value const&)> f, bool recurse = true) const;
  bool all(std::function<bool(value const&)> f, bool recurse = true) const;

  void each_offset(std::function<void(value const&, offset const&)> f) const;

private:
  value const* do_flat_at(size_t i, size_t& base) const;

  void do_each_offset(std::function<void(value const&, offset const&)> f,
                      offset& o) const;

private:
  friend access;

  void serialize(serializer& sink) const;
  void deserialize(deserializer& source);

  template <typename Iterator>
  friend trial<void> print(record const& r, Iterator&& out,
                           char const* delim = ", ")
  {
    *out++ = '(';

    auto t = util::print_delimited(delim, r.begin(), r.end(), out);
    if (! t)
      return t.error();

    *out++ = ')';

    return nothing;
  }

  template <typename Iterator>
  friend trial<void> parse(record& r, Iterator& begin, Iterator end,
                           type_const_ptr const& elem_type,
                           string const& sep = ", ",
                           string const& left = "(",
                           string const& right = ")",
                           string const& esc = "\\")
  {
    if (begin == end)
      return error{"empty iterator range"};

    auto str = parse<string>(begin, end);
    if (! str)
      return str.error();

    for (auto p : str->trim(left, right).split(sep, esc))
    {
      auto t = parse<value>(p.first, p.second, elem_type);
      if (t)
        r.push_back(std::move(*t));
      else
        return t.error();
    }

    return nothing;
  }

  friend bool operator==(record const& x, record const& y);
  friend bool operator<(record const& x, record const& y);
};

// For now, we distinguish vectors and sets only logically.

class vector : public record
{
public:
  using record::record;

  vector() = default;

  vector(record r)
    : record(std::move(r))
  {
  }

  template <typename Iterator>
  friend trial<void> parse(vector& v, Iterator& begin, Iterator end,
                           type_const_ptr const& elem_type,
                           string const& sep = ", ",
                           string const& left = "[",
                           string const& right = "]",
                           string const& esc = "\\")
  {
    return parse(static_cast<record&>(v), begin, end,
                 elem_type, sep, left, right, esc);
  }
};


class set : public record
{
public:
  using record::record;

  set() = default;

  set(record r)
    : record(std::move(r))
  {
  }

  template <typename Iterator>
  friend trial<void> parse(set& v, Iterator& begin, Iterator end,
                           type_const_ptr const& elem_type,
                           string const& sep = ", ",
                           string const& left = "{",
                           string const& right = "}",
                           string const& esc = "\\")
  {
    return parse(static_cast<record&>(v), begin, end,
                 elem_type, sep, left, right, esc);
  }
};


/// An associative array.
class table : public std::map<value, value>,
              util::totally_ordered<table>
{
  using super = std::map<value, value>;

public:
  using super::super;

  void each(std::function<void(value const&, value const&)> f) const;
  bool any(std::function<bool(value const&, value const&)> f) const;
  bool all(std::function<bool(value const&, value const&)> f) const;

private:
  friend access;

  void serialize(serializer& sink) const;
  void deserialize(deserializer& source);

  template <typename Iterator>
  friend trial<void> print(table const& t, Iterator&& out)
  {
    *out++ = '{';
    auto first = t.begin();
    auto last = t.end();
    while (first != last)
    {
      auto t = print(first->first, out);
      if (! t)
        return t.error();

      t = print(" -> ", out);
      if (! t)
        return t.error();

      t = print(first->second, out);
      if (! t)
        return t.error();

      if (++first != last)
      {
        t = print(", ", out);
        if (! t)
          return t.error();
      }
    }

    *out++ = '}';

    return nothing;
  }

  friend bool operator==(table const& x, table const& y);
  friend bool operator<(table const& x, table const& y);
};


namespace detail {

template <typename Iterator>
class value_parser
{
public:
  using result_type = trial<void>;

  value_parser(value& v, Iterator& begin, Iterator end,
               string const& set_sep,
               string const& set_left,
               string const& set_right,
               string const& vec_sep,
               string const& vec_left,
               string const& vec_right,
               string const& esc)
    : v_{v},
      begin_{begin},
      end_{end},
      set_sep_{set_sep},
      set_left_{set_left},
      set_right_{set_right},
      vec_sep_{vec_sep},
      vec_left_{vec_left},
      vec_right_{vec_right},
      esc_{esc}
  {
  }

  template <
    typename T,
    typename = EnableIf<is_basic_type<T>>
  >
  result_type operator()(T const&) const
  {
    return value_parse<type_type<T>>(begin_, end_);
  }

  result_type operator()(vector_type const& t) const
  {
    return value_parse<vector>(begin_, end_, t.elem_type,
                               vec_sep_, vec_left_, vec_right_, esc_);
  }

  result_type operator()(set_type const& t) const
  {
    return value_parse<set>(begin_, end_, t.elem_type,
                            set_sep_, set_left_, set_right_, esc_);
  }

  result_type operator()(invalid_type const&) const
  {
    return error{"cannot parse an invalid type"};
  }

  result_type operator()(enum_type const&) const
  {
    return error{"cannot parse an enum type"};
  }

  result_type operator()(table_type const&) const
  {
    return error{"cannot parse tables (yet)"};
  }

  result_type operator()(record_type const&) const
  {
    return error{"cannot parse records (yet)"};
  }

  template <typename T, typename... Args>
  result_type value_parse(Args&&... args) const
  {
    T x;
    auto t = parse(x, std::forward<Args>(args)...);
    if (! t)
      return t.error();

    v_ = value{std::move(x)};
    return nothing;
  }

private:
  value& v_;
  Iterator& begin_;
  Iterator end_;
  string const& set_sep_;
  string const& set_left_;
  string const& set_right_;
  string const& vec_sep_;
  string const& vec_left_;
  string const& vec_right_;
  string const& esc_;
};

} // namespace detail
} // namespace vast

// These require a complete definition of the class value.
#include "vast/detail/parser/value.h"
#include "vast/detail/parser/skipper.h"

namespace vast {
namespace detail {

template <typename Iterator>
trial<void> parse(value& v, Iterator& begin, Iterator end)
{
  detail::parser::value<Iterator> grammar;
  detail::parser::skipper<Iterator> skipper;

  if (phrase_parse(begin, end, grammar, skipper, v) && begin == end)
    return nothing;
  else
    return error{"failed to parse value"};
}

} // namespace detail

template <typename Iterator>
trial<void> parse(value& v, Iterator& begin, Iterator end,
                      type_const_ptr type = {},
                      string const& set_sep = ", ",
                      string const& set_left = "{",
                      string const& set_right = "}",
                      string const& vec_sep = ", ",
                      string const& vec_left = "[",
                      string const& vec_right = "]",
                      string const& esc = "\\")
{
  if (! type)
    return detail::parse(v, begin, end);

  detail::value_parser<Iterator> p{v, begin, end,
                                   set_sep, set_left, set_right,
                                   vec_sep, vec_left, vec_right, esc};

  return apply_visitor(p, type->info());
}

} // namespace vast

#endif
