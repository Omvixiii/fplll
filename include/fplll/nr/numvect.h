// SPDX-FileCopyrightText: 2011 Xavier Pujol
//
// SPDX-License-Identifier: LGPL-2.1-or-later

#ifndef FPLLL_NUMVECT_H
#define FPLLL_NUMVECT_H

#include "../defs.h"

#include <algorithm>
#include <cstddef>
#include <ios>
#include <istream>
#include <ostream>
#include <vector>

namespace fplll
{

/** Extends the size of the given vector. */
template <class T> void extend_vect(std::vector<T> &v, int size)
{
  if (static_cast<int>(v.size()) < size)
  {
    v.resize(static_cast<std::size_t>(size));
  }
}

/** Reverses a vector by consecutive swaps. */
template <class T> void reverse_by_swap(std::vector<T> &v, int first, int last)
{
  for (; first < last; first++, last--)
  {
    v[static_cast<std::size_t>(first)].swap(v[static_cast<std::size_t>(last)]);
  }
}

/** Rotates a vector by consecutive swaps. */
template <class T> void rotate_by_swap(std::vector<T> &v, int first, int middle, int last)
{
  // Algorithm from the STL code
  reverse_by_swap(v, first, middle - 1);
  reverse_by_swap(v, middle, last);
  for (; first < middle && middle <= last; first++, last--)
  {
    v[static_cast<std::size_t>(first)].swap(v[static_cast<std::size_t>(last)]);
  }
  if (first == middle)
  {
    reverse_by_swap(v, middle, last);
  }
  else
  {
    reverse_by_swap(v, first, middle - 1);
  }
}

/** Rotates a vector left-wise by consecutive swaps. */
template <class T> void rotate_left_by_swap(std::vector<T> &v, int first, int last)
{
  FPLLL_DEBUG_CHECK(0 <= first && first <= last && last < static_cast<int>(v.size()));
  for (int i = first; i < last; i++)
  {
    v[static_cast<std::size_t>(i)].swap(v[static_cast<std::size_t>(i) + 1]);
  }
}

/** Rotates a vector right-wise by consecutive swaps. */
template <class T> void rotate_right_by_swap(std::vector<T> &v, int first, int last)
{
  FPLLL_DEBUG_CHECK(0 <= first && first <= last && last < static_cast<int>(v.size()));
  for (int i = last - 1; i >= first; i--)
  {
    v[static_cast<std::size_t>(i)].swap(v[static_cast<std::size_t>(i) + 1]);
  }
}

/** Prints a vector on stream os. */
template <class T> std::ostream &operator<<(std::ostream &os, const std::vector<T> &v)
{
  os << "[";
  const int n = static_cast<int>(v.size());
  for (int i = 0; i < n; i++)
  {
    if (i > 0)
    {
      os << " ";
    }
    os << v[static_cast<std::size_t>(i)];
  }
  os << "]";
  return os;
}

/** Reads a vector from stream is. */
template <class T> std::istream &operator>>(std::istream &is, std::vector<T> &v)
{
  char c;
  v.clear();
  if (!(is >> c))
  {
    return is;
  }
  if (c != '[')
  {
    is.setstate(std::ios::failbit);
    return is;
  }
  while (is >> c && c != ']')
  {
    is.putback(c);
    v.resize(v.size() + 1);
    if (!(is >> v.back()))
    {
      v.pop_back();
      return is;
    }
  }
  return is;
}

/** Generates a zero vector. */
template <class T> void gen_zero_vect(std::vector<T> &v, int n)
{
  v.resize(static_cast<std::size_t>(n));
  std::fill(v.begin(), v.end(), 0);
}

template <class T> class NumVect;

template <class T> std::ostream &operator<<(std::ostream &os, const NumVect<T> &v);

template <class T> std::istream &operator>>(std::istream &is, NumVect<T> &v);

/** NumVect is a one-dimensional container of numbers. It is a thin wrapper
    around std::vector that implements the vector operations used by the
    reduction algorithms. All indices are 0-based. */
template <class T> class NumVect
{
public:
  /** Creates an empty NumVect (0). */
  NumVect() = default;
  /** Initializes NumVect with the elements of a given vector. */
  NumVect(const std::vector<T> &v) : data(v) {}
  /** Initializes a NumVect of the given size. The initial content is
      undefined. */
  NumVect(int size) : data(static_cast<std::size_t>(size)) {}
  NumVect(int size, const T &t) : data(static_cast<std::size_t>(size), t) {}
  /** Swaps the data between the NumVect and the given NumVect. */
  void swap(NumVect &v) { data.swap(v.data); }
  /** Returns the number of elements in NumVect. */
  int size() const { return static_cast<int>(data.size()); }
  /** Checks whether NumVect is empty. */
  bool empty() const { return data.empty(); }
  /** Sets the size of NumVect. */
  void resize(int size) { data.resize(static_cast<std::size_t>(size)); }
  /** Sets the size of NumVect and all its elements to t.*/
  void resize(int size, const T &t) { data.resize(static_cast<std::size_t>(size), t); }
  /** Sets the size of NumVect and all its elements to zero. */
  void gen_zero(int size)
  {
    data.resize(static_cast<std::size_t>(size));
    fill(0);
  }

  /** Compares two NumVects by comparing the underlying vectors. */
  bool operator==(const NumVect<T> &other) const { return data == other.data; }

  /** Inserts an element in the back of NumVect. */
  void push_back(const T &t) { data.push_back(t); }
  /** Removes the back element of NumVect. */
  void pop_back() { data.pop_back(); }
  /** Returns a reference to the front element of NumVect. */
  T &front() { return data.front(); }
  /** Returns a const reference to the front element of NumVect,
      on constant object. */
  const T &front() const { return data.front(); }
  /** Returns a reference to the back element of NumVect. */
  T &back() { return data.back(); }
  /** Returns a const reference to the back element of NumVect,
      on constant object. */
  const T &back() const { return data.back(); }
  /** Extends the size of NumVect, only if it is needed. */
  void extend(int maxSize)
  {
    if (size() < maxSize)
    {
      data.resize(static_cast<std::size_t>(maxSize));
    }
  }
  void clear() { data.clear(); }
  /** Returns a reference to the i-th element of NumVect. */
  T &operator[](int i) { return data[static_cast<std::size_t>(i)]; }
  /** Returns a const reference to the i-th element of NumVect on constant
      object. */
  const T &operator[](int i) const { return data[static_cast<std::size_t>(i)]; }
  /** Addition of two NumVector objects, till index n. */
  void add(const NumVect<T> &v, int n);
  /** Addition of two NumVector objects. */
  void add(const NumVect<T> &v) { add(v, size()); }
  /** Subtraction of two NumVector objects, till index n. */
  void sub(const NumVect<T> &v, int n);
  /** Subtraction of two NumVector objects. */
  void sub(const NumVect<T> &v) { sub(v, size()); }
  /** Multiplication of NumVector and a number c, from index b till index n. */
  void mul(const NumVect<T> &v, int b, int n, T c);
  /** Multiplication of NumVector and a number c, till index n. */
  void mul(const NumVect<T> &v, int n, T c);
  /** Multiplication of NumVector and a number c. */
  void mul(const NumVect<T> &v, T c) { mul(v, size(), c); }
  /** Division of NumVector and a number c, from index b till index n. */
  // Kept by value: part of the public template interface.
  // NOLINTBEGIN(performance-unnecessary-value-param)
  void div(const NumVect<T> &v, int b, int n, T c);
  /** Division of NumVector and a number c, till index n. */
  void div(const NumVect<T> &v, int n, T c);
  /** Division of NumVector and a number c. */
  void div(const NumVect<T> &v, T c) { div(v, size(), c); }
  /** Incrementing each coefficient of NumVector by its product with
      number c, from beg to index n - 1. */
  void addmul(const NumVect<T> &v, T x, int beg, int n);
  /** Incrementing each coefficient of NumVector by its product with
      number c, till index n. */
  void addmul(const NumVect<T> &v, T x, int n);
  // NOLINTEND(performance-unnecessary-value-param)
  /** Incrementing each coefficient of NumVector by its product with
      number c. */
  void addmul(const NumVect<T> &v, T x) { addmul(v, x, size()); }
  void addmul_2exp(const NumVect<T> &v, const T &x, long expo, T &tmp)
  {
    addmul_2exp(v, x, expo, size(), tmp);
  }
  void addmul_2exp(const NumVect<T> &v, const T &x, long expo, int n, T &tmp);
  void addmul_si(const NumVect<T> &v, long x) { addmul_si(v, x, size()); }
  void addmul_si(const NumVect<T> &v, long x, int n);
  void addmul_si_2exp(const NumVect<T> &v, long x, long expo, T &tmp)
  {
    addmul_si_2exp(v, x, expo, size(), tmp);
  }
  void addmul_si_2exp(const NumVect<T> &v, long x, long expo, int n, T &tmp);

  /** (v[first],...,v[last]) becomes (v[first+1],...,v[last],v[first]) */
  void rotate_left(int first, int last) { rotate_left_by_swap(data, first, last); }

  /** (v[first],...,v[last]) becomes (v[last],v[first],...,v[last-1]) */
  void rotate_right(int first, int last) { rotate_right_by_swap(data, first, last); }

  /** Fills NumVect with the value given. */
  void fill(long value);

  /** Checks if NumVect has zero elements from fromCol. */
  bool is_zero(int fromCol = 0) const;

  /** Returns last non-zero index of NumVector. */
  int size_nz() const;

  friend std::ostream &operator<< <T>(std::ostream &os, const NumVect<T> &v);
  friend std::istream &operator>> <T>(std::istream &is, NumVect<T> &v);

private:
  std::vector<T> data;
};

template <class T> void NumVect<T>::add(const NumVect<T> &v, int n)
{
  FPLLL_DEBUG_CHECK(n <= size() && size() == v.size() && v.is_zero(n));
  for (int i = n - 1; i >= 0; i--)
  {
    data[static_cast<std::size_t>(i)].add(data[static_cast<std::size_t>(i)],
                                          v.data[static_cast<std::size_t>(i)]);
  }
}

template <class T> void NumVect<T>::sub(const NumVect<T> &v, int n)
{
  FPLLL_DEBUG_CHECK(n <= size() && size() == v.size() && v.is_zero(n));
  for (int i = n - 1; i >= 0; i--)
  {
    data[static_cast<std::size_t>(i)].sub(data[static_cast<std::size_t>(i)],
                                          v.data[static_cast<std::size_t>(i)]);
  }
}

template <class T> void NumVect<T>::mul(const NumVect<T> &v, int b, int n, T c)
{
  FPLLL_DEBUG_CHECK(n <= size() && size() == v.size() && v.is_zero(n));
  for (int i = n - 1; i >= b; i--)
  {
    data[static_cast<std::size_t>(i)].mul(v.data[static_cast<std::size_t>(i)], c);
  }
}

template <class T> void NumVect<T>::mul(const NumVect<T> &v, int n, T c) { mul(v, 0, n, c); }

template <class T> void NumVect<T>::div(const NumVect<T> &v, int b, int n, T c)
{
  FPLLL_DEBUG_CHECK(n <= size() && size() == v.size() && v.is_zero(n));
  for (int i = n - 1; i >= b; i--)
  {
    data[static_cast<std::size_t>(i)].div(v.data[static_cast<std::size_t>(i)], c);
  }
}

template <class T> void NumVect<T>::div(const NumVect<T> &v, int n, T c) { div(v, 0, n, c); }

template <class T> void NumVect<T>::addmul(const NumVect<T> &v, T x, int beg, int n)
{
  FPLLL_DEBUG_CHECK(n <= size() && size() == v.size() && v.is_zero(n));
  for (int i = n - 1; i >= beg; i--)
  {
    data[static_cast<std::size_t>(i)].addmul(v.data[static_cast<std::size_t>(i)], x);
  }
}

template <class T> void NumVect<T>::addmul(const NumVect<T> &v, T x, int n)
{
  this->addmul(v, x, 0, n);
}

template <class T>
void NumVect<T>::addmul_2exp(const NumVect<T> &v, const T &x, long expo, int n, T &tmp)
{
  FPLLL_DEBUG_CHECK(n <= size() && size() == v.size() && v.is_zero(n));
  for (int i = n - 1; i >= 0; i--)
  {
    tmp.mul(v.data[static_cast<std::size_t>(i)], x);
    tmp.mul_2si(tmp, expo);
    data[static_cast<std::size_t>(i)].add(data[static_cast<std::size_t>(i)], tmp);
  }
}

template <class T> void NumVect<T>::addmul_si(const NumVect<T> &v, long x, int n)
{
  FPLLL_DEBUG_CHECK(n <= size() && size() == v.size() && v.is_zero(n));
  for (int i = n - 1; i >= 0; i--)
  {
    data[static_cast<std::size_t>(i)].addmul_si(v.data[static_cast<std::size_t>(i)], x);
  }
}

template <class T>
void NumVect<T>::addmul_si_2exp(const NumVect<T> &v, long x, long expo, int n, T &tmp)
{
  FPLLL_DEBUG_CHECK(n <= size() && size() == v.size() && v.is_zero(n));
  for (int i = n - 1; i >= 0; i--)
  {
    tmp.mul_si(v.data[static_cast<std::size_t>(i)], x);
    tmp.mul_2si(tmp, expo);
    data[static_cast<std::size_t>(i)].add(data[static_cast<std::size_t>(i)], tmp);
  }
}

template <class T> void NumVect<T>::fill(long value)
{
  for (int i = 0; i < size(); i++)
  {
    data[static_cast<std::size_t>(i)] = value;
  }
}

template <class T> bool NumVect<T>::is_zero(int fromCol) const
{
  for (int i = fromCol; i < size(); i++)
  {
    if (!data[static_cast<std::size_t>(i)].is_zero())
    {
      return false;
    }
  }
  return true;
}

template <class T> int NumVect<T>::size_nz() const
{
  int i;
  for (i = size(); i > 0; i--)
  {
    if (data[static_cast<std::size_t>(i - 1)] != 0)
    {
      break;
    }
  }
  return i;
}

/** Computes the truncated dot product between two NumVects using coefficients [beg, n).
 * Constraint: n > beg.
 */
template <class T>
inline void dot_product(T &result, const NumVect<T> &v1, const NumVect<T> &v2, int beg, int n)
{
  FPLLL_DEBUG_CHECK(beg >= 0 && n > beg && n <= v1.size() && n <= v2.size());
  result.mul(v1[beg], v2[beg]);
  for (int i = beg + 1; i < n; i++)
  {
    result.addmul(v1[i], v2[i]);
  }
}

template <class T>
inline void dot_product(T &result, const NumVect<T> &v1, const NumVect<T> &v2, int n)
{
  FPLLL_DEBUG_CHECK(n <= v1.size() && v1.size() == v2.size() && (v1.is_zero(n) || v2.is_zero(n)));
  dot_product(result, v1, v2, 0, n);
}

/** Prints a NumVect on stream os. */
template <class T> std::ostream &operator<<(std::ostream &os, const NumVect<T> &v)
{
  return os << v.data;
}

/** Reads a NumVect from stream is. */
template <class T> std::istream &operator>>(std::istream &is, NumVect<T> &v)
{
  return is >> v.data;
}

}  // namespace fplll

#endif
