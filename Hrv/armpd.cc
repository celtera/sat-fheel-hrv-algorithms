// ampd.cc rev. 03 Jan 2014 by Stuart Ambler.  Implements automatic
// multiscale-based peak detection (AMPD) algorithm as in An Efficient
// Algorithm for Automatic Peak Detection in Noisy Periodic and Quasi-Periodic
// Signals, by Felix Scholkmann, Jens Boss and Martin Wolf, Algorithms 2012, 5,
// 588-603.  Self-contained implementation and main function for test, except
// for dependency on argtable2.h for parsing command line arguments.
// Copyright (c) 2014 Stuart Ambler.
// Distributed under the Boost License in the accompanying file LICENSE.

// The algorithm, restated using 0-based array and matrix subscripts, and
// changed to add computation of minima as well as maxima.  In the process of
// doing this I came across something unexplained in the paper: it implicitly
// wants the number of windows evaluated for highest k to be >= 2.  It gives no
// reason why k >= 1 isn't good enough; perhaps it's so as to have one formula
// regardless of whether N is even or odd.  To see this, the number of windows
// at highest k, in the paper's notation, is N-khi+1 -(khi+2) + 1 = N-2*khi.
// Since the paper gives khi=L=ceil(N/2)-1, the number of windows is
// N-2*(ceil(N/2)-1) = N-2*ceil(N/2)+2 = 2 for even N, 3 for odd N.  If khi
// were increased by 1 regardless of the parity of N, the number of windows
// would be 0 for even N, which is no good, or 1 for odd N.

// Let x = {x0, x1, ... x(n-1)} be the sample of length n.  First calculate
// the least-squares straight line fit to x and subtract it.  Then,
// let el = ceil(n/2) - 1,, and for k=1,...,el, kix=k-1, do the following:
//   Not for calculation, let wk = 2*(k+1) be the window width, or more
//   precisely, comparison length to check if a given value of x, xi can be a
//   maximum or minimum vs. its neighbors, x(i+-k).  Consider the elements of x
//   too near its start or end to be able to check both such neighbors within x,
//   are not maxima or minima.  Construct two el x n matrices mpk, mtr, of
//   doubles with  alpha + random uniform number in [0, 1] (evaluated for each
//   element of mpk, mtr that's given this value) being assigned to the elements
//   of mpk that aren't (strict) maxima and of mtr that aren't (strict) minima,
//   and 0 assigned to those of mpk that are maxima, and of mtr that are minima,
//   as follows:
//     for i = 0,...,k-1=kix define not max/min since can't do both comparisons
//         i = n-k=n-kix-1,...,n-1 the same
//         i = k=kix+1,...,n-k-1=n-kix-2
//           xi > x(i-k)=x(i-kix-1) and xi > x(i+k)=x(i+kix+1) => max
//                                                             => mpk(kix,i) = 0
//           xi < x(i-k)=x(i-kix-1) and xi < x(i+k)=x(i+kix+1) => min
//                                                             => mtr(kix,i) = 0

// Then the remainder of the algorithm is applied to mpk and mtr separately for
// max and min, still adjusting for 0-base.  We refer to mpk/mtr as m here.
// Calculate gamma = {gamma0, gamma1, ..., gamma(el-1)} as
// gamma(kix) = sum of row k of m.  Define lamb (lambda) = the first kix for
// which gamma(kix) is a minimum among all the values of gamma, and remove rows
// kix+1,... from m, resulting in a (lamb + 1) x n matrix mr (or, simply don't
// use those rows).  Note that the method assumes that lamb > 0 in the form
// in the paper that computes something like the sample standard deviation (but
// no sqrt for 1/(lamb-1).  Since the more maxima/minima there are, the more
// zero entries there are in a row, the other entries being positive with
// expected value 1.5, the expected value of the sum of the row is
// 1.5 * (number of non-maxima/minima in the row).  Hence one would expect lamb
// to be the index of the row with the most maxima/minima in the row.
// Then for each column i of mr, calculate sumsqdev = sum over that column of
// the square of the value minus the mean of that column.  Those indices i for
// which sumsqdev is zero are returned in a vector of peaks/troughs, according
// to the algorithm in the paper (see below for optional modification).  (The
// algorithm actually takes the square root of sumsqdev and divides by lamb - 1,
// but I think this is a mistake though it may have been useful to the authors
// for their plots of sigma_i.  For the algorithm, it's unnecessary to take the
// square root, and dividing by lamb - 1 will blow up if lamb == 1.)

// I haven't figured out why the value of lamb is "correct", but it makes some
// sense intuitively.  If the signal is exactly periodic and monotone between
// peak and trough, peaks will be detected for scale values with windows up to
// the period.  Even if not "periodically monotone" there will be some region
// near the peak which falls away on either side, unless the sampling only gives
// one sample there.  The method fails to allow for lamb == 0, but this can be
// taken care of as a special case, just using the values in the first row of m.

// lamb is not always correct for all data.  For example it skips 5 of the 29
// peaks in data generated in the following (output in subdirectory ampd2)
// ../ampd -a 1. -b 1. -c .5 -d .1 -f 10. -g 70. -h 5. -i 5. -q 12. -s 0. -l 5. -n 100 -t 0. -u 0.1 -v 0.5 -w 0.083333 -z
// which calculates lamb == 1.  lamb == 0 works correctly for this data.
// I think the algorithm might work better for data with finer sampling of
// something smooth.

// I wasn't sure how close to zero to check sumsqdev; tried 8 * epsilon() and
// then 1.0e-16, both of which worked.  The paper says "equal", and I think
// that's what it should be.  In fact, I don't see why not just to check that
// all elements of the column (up through lamb) are zero.  If no randomness were
// used in m, then a column with *no* maxima/ minima at any scale would have
// sumsqdev zero and be chosen as having a peak/trough; not good.  Anyway it's
// simpler just to check all elements of the column zero, and so I added the
// -z command line option; I think it should always be set, and except that
// I was trying to implement the algorithm as it was, I would have made it
// the default and eliminated the option of setting it differently.

// Just to see if they mattered (so far I haven't found that they do), I added
// options to use normally distributed rather than uniformly distributed
// random numbers in the algorithm, and to change the mean and standard
// deviation of these distributions.

#include <cmath>

#include <fstream>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <utility>
#include <valarray>

using namespace std;

typedef valarray<double> Vad;
typedef pair<bool, double> UseIndexAndDevReturn;
typedef tuple<int, int, int, Vad, Vad> AmpdReturn;

// Print program usage.

void usage()
{
  const char* s
      = "Test AMPD.\n"
        "\n"
        "Usage:\n"
        " ./ampd [-a <double>] [-b <double>] [-c <double>] [-d <double>]\n"
        "        [-f <double>] [-g <double>] [-h <double>] [-i <double>]\n"
        "        [-q <double>] [-s <double>] [-l <double>] [-n <int>]\n"
        "        [-t <double>] [-u <double>] [-v <double>] [-w <double>] [-o] [-z]\n"
        "Defaults:\n"
        " ./ampd -a 1. -b 1. -c .5 -d .1 -f 10. -g 70. -h 5. -i 5. -q 12. -s 0. -l 5.\n"
        "        -n 1000 -t 0. -u 1.0 -v .5 -w 0.08333333\n"
        "\n"
        "Arguments (all optional):\n"
        " -a --a         coefficient of freq. f1/fs term     [default  1.0]\n"
        " -b --b         coefficient of freq. f2/fs term     [default  1.0]\n"
        " -c --c         coefficient of freq. f3/fs term     [default  0.5]\n"
        " -d --d         coefficient of random error term    [default  0.1]\n"
        " -f --f1        frequency 1 (will be divided by fs) [default 10.0]\n"
        " -g --f2        frequency 2 (will be divided by fs) [default 70.0]\n"
        " -h --f3_start  frequency 3 starting value (/fs)    [default  5.0]\n"
        " -i --f3_end    frequency 3 ending value   (/fs)    [default  5.0]\n"
        " -q --fs        frequency divisor                   [default 12.0]\n"
        " -s --start_t   starting time in 'seconds'          [default  0.0]\n"
        " -l --len_t     time length   in 'seconds'          [default  5.0]\n"
        " -n --n         number of samples of time series - twice its square times\n"
        "                sizeof(double) must fit in memory   [default 1000]\n"
        " -t --err_mean  mean of normal random error         [default  0.0]\n"
        " -u --err_stdev standard dev of normal random error [default  1.0]\n"
        " -v --alg_mean  mean of random nrs used in alg      [default  0.5]\n"
        " -w --alg_stdev standard deviation rand nrs in alg  [default  1/12]\n"
        "\n"
        "Options:\n"
        " -h --help   Show this help message and exit.\n"
        " -o --normal Use normal rather than uniform dist for rand nrs in alg.\n"
        " -z --zero   Test for column zero rather than zero variance.\n"
        "\n";
  std::cout << s;
}

// Must be called with setup=true before other use!  The default initialization
// for stdev is no good.

double alg_rand(
    bool setup = false, bool arg_normal = false, double arg_mean = 0.5,
    double arg_stdev = 1.0 / 12.0)
{
  static bool normal;
  static double mean;
  static double stdev;
  static default_random_engine rng;
  static uniform_real_distribution<double> uniform_dist(0.0, 1.0);
  static normal_distribution<double> normal_dist(0.0, 1.0);
  if(setup)
  {
    normal = arg_normal;
    if(normal)
    {
      mean = arg_mean;
      stdev = arg_stdev;
      normal_dist = normal_distribution<double>(mean, stdev);
    }
    else
    {
      mean = arg_mean;
      stdev = arg_stdev;
      // using mean=(a+b)/2, stdev=(b-a)^2/12
      double a = mean - stdev * sqrt(3.0);
      double b = mean + stdev * sqrt(3.0);
      uniform_dist = uniform_real_distribution<double>(a, b);
    }
  }
  if(normal)
    return normal_dist(rng);
  else
    return uniform_dist(rng);
}

double calc_sum_sq_dev(Vad v)
{
  double mean = v.sum() / v.size();
  v -= mean;
  v *= v;
  return v.sum();
}

// Faster than calc_nr_nonzero, if don't need the graphs.

bool is_zero(Vad v)
{
  for(int i = 0; i < v.size(); i++)
  {
    if(v[i] != 0.0)
      return false;
  }
  return true;
}

int calc_nr_nonzero(Vad v)
{
  int nr_nonzero = 0;
  for(int i = 0; i < v.size(); i++)
  {
    if(v[i] != 0.0)
      nr_nonzero++;
  }
  return nr_nonzero;
}

// Faster than use_index_and_dev if don't need graphs.

bool use_index(Vad v, bool col_zero)
{
  static constexpr double sum_sq_dev_min = 1.0e-16;
  if(col_zero)
    return is_zero(v);
  else
    return (calc_sum_sq_dev(v) < sum_sq_dev_min);
}

UseIndexAndDevReturn use_index_and_dev(Vad v, bool col_zero)
{
  static constexpr double sum_sq_dev_min = 1.0e-16;
  if(col_zero)
  {
    int nr_nonzero = calc_nr_nonzero(v);
    return make_pair(nr_nonzero == 0, double(nr_nonzero) / double(v.size()));
  }
  else
  {
    double sum_sq_dev = calc_sum_sq_dev(v);
    return make_pair(sum_sq_dev < sum_sq_dev_min, sqrt(sum_sq_dev / double(v.size())));
  }
}

AmpdReturn ampd(
    Vad x, double alg_mean, double alg_stdev, bool normal = false, bool col_zero = false,
    bool write_files = false, const char* lms_str = NULL, const char* gamma_str = NULL,
    const char* peaks_str = NULL, const char* troughs_str = NULL)
{
  static const double alpha = 1.0;
  int n = x.size();
  int el = ceil(n / 2) - 1;
  Vad mpk(el * n); // initializes elements to 0.0
  Vad mtr(el * n); // initializes elements to 0.0

  alg_rand(true, normal, alg_mean, alg_stdev); // setup=true before other use.
  int row_start = 0;
  for(int kix = 0; kix < el; kix++, row_start += n)
  {
    for(int i = 0; i <= kix; i++)
    {
      mpk[row_start + i] = alpha + alg_rand();
      mtr[row_start + i] = alpha + alg_rand();
    }
    for(int i = kix + 1; i < n - kix - 1; i++)
    {
      if(x[i] <= x[i - kix - 1] || x[i] <= x[i + kix + 1])
        mpk[row_start + i] = alpha + alg_rand();
      if(x[i] >= x[i - kix - 1] || x[i] >= x[i + kix + 1])
        mtr[row_start + i] = alpha + alg_rand();
    }
    for(int i = n - kix - 1; i < n; i++)
    {
      mpk[row_start + i] = alpha + alg_rand();
      mtr[row_start + i] = alpha + alg_rand();
    }
  }

  Vad pk_gamma(el);
  Vad tr_gamma(el);
  row_start = 0;
  for(int kix = 0; kix < el; kix++, row_start += n)
  {
    pk_gamma[kix] = Vad(mpk[slice(row_start, n, 1)]).sum();
    tr_gamma[kix] = Vad(mtr[slice(row_start, n, 1)]).sum();
  }
  double min_pk_gamma = pk_gamma[0];
  double min_tr_gamma = tr_gamma[0];
  int min_pk_gamma_kix = 0;
  int min_tr_gamma_kix = 0;
  for(int kix = 1; kix < el; kix++)
  {
    if(pk_gamma[kix] < min_pk_gamma)
    {
      min_pk_gamma = pk_gamma[kix];
      min_pk_gamma_kix = kix;
    }
    if(tr_gamma[kix] < min_tr_gamma)
    {
      min_tr_gamma = tr_gamma[kix];
      min_tr_gamma_kix = kix;
    }
  }
  int pk_lamb = min_pk_gamma_kix;
  int tr_lamb = min_tr_gamma_kix;

  Vad pk_zero_dev_ixs(n);
  Vad tr_zero_dev_ixs(n);
  int pk_zero_ixs_ix = 0;
  int tr_zero_ixs_ix = 0;
  Vad pk_dev(n); // for writing files for graphs
  for(int i = 0; i < n; i++)
  {
    // use_index is faster if don't need graphs
    UseIndexAndDevReturn pk_ret
        = use_index_and_dev(Vad(mpk[slice(i, pk_lamb + 1, n)]), col_zero);
    pk_dev[i] = pk_ret.second; // for writing files for graphs
    // could use use_index but want to use same code for pk and tr
    UseIndexAndDevReturn tr_ret
        = use_index_and_dev(Vad(mtr[slice(i, tr_lamb + 1, n)]), col_zero);
    if(pk_lamb == 0 && mpk[i] == 0.0 || pk_lamb != 0 && pk_ret.first)
    {
      pk_zero_dev_ixs[pk_zero_ixs_ix] = i;
      pk_zero_ixs_ix++;
    }
    if(tr_lamb == 0 && mtr[i] == 0.0 || tr_lamb != 0 && tr_ret.first)
    {
      tr_zero_dev_ixs[tr_zero_ixs_ix] = i;
      tr_zero_ixs_ix++;
    }
  }

  if(write_files)
  {
    ofstream lms(lms_str);
    int row_start = 0;
    for(int k = 0; k < el; k++, row_start += n)
      for(int i = 0; i < n; i++)
        lms << i << " " << k << " " << mpk[row_start + i] << endl;
    lms.close();

    ofstream gamma(gamma_str);
    for(int k = 0; k < el; k++)
      gamma << k << " " << pk_gamma[k] / double(n) << endl;
    gamma.close();

    ofstream peaks(peaks_str);
    for(int j = 0; j < pk_zero_ixs_ix; j++)
    {
      int i = pk_zero_dev_ixs[j];
      peaks << i << " " << x[i] << endl;
    }
    peaks.close();

    ofstream troughs(troughs_str);
    for(int j = 0; j < tr_zero_ixs_ix; j++)
    {
      int i = tr_zero_dev_ixs[j];
      troughs << i << " " << x[i] << endl;
    }
    troughs.close();
  }

  return make_tuple(
      el, pk_lamb, tr_lamb, Vad(pk_zero_dev_ixs[slice(0, pk_zero_ixs_ix, 1)]),
      Vad(tr_zero_dev_ixs[slice(0, tr_zero_ixs_ix, 1)]));
}
