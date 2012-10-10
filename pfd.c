#include "pfd.h"


ticks* pfd_store[PFD_NUM_STORES];

void 
pfd_store_init(uint32_t num_entries)
{
  uint32_t i;
  for (i = 0; i < PFD_NUM_STORES; i++)
    {
      pfd_store[i] = (ticks*) malloc(num_entries * sizeof(ticks));
      assert(pfd_store[i] != NULL);
    }
}


static inline 
double absd(double x)
{
  if (x >= 0)
    {
      return x;
    }
  else 
    {
      return -x;
    }
}


void 
print_abs_deviation(abs_deviation_t* abs_dev)
{
  PRINT("\n -- abs deviation stats:");
  PRINT("    num : %llu", abs_dev->num_vals);
  PRINT("    avg : %-10.1f (10%%: %-4.1f, 25%%: %-4.1f)", 
	abs_dev->avg, abs_dev->avg_10p, abs_dev->avg_25p);
  PRINT("    dev : %.1f", abs_dev->abs_dev);
  PRINT("    min : %-10.1f (elem: %llu)", abs_dev->min_dev, abs_dev->min_dev_idx);
  PRINT("    max : %-10.1f (elem: %llu)", abs_dev->max_dev, abs_dev->max_dev_idx);
  double v10p = 100 * 
    (1 - (abs_dev->num_vals - abs_dev->num_dev_10p) / (double) abs_dev->num_vals);
  PRINT("  0-10%% : %-10u (%4.1f%%)", abs_dev->num_dev_10p, v10p);
  double v25p = 100 
    * (1 - (abs_dev->num_vals - abs_dev->num_dev_25p) / (double) abs_dev->num_vals);
  PRINT(" 10-25%% : %-10u (%4.1f%%)", abs_dev->num_dev_25p, v25p);
  double v50p = 100 * 
    (1 - (abs_dev->num_vals - abs_dev->num_dev_50p) / (double) abs_dev->num_vals);
  PRINT(" 25-50%% : %-10u (%4.1f%%)", abs_dev->num_dev_50p, v50p);
  double v75p = 100 * 
    (1 - (abs_dev->num_vals - abs_dev->num_dev_75p) / (double) abs_dev->num_vals);
  PRINT(" 50-75%% : %-10u (%4.1f%%)", abs_dev->num_dev_75p, v75p);
  double vrest = 100 * 
    (1 - (abs_dev->num_vals - abs_dev->num_dev_rst) / (double) abs_dev->num_vals);
  PRINT("75-100%% : %-10u (%4.1f%%)", abs_dev->num_dev_rst, vrest);
}

void
get_abs_deviation(ticks* vals, size_t num_vals, abs_deviation_t* abs_dev)
{
  abs_dev->num_vals = num_vals;
  ticks sum_vals = 0;
  uint32_t i;
  for (i = 0; i < num_vals; i++)
    {
      if ((int64_t) vals[i] < 0)
	{
	  vals[i] = 0;
	}
      sum_vals += vals[i];
    }
  double avg = sum_vals / (double) num_vals;
  abs_dev->avg = avg;
  double max_dev = 0;
  double min_dev = DBL_MAX;
  uint64_t max_dev_idx, min_dev_idx;
  uint32_t num_dev_10p = 0; ticks sum_vals_10p = 0; double dev_10p = 0.1 * avg;
  uint32_t num_dev_25p = 0; ticks sum_vals_25p = 0; double dev_25p = 0.25 * avg;
  uint32_t num_dev_50p = 0; double dev_50p = 0.5 * avg;
  uint32_t num_dev_75p = 0; double dev_75p = 0.75 * avg;
  uint32_t num_dev_rst = 0;

  double sum_adev = 0;
  for (i = 0; i < num_vals; i++)
    {
      double diff = vals[i] - avg;
      double ad = absd(diff);
      if (ad > max_dev)
	{
	  max_dev = ad;
	  max_dev_idx = i;
	}
      else if (ad < min_dev)
	{
	  min_dev = ad;
	  min_dev_idx = i;
	}

      if (ad <= dev_10p)
	{
	  num_dev_10p++;
	  sum_vals_10p += vals[i];
	}
      else if (ad <= dev_25p)
	{
	  num_dev_25p++;
	  sum_vals_25p += vals[i];
	}
      else if (ad <= dev_50p)
	{
	  num_dev_50p++;
	}
      else if (ad <= dev_75p)
	{
	  num_dev_75p++;
	}
      else
	{
	  num_dev_rst++;
	}

      sum_adev += ad;
    }
  abs_dev->min_dev = min_dev;
  abs_dev->min_dev_idx = min_dev_idx;
  abs_dev->max_dev = max_dev;
  abs_dev->max_dev_idx = max_dev_idx;
  abs_dev->num_dev_10p = num_dev_10p;
  abs_dev->num_dev_25p = num_dev_25p;
  abs_dev->num_dev_50p = num_dev_50p;
  abs_dev->num_dev_75p = num_dev_75p;
  abs_dev->num_dev_rst = num_dev_rst;

  abs_dev->avg_10p = sum_vals_10p / (double) num_dev_10p;
  abs_dev->avg_25p = sum_vals_25p / (double) num_dev_25p;

  double adev = sum_adev / num_vals;
  abs_dev->abs_dev = adev;
}
