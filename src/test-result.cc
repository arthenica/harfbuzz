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

#include "hb-result.hh"


// Track constructor and destructor counts to verify leak-freedom
static int live_instances = 0;

enum error_code_t {
  ERR_A,
  ERR_B,
  ERR_C,
};

template <typename T>
using result = hb_result_t<T, error_code_t>;

struct resource_t
{
  int id;

  resource_t (int id_) : id (id_) { live_instances++; }
  resource_t (const resource_t& o) : id (o.id) { live_instances++; }
  resource_t (resource_t&& o) : id (o.id) { o.id = -1; live_instances++; }
  ~resource_t () { live_instances--; }

  resource_t& operator = (const resource_t& o)
  {
    id = o.id;
    return *this;
  }
  resource_t& operator = (resource_t&& o)
  {
    id = o.id;
    o.id = -1;
    return *this;
  }

  bool operator == (const resource_t& o) const { return id == o.id; }
};

static result<int> returns_ok (int v)
{
  return v;
}

static result<int> returns_err (error_code_t e)
{
  return e;
}

static result<int> try_callee (bool fail, int v, error_code_t e = ERR_A)
{
  if (fail) return e;
  return v;
}

static result<int> try_caller (bool fail1, bool fail2)
{
  int index = TRY (try_callee (fail1, 10, ERR_A));
  int index2 = TRY (try_callee (fail2, 20, ERR_B));
  return index + index2;
}

static result<void> try_caller_void (bool fail)
{
  TRY (try_callee (fail, 5, ERR_C));
  return Ok ();
}

static result<float> try_caller_type_change (bool fail)
{
  int index = TRY (try_callee (fail, 100, ERR_A));
  return (float) index * 1.5f;
}

static void test_ok_basic ()
{
  result<int> r = returns_ok (42);
  hb_always_assert (r.is_ok ());
  hb_always_assert (!r.is_err ());
  hb_always_assert ((bool) r);
  hb_always_assert (r.value () == 42);
  hb_always_assert (*r == 42);
  hb_always_assert (r.value_or (0) == 42);
  hb_always_assert (r == Ok (42));
  hb_always_assert (Ok (42) == r);
  hb_always_assert (r == 42);
}

static void test_err_basic ()
{
  result<int> r = returns_err (ERR_A);
  hb_always_assert (!r.is_ok ());
  hb_always_assert (r.is_err ());
  hb_always_assert (!r);
  hb_always_assert (r.error () == ERR_A);
  hb_always_assert (r.value_or (99) == 99);
  hb_always_assert (r == Err (ERR_A));
  hb_always_assert (Err (ERR_A) == r);
  hb_always_assert (r == ERR_A);
  hb_always_assert (ERR_A == r);
  hb_always_assert (r != ERR_B);
}

static void test_explicit_ok_err ()
{
  result<unsigned> r1 = Ok (100u);
  hb_always_assert (r1.is_ok ());
  hb_always_assert (r1.value () == 100u);

  result<unsigned> r2 = Err (ERR_A);
  hb_always_assert (r2.is_err ());
  hb_always_assert (r2.error () == ERR_A);
}

static void test_pointers ()
{
  int x = 123;
  result<int*> r = &x;
  hb_always_assert (r.is_ok ());
  hb_always_assert (*r == &x);
  hb_always_assert (**r == 123);
  **r = 456;
  hb_always_assert (x == 456);
}

static void test_void ()
{
  result<void> r1 = Ok ();
  hb_always_assert (r1.is_ok ());
  hb_always_assert (!r1.is_err ());
  hb_always_assert ((bool) r1);
  hb_always_assert (r1 == Ok ());
  hb_always_assert (r1 != Err (ERR_A));
  hb_always_assert (Ok() == r1);
  hb_always_assert (Err (ERR_A) != r1);

  result<void> r2 = Err (ERR_A);
  hb_always_assert (!r2.is_ok ());
  hb_always_assert (r2.is_err ());
  hb_always_assert (!r2);
  hb_always_assert (r2.error () == ERR_A);
  hb_always_assert (r2 == ERR_A);
  hb_always_assert (r2 == Err (ERR_A));
  hb_always_assert (r2 != Err (ERR_B));
  hb_always_assert (r2 != Ok ());
  hb_always_assert (Err (ERR_A) == r2);
  hb_always_assert (Err (ERR_B) != r2);
  hb_always_assert (Ok () != r2);
}

static void test_resource_cleanup ()
{
  hb_always_assert (live_instances == 0);

  {
    result<resource_t> r = resource_t (1);
    hb_always_assert (live_instances == 1);
    hb_always_assert (r.value ().id == 1);
  }
  hb_always_assert (live_instances == 0);

  {
    result<resource_t> r1 = resource_t (2);
    hb_always_assert (live_instances == 1);
    result<resource_t> r2 = r1;
    hb_always_assert (live_instances == 2);
    result<resource_t> r3 = std::move (r1);
    hb_always_assert (live_instances == 3);
  }
  hb_always_assert (live_instances == 0);

  {
    result<resource_t> r = resource_t (3);
    hb_always_assert (live_instances == 1);
    r = Err (ERR_A);
    hb_always_assert (live_instances == 0);
    hb_always_assert (r.is_err ());
  }
  hb_always_assert (live_instances == 0);
}

static void test_try_macro ()
{
  // Test success path
  result<int> r_success = try_caller (false, false);
  hb_always_assert (r_success.is_ok ());
  hb_always_assert (r_success.value () == 30);

  // Test first failure propagates
  result<int> r_fail1 = try_caller (true, false);
  hb_always_assert (r_fail1.is_err ());
  hb_always_assert (r_fail1.error () == ERR_A);

  // Test second failure propagates
  result<int> r_fail2 = try_caller (false, true);
  hb_always_assert (r_fail2.is_err ());
  hb_always_assert (r_fail2.error () == ERR_B);

  // Test TRY in void function
  result<void> v_ok = try_caller_void (false);
  hb_always_assert (v_ok.is_ok ());

  result<void> v_err = try_caller_void (true);
  hb_always_assert (v_err.is_err ());
  hb_always_assert (v_err.error () == ERR_C);

  // Test TRY with different return type
  result<float> f_ok = try_caller_type_change (false);
  hb_always_assert (f_ok.is_ok ());
  hb_always_assert (f_ok.value () == 150.0f);

  result<float> f_err = try_caller_type_change (true);
  hb_always_assert (f_err.is_err ());
  hb_always_assert (f_err.error () == ERR_A);
}

static void test_same_or_convertible_types ()
{
  // Types convertible into each other cannot be directly constructed without Ok/Err
  static_assert (!std::is_constructible<hb_result_t<int64_t, int32_t>, int>::value, "");
  static_assert (!std::is_constructible<hb_result_t<int64_t, int32_t>, int64_t>::value, "");
  static_assert (!std::is_constructible<hb_result_t<int64_t, int32_t>, int32_t>::value, "");
  static_assert (std::is_constructible<hb_result_t<int64_t, int32_t>, hb_result_ok_t<int>>::value, "");
  static_assert (std::is_constructible<hb_result_t<int64_t, int32_t>, hb_result_err_t<int>>::value, "");

  // Same types cannot be directly constructed without Ok/Err
  static_assert (!std::is_constructible<hb_result_t<int, int>, int>::value, "");
  static_assert (std::is_constructible<hb_result_t<int, int>, hb_result_ok_t<int>>::value, "");
  static_assert (std::is_constructible<hb_result_t<int, int>, hb_result_err_t<int>>::value, "");

  // Types not convertible into each other can be directly constructed
  static_assert (std::is_constructible<hb_result_t<int, error_code_t>, int>::value, "");
  static_assert (std::is_constructible<hb_result_t<int, error_code_t>, error_code_t>::value, "");

  hb_result_t<int64_t, int32_t> r_ok = Ok (1);
  hb_always_assert (r_ok.is_ok ());
  hb_always_assert (r_ok.value () == 1);

  hb_result_t<int64_t, int32_t> r_err = Err (2);
  hb_always_assert (r_err.is_err ());
  hb_always_assert (r_err.error () == 2);

  hb_result_t<int, int> same_ok = Ok (42);
  hb_always_assert (same_ok.is_ok ());
  hb_always_assert (same_ok.value () == 42);

  hb_result_t<int, int> same_err = Err (99);
  hb_always_assert (same_err.is_err ());
  hb_always_assert (same_err.error () == 99);

  // Check that enum, int type works as egpected
  {
    hb_result_t<int, error_code_t> r = 1;
    hb_always_assert(r == Ok(1));
    hb_always_assert(r == 1);
    r = ERR_A;
    hb_always_assert(r == Err(ERR_A));
    hb_always_assert(r == ERR_A);
  }

  {
    hb_result_t<error_code_t, int> r = 1;
    hb_always_assert(r == Err(1));
    hb_always_assert(r == 1);
    r = ERR_A;
    hb_always_assert(r == Ok(ERR_A));
    hb_always_assert(r == ERR_A);
  }
}

int main ()
{
  test_ok_basic ();
  test_err_basic ();
  test_explicit_ok_err ();
  test_pointers ();
  test_void ();
  test_resource_cleanup ();
  test_try_macro ();
  test_same_or_convertible_types ();

  return 0;
}
