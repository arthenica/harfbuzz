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

#include "result.hh"

using namespace graph;

// Track constructor and destructor counts to verify leak-freedom
static int live_instances = 0;

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

static Result<int> returns_ok (int v)
{
  return v;
}

static Result<int> returns_err (GraphError e)
{
  return e;
}

static Result<int> try_callee (bool fail, int v, GraphError e = ALLOCATION)
{
  if (fail) return e;
  return v;
}

static Result<int> try_caller (bool fail1, bool fail2)
{
  int index = TRY (try_callee (fail1, 10, ALLOCATION));
  int index2 = TRY (try_callee (fail2, 20, OVERFLOW));
  return index + index2;
}

static Result<void> try_caller_void (bool fail)
{
  TRY (try_callee (fail, 5, LIMIT_EXCEEDED));
  return Ok ();
}

static Result<float> try_caller_type_change (bool fail)
{
  int index = TRY (try_callee (fail, 100, INVARIANT));
  return (float) index * 1.5f;
}

static void test_ok_basic ()
{
  Result<int> r = returns_ok (42);
  hb_always_assert (r.is_ok ());
  hb_always_assert (!r.is_err ());
  hb_always_assert ((bool) r);
  hb_always_assert (r.unwrap () == 42);
  hb_always_assert (r.get_ok () == 42);
  hb_always_assert (*r == 42);
  hb_always_assert (r.unwrap_or (0) == 42);
  hb_always_assert (r == Ok (42));
  hb_always_assert (Ok (42) == r);
  hb_always_assert (r == 42);
}

static void test_err_basic ()
{
  Result<int> r = returns_err (ALLOCATION);
  hb_always_assert (!r.is_ok ());
  hb_always_assert (r.is_err ());
  hb_always_assert (!r);
  hb_always_assert (r.error () == ALLOCATION);
  hb_always_assert (r.get_error () == ALLOCATION);
  hb_always_assert (r.unwrap_or (99) == 99);
  hb_always_assert (r == Err (ALLOCATION));
  hb_always_assert (Err (ALLOCATION) == r);
  hb_always_assert (r == ALLOCATION);
  hb_always_assert (ALLOCATION == r);
  hb_always_assert (r != OVERFLOW);
}

static void test_explicit_ok_err ()
{
  Result<unsigned> r1 = Ok (100u);
  hb_always_assert (r1.is_ok ());
  hb_always_assert (r1.unwrap () == 100u);

  Result<unsigned> r2 = Err (LIMIT_EXCEEDED);
  hb_always_assert (r2.is_err ());
  hb_always_assert (r2.error () == LIMIT_EXCEEDED);

  Result<unsigned> r3 = Result<unsigned>::ok (200u);
  hb_always_assert (r3.is_ok ());
  hb_always_assert (r3.unwrap () == 200u);

  Result<unsigned> r4 = Result<unsigned>::err (INVARIANT);
  hb_always_assert (r4.is_err ());
  hb_always_assert (r4.error () == INVARIANT);
}

static void test_pointers ()
{
  int x = 123;
  Result<int*> r = &x;
  hb_always_assert (r.is_ok ());
  hb_always_assert (*r == &x);
  hb_always_assert (**r == 123);
  **r = 456;
  hb_always_assert (x == 456);
}

static void test_void ()
{
  Result<void> r1 = Ok ();
  hb_always_assert (r1.is_ok ());
  hb_always_assert (!r1.is_err ());
  hb_always_assert ((bool) r1);
  r1.unwrap ();
  hb_always_assert (r1 == Ok ());

  Result<void> r2 = Err (OVERFLOW);
  hb_always_assert (!r2.is_ok ());
  hb_always_assert (r2.is_err ());
  hb_always_assert (!r2);
  hb_always_assert (r2.error () == OVERFLOW);
  hb_always_assert (r2 == OVERFLOW);
  hb_always_assert (r2 == Err (OVERFLOW));

  Result<void> r3 = Result<void>::ok ();
  hb_always_assert (r3.is_ok ());

  Result<void> r4 = Result<void>::err (ALLOCATION);
  hb_always_assert (r4.is_err ());
  hb_always_assert (r4.error () == ALLOCATION);
}

static void test_resource_cleanup ()
{
  hb_always_assert (live_instances == 0);

  {
    Result<resource_t> r = resource_t (1);
    hb_always_assert (live_instances == 1);
    hb_always_assert (r.unwrap ().id == 1);
  }
  hb_always_assert (live_instances == 0);

  {
    Result<resource_t> r1 = resource_t (2);
    hb_always_assert (live_instances == 1);
    Result<resource_t> r2 = r1;
    hb_always_assert (live_instances == 2);
    Result<resource_t> r3 = std::move (r1);
    hb_always_assert (live_instances == 3);
  }
  hb_always_assert (live_instances == 0);

  {
    Result<resource_t> r = resource_t (3);
    hb_always_assert (live_instances == 1);
    r = Err (ALLOCATION);
    hb_always_assert (live_instances == 0);
    hb_always_assert (r.is_err ());
  }
  hb_always_assert (live_instances == 0);
}

static void test_try_macro ()
{
  // Test success path
  Result<int> r_success = try_caller (false, false);
  hb_always_assert (r_success.is_ok ());
  hb_always_assert (r_success.unwrap () == 30);

  // Test first failure propagates
  Result<int> r_fail1 = try_caller (true, false);
  hb_always_assert (r_fail1.is_err ());
  hb_always_assert (r_fail1.error () == ALLOCATION);

  // Test second failure propagates
  Result<int> r_fail2 = try_caller (false, true);
  hb_always_assert (r_fail2.is_err ());
  hb_always_assert (r_fail2.error () == OVERFLOW);

  // Test TRY in void function
  Result<void> v_ok = try_caller_void (false);
  hb_always_assert (v_ok.is_ok ());

  Result<void> v_err = try_caller_void (true);
  hb_always_assert (v_err.is_err ());
  hb_always_assert (v_err.error () == LIMIT_EXCEEDED);

  // Test TRY with different return type
  Result<float> f_ok = try_caller_type_change (false);
  hb_always_assert (f_ok.is_ok ());
  hb_always_assert (f_ok.unwrap () == 150.0f);

  Result<float> f_err = try_caller_type_change (true);
  hb_always_assert (f_err.is_err ());
  hb_always_assert (f_err.error () == INVARIANT);
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

  return 0;
}
