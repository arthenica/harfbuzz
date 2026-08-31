/*
 * Copyright © 2025  Google, Inc.
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 *
 * Google Author(s): Garret Rieger
 */

#ifndef HB_RESULT_HH
#define HB_RESULT_HH

#include "hb.hh"
#include "hb-meta.hh"

#include <new>
#include <utility>

// Generic error code useful for describing common error scenarios that
// occur in harfbuzz code.
enum error_code_t {
  ALLOCATION_FAILURE,
  INVARIANT_VIOLATED,
  OFFSET_OVERFLOW,
  LIMIT_EXCEEDED,
};

static inline const char* to_string (error_code_t err)
{
  switch (err)
  {
    case ALLOCATION_FAILURE: return "error_code_t::ALLOCATION_FAILURE";
    case INVARIANT_VIOLATED: return "error_code_t::INVARIANT_VIOLATED";
    case OFFSET_OVERFLOW: return "error_code_t::OFFSET_OVERFLOW";
    case LIMIT_EXCEEDED: return "error_code_t::LIMIT_EXCEEDED";
    default: return "error_code_t::UNKNOWN";
  }
}

// Helper structs for Ok(...) and Err(...), not used directly
// by users of hb_result_t.
template <typename T>
struct hb_result_ok_t
{
  T value;
};

template <>
struct hb_result_ok_t<void> {};

template <typename E>
struct hb_result_err_t
{
  E error;

  constexpr hb_result_err_t (E e) : error (e) {}

  constexpr operator E () const { return error; }
  constexpr bool operator == (E other) const { return error == other; }
  constexpr bool operator != (E other) const { return error != other; }
  constexpr bool operator == (const hb_result_err_t& other) const { return error == other.error; }
  constexpr bool operator != (const hb_result_err_t& other) const { return error != other.error; }
};

template <typename T>
static inline constexpr hb_result_ok_t<hb_decay<T>> Ok (T&& v)
{
  return hb_result_ok_t<hb_decay<T>>{std::forward<T> (v)};
}

static inline constexpr hb_result_ok_t<void> Ok ()
{
  return hb_result_ok_t<void>{};
}

template <typename E = error_code_t>
static inline constexpr hb_result_err_t<hb_decay<E>> Err (E&& e)
{
  return hb_result_err_t<hb_decay<E>>{std::forward<E> (e)};
}

template <typename T, typename E = error_code_t>
struct hb_result_t
{
  private:
  union
  {
    T val_;
    E err_;
  };
  bool is_ok_;

  void destroy ()
  {
    if (is_ok_)
      val_.~T ();
    else
      err_.~E ();
  }

  public:
  ~hb_result_t ()
  {
    destroy ();
  }

  hb_result_t () = delete;

  hb_result_t (const hb_result_t& o) : is_ok_ (o.is_ok_)
  {
    if (is_ok_)
      new (std::addressof (val_)) T (o.val_);
    else
      new (std::addressof (err_)) E (o.err_);
  }

  hb_result_t (hb_result_t&& o) noexcept (std::is_nothrow_move_constructible<T>::value &&
                                     std::is_nothrow_move_constructible<E>::value)
      : is_ok_ (o.is_ok_)
  {
    if (is_ok_)
      new (std::addressof (val_)) T (std::move (o.val_));
    else
      new (std::addressof (err_)) E (std::move (o.err_));
  }

  hb_result_t& operator = (const hb_result_t& o)
  {
    if (this == &o) return *this;
    if (is_ok_ && o.is_ok_)
    {
      val_ = o.val_;
    }
    else if (!is_ok_ && !o.is_ok_)
    {
      err_ = o.err_;
    }
    else
    {
      destroy ();
      is_ok_ = o.is_ok_;
      if (is_ok_)
        new (std::addressof (val_)) T (o.val_);
      else
        new (std::addressof (err_)) E (o.err_);
    }
    return *this;
  }

  hb_result_t& operator = (hb_result_t&& o) noexcept (std::is_nothrow_move_assignable<T>::value &&
                                                std::is_nothrow_move_assignable<E>::value)
  {
    if (this == &o) return *this;
    if (is_ok_ && o.is_ok_)
    {
      val_ = std::move (o.val_);
    }
    else if (!is_ok_ && !o.is_ok_)
    {
      err_ = std::move (o.err_);
    }
    else
    {
      destroy ();
      is_ok_ = o.is_ok_;
      if (is_ok_)
        new (std::addressof (val_)) T (std::move (o.val_));
      else
        new (std::addressof (err_)) E (std::move (o.err_));
    }
    return *this;
  }

  // Construct from Ok(...) tag
  template <typename U = T,
            hb_enable_if ((std::is_constructible<T, U>::value))>
  hb_result_t (hb_result_ok_t<U>&& o) : is_ok_ (true)
  {
    new (std::addressof (val_)) T (std::move (o.value));
  }

  template <typename U = T,
            hb_enable_if ((std::is_constructible<T, const U&>::value))>
  hb_result_t (const hb_result_ok_t<U>& o) : is_ok_ (true)
  {
    new (std::addressof (val_)) T (o.value);
  }

  // Construct from Err(...) tag
  template <typename F = E,
            hb_enable_if ((std::is_constructible<E, F>::value))>
  hb_result_t (hb_result_err_t<F> e) : is_ok_ (false)
  {
    new (std::addressof (err_)) E (std::move (e.error));
  }

  // Construct directly from error E (when T != E)
  template <hb_enable_if ((!hb_is_same (T, E)))>
  hb_result_t (E e) : is_ok_ (false)
  {
    new (std::addressof (err_)) E (e);
  }

  // Construct directly from value T (when T != E)
  template <typename U = T,
            hb_enable_if ((std::is_constructible<T, U>::value &&
                           !hb_is_same (hb_decay<U>, E)))>
  hb_result_t (U&& v) : is_ok_ (true)
  {
    new (std::addressof (val_)) T (std::forward<U> (v));
  }

  bool is_ok () const { return is_ok_; }
  bool is_err () const { return !is_ok_; }
  explicit operator bool () const { return is_ok (); }

  T& value () & { assert (is_ok_); return val_; }
  const T& value () const & { assert (is_ok_); return val_; }
  T value () && { assert (is_ok_); return std::move (val_); }

  T& operator * () & { return value (); }
  const T& operator * () const & { return value (); }
  T operator * () && { return std::move (*this).value (); }

  T* operator -> () { return std::addressof (value ()); }
  const T* operator -> () const { return std::addressof (value ()); }

  template <typename U>
  T value_or (U&& default_value) const
  {
    if (is_ok_) return val_;
    return std::forward<U> (default_value);
  }

  const E& error () const & { assert (!is_ok_); return err_; }
  E& error () & { assert (!is_ok_); return err_; }
  E error () && { assert (!is_ok_); return std::move (err_); }

  bool operator == (const hb_result_t& o) const
  {
    if (is_ok_ != o.is_ok_) return false;
    if (is_ok_) return val_ == o.val_;
    return err_ == o.err_;
  }

  bool operator != (const hb_result_t& o) const
  {
    return !(*this == o);
  }

  template <typename U>
  bool operator == (const hb_result_ok_t<U>& o) const
  {
    return is_ok_ && val_ == o.value;
  }

  template <typename U>
  bool operator != (const hb_result_ok_t<U>& o) const
  {
    return !(*this == o);
  }

  template <typename F>
  bool operator == (const hb_result_err_t<F>& e) const
  {
    return !is_ok_ && err_ == e.error;
  }

  template <typename F>
  bool operator != (const hb_result_err_t<F>& e) const
  {
    return !(*this == e);
  }

  template <hb_enable_if ((!hb_is_same (T, E)))>
  bool operator == (E e) const
  {
    return !is_ok_ && err_ == e;
  }

  template <hb_enable_if ((!hb_is_same (T, E)))>
  bool operator != (E e) const
  {
    return is_ok_ || err_ != e;
  }
};

// Partial specialization for void
template <typename E>
struct hb_result_t<void, E>
{
  private:
  union
  {
    E err_;
  };
  bool is_ok_;

  template <typename, typename> friend struct hb_result_t;

  public:
  ~hb_result_t ()
  {
    if (!is_ok_)
      err_.~E ();
  }

  hb_result_t () : is_ok_ (true) {}
  hb_result_t (hb_result_ok_t<void>) : is_ok_ (true) {}

  hb_result_t (const hb_result_t& o) : is_ok_ (o.is_ok_)
  {
    if (!is_ok_)
      new (std::addressof (err_)) E (o.err_);
  }

  hb_result_t (hb_result_t&& o) noexcept (std::is_nothrow_move_constructible<E>::value)
      : is_ok_ (o.is_ok_)
  {
    if (!is_ok_)
      new (std::addressof (err_)) E (std::move (o.err_));
  }

  hb_result_t& operator = (const hb_result_t& o)
  {
    if (this == &o) return *this;
    if (!is_ok_ && !o.is_ok_)
    {
      err_ = o.err_;
    }
    else if (!is_ok_ && o.is_ok_)
    {
      err_.~E ();
      is_ok_ = true;
    }
    else if (is_ok_ && !o.is_ok_)
    {
      new (std::addressof (err_)) E (o.err_);
      is_ok_ = false;
    }
    return *this;
  }

  hb_result_t& operator = (hb_result_t&& o) noexcept (std::is_nothrow_move_assignable<E>::value)
  {
    if (this == &o) return *this;
    if (!is_ok_ && !o.is_ok_)
    {
      err_ = std::move (o.err_);
    }
    else if (!is_ok_ && o.is_ok_)
    {
      err_.~E ();
      is_ok_ = true;
    }
    else if (is_ok_ && !o.is_ok_)
    {
      new (std::addressof (err_)) E (std::move (o.err_));
      is_ok_ = false;
    }
    return *this;
  }

  template <typename F = E,
            hb_enable_if ((std::is_constructible<E, F>::value))>
  hb_result_t (hb_result_err_t<F> e) : is_ok_ (false)
  {
    new (std::addressof (err_)) E (std::move (e.error));
  }

  // Construct directly from error E
  hb_result_t (E e) : is_ok_ (false)
  {
    new (std::addressof (err_)) E (e);
  }

  bool is_ok () const { return is_ok_; }
  bool is_err () const { return !is_ok_; }
  explicit operator bool () const { return is_ok (); }

  const E& error () const & { assert (!is_ok_); return err_; }
  E& error () & { assert (!is_ok_); return err_; }
  E error () && { assert (!is_ok_); return std::move (err_); }

  bool operator == (const hb_result_t<void>& o) const
  {
    return is_ok_ == o.is_ok_ && (is_ok_ || err_ == o.err_);
  }

  bool operator != (const hb_result_t<void>& o) const
  {
    return !(*this == o);
  }

  template <hb_enable_if ((!hb_is_same (E, hb_result_err_t<E>)))>
  bool operator == (E e) const
  {
    return !is_ok_ && err_ == e;
  }

  template <hb_enable_if ((!hb_is_same (E, hb_result_err_t<E>)))>
  bool operator != (E e) const
  {
    return is_ok_ || err_ != e;
  }
};

template <typename U, typename T, typename E>
static inline bool operator == (const hb_result_ok_t<U>& o, const hb_result_t<T, E>& r)
{
  return r == o;
}
template <typename U, typename T, typename E>
static inline bool operator != (const hb_result_ok_t<U>& o, const hb_result_t<T, E>& r)
{
  return r != o;
}
template <typename F, typename T, typename E>
static inline bool operator == (const hb_result_err_t<F>& e, const hb_result_t<T, E>& r)
{
  return r == e;
}
template <typename F, typename T, typename E>
static inline bool operator != (const hb_result_err_t<F>& e, const hb_result_t<T, E>& r)
{
  return r != e;
}
template <typename T, typename E>
static inline bool operator == (E e, const hb_result_t<T, E>& r)
{
  return r == e;
}
template <typename T, typename E>
static inline bool operator != (E e, const hb_result_t<T, E>& r)
{
  return r != e;
}

#ifndef TRY
#define TRY(...)                                          \
  ({                                                      \
    auto res = (__VA_ARGS__);                             \
    if (!res.is_ok()) return Err(std::move(res).error()); \
    std::move(*res);                                      \
  })
#endif

#endif /* HB_RESULT_HH */
