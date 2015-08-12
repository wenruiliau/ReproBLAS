#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <idxdBLAS.h>

#include "../common/test_opt.h"
#include "../common/test_time.h"
#include "../common/test_metric.h"

#include "../../config.h"

#include "bench_vecvec_fill_header.h"

static opt_option fold;

static void bench_ssiconv_options_initialize(void){
  fold._int.header.type       = opt_int;
  fold._int.header.short_name = 'k';
  fold._int.header.long_name  = "fold";
  fold._int.header.help       = "fold";
  fold._int.required          = 0;
  fold._int.min               = 0;
  fold._int.max               = SIMAXFOLD;
  fold._int.value             = SIDEFAULTFOLD;
}

int bench_vecvec_fill_show_help(void){
  bench_ssiconv_options_initialize();

  opt_show_option(fold);

  return 0;
}

const char* bench_vecvec_fill_name(int argc, char** argv){
  (void)argc;
  (void)argv;
  static char name_buffer[MAX_LINE];

  bench_ssiconv_options_initialize();
  opt_eval_option(argc, argv, &fold);

  snprintf(name_buffer, MAX_LINE * sizeof(char), "Benchmark [ssiconv] (fold = %d)", fold._int.value);
  return name_buffer;
}

int bench_vecvec_fill_test(int argc, char** argv, int N, int FillX, double RealScaleX, double ImagScaleX, int incX, int FillY, double RealScaleY, double ImagScaleY, int incY, int trials){
  (void)argc;
  (void)argv;
  (void)FillY;
  (void)RealScaleY;
  (void)ImagScaleY;
  (void)incY;
  int rc = 0;
  int i;
  int j;
  float res = 0.0;
  float_indexed *ires;

  bench_ssiconv_options_initialize();
  opt_eval_option(argc, argv, &fold);

  util_random_seed();

  float *X = util_svec_alloc(N, incX);

  //fill x
  util_svec_fill(N, X, incX, FillX, RealScaleX, ImagScaleX);

  if(fold._int.value == 0){
    ires = idxd_sialloc(SIMAXFOLD);
    idxd_sisetzero(SIMAXFOLD, ires);
    idxdBLAS_sissum(SIMAXFOLD, N, X, incX, ires);
    time_tic();
    for(j = 1; j <= SIMAXFOLD; j++){
      for(i = 0; i < trials; i++){
        res = idxd_ssiconv(j, ires);
      }
    }
    time_toc();
    free(ires);
  }else{
    ires = idxd_sialloc(fold._int.value);
    idxd_sisetzero(fold._int.value, ires);
    idxdBLAS_sissum(fold._int.value, N, X, incX, ires);
    time_tic();
    for(i = 0; i < trials; i++){
      res = idxd_ssiconv(fold._int.value, ires);
    }
    time_toc();
    free(ires);
  }

  metric_load_double("time", time_read());
  metric_load_float("res", res);
  metric_load_double("trials", (double)trials);
  if(fold._int.value == 0){
    metric_load_double("input", (double)SIMAXFOLD);
  }else{
    metric_load_double("input", (double)1);
  }
  metric_load_double("output", (double)1);
  metric_dump();

  free(X);
  return rc;
}
