#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>

#include <binned.h>
#include <binnedBLAS.h>
#include <reproBLAS.h>

#include "../common/test_opt.h"
#include "../common/test_matvec_fill_header.h"

#include "wrap_rcgemv.h"

static opt_option max_blocks;
static opt_option shuffles;
static opt_option fold;

static void corroborate_rcgemv_options_initialize(void){
  max_blocks._int.header.type       = opt_int;
  max_blocks._int.header.short_name = 'B';
  max_blocks._int.header.long_name  = "blocks";
  max_blocks._int.header.help       = "maximum number of blocks";
  max_blocks._int.required          = 0;
  max_blocks._int.min               = 1;
  max_blocks._int.max               = INT_MAX;
  max_blocks._int.value             = 1024;

  shuffles._int.header.type       = opt_int;
  shuffles._int.header.short_name = 'S';
  shuffles._int.header.long_name  = "shuffles";
  shuffles._int.header.help       = "number of times to shuffle";
  shuffles._int.required          = 0;
  shuffles._int.min               = 0;
  shuffles._int.max               = INT_MAX;
  shuffles._int.value             = 5;

  fold._int.header.type       = opt_int;
  fold._int.header.short_name = 'k';
  fold._int.header.long_name  = "fold";
  fold._int.header.help       = "fold";
  fold._int.required          = 0;
  fold._int.min               = 2;
  fold._int.max               = binned_SBMAXFOLD;
  fold._int.value             = SIDEFAULTFOLD;
}

int corroborate_rcgemv(int fold, char Order, char TransA, int M, int N, float complex *alpha, float complex *A, int lda, float complex *X, int incX, float complex *beta, float complex *Y, float_complex_binned *YI, int incY, float complex *ref, int max_num_blocks) {

  int i;
  int num_blocks = 1;
  int block_opN;
  int opN;
  int opM;
  switch(TransA){
    case 'n':
    case 'N':
      opN = N;
      opM = M;
    break;
    default:
      opN = M;
      opM = N;
    break;
  }

  float complex *res = util_cvec_alloc(opM, incY);
  float_binned *Ires = malloc(opM * incY * binned_cbsize(fold));

  num_blocks = 1;
  while (num_blocks < opN && num_blocks <= max_num_blocks) {
    memcpy(res, Y, opM * incY * sizeof(complex float));
    memcpy(Ires, YI, opM * incY * binned_cbsize(fold));
    if (num_blocks == 1){
      wrap_rcgemv(fold, Order, TransA, M, N, alpha, A, lda, X, incX, beta, res, incY);
    }else {
      switch(TransA){
        case 'n':
        case 'N':
          {
            block_opN = (N + num_blocks - 1) / num_blocks;
            for (i = 0; i < N; i += block_opN) {
              block_opN = block_opN < N - i ? block_opN : (N-i);
              switch(Order){
                case 'r':
                case 'R':
                  binnedBLAS_cbcgemv(fold, Order, TransA, M, block_opN, alpha, A + i, lda, X + i * incX, incX, Ires, incY);
                  break;
                default:
                  binnedBLAS_cbcgemv(fold, Order, TransA, M, block_opN, alpha, A + i * lda, lda, X + i * incX, incX, Ires, incY);
                  break;
              }
            }
          }
          break;
        default:
          {
            block_opN = (M + num_blocks - 1) / num_blocks;
            for (i = 0; i < M; i += block_opN) {
              block_opN = block_opN < M - i ? block_opN : (M-i);
              switch(Order){
                case 'r':
                case 'R':
                  binnedBLAS_cbcgemv(fold, Order, TransA, block_opN, N, alpha, A + i * lda, lda, X + i * incX, incX, Ires, incY);
                  break;
                default:
                  binnedBLAS_cbcgemv(fold, Order, TransA, block_opN, N, alpha, A + i, lda, X + i * incX, incX, Ires, incY);
                  break;
              }
            }
          }
          break;
      }
      for(i = 0; i < opM; i++){
        binned_ccbconv_sub(fold, Ires + i * incY * binned_cbnum(fold), res + i * incY);
      }
    }
    for(i = 0; i < opM; i++){
      if(res[i * incY] != ref[i * incY]){
        printf("reproBLAS_rcgemv(A, X, Y)[num_blocks=%d] = %g + %gi != %g + %gi\n", num_blocks, creal(res[i * incY]), cimag(res[i * incY]), creal(ref[i * incY]), cimag(ref[i * incY]));
        return 1;
      }
    }
    num_blocks *= 2;
  }
  free(res);
  free(Ires);
  return 0;
}

int matvec_fill_show_help(void){
  corroborate_rcgemv_options_initialize();

  opt_show_option(fold);
  opt_show_option(max_blocks);
  opt_show_option(shuffles);
  return 0;
}

const char* matvec_fill_name(int argc, char** argv){
  static char name_buffer[MAX_LINE];

  corroborate_rcgemv_options_initialize();

  opt_eval_option(argc, argv, &fold);
  opt_eval_option(argc, argv, &max_blocks);

  snprintf(name_buffer, MAX_LINE * sizeof(char), "Corroborate rcgemv results fold=%d", fold._int.value);
  return name_buffer;
}

int matvec_fill_test(int argc, char** argv, char Order, char TransA, int M, int N, double RealAlpha, double ImagAlpha, int FillA, double RealScaleA, double ImagScaleA, int lda, int FillX, double RealScaleX, double ImagScaleX, int incX, double RealBeta, double ImagBeta, int FillY, double RealScaleY, double ImagScaleY, int incY){
  int rc = 0;
  int i;

  corroborate_rcgemv_options_initialize();

  opt_eval_option(argc, argv, &fold);
  opt_eval_option(argc, argv, &max_blocks);
  opt_eval_option(argc, argv, &shuffles);

  util_random_seed();
  int opN;
  int opM;
  char NTransA;
  switch(TransA){
    case 'n':
    case 'N':
      opN = N;
      opM = M;
      NTransA = 'T';
    break;
    default:
      opN = M;
      opM = N;
      NTransA = 'N';
    break;
  }

  float complex *A  = util_cmat_alloc(Order, M, N, lda);
  float complex *X  = util_cvec_alloc(opN, incX);
  float complex *Y  = util_cvec_alloc(opM, incY);
  float_complex_binned *YI = (float_complex_binned*)malloc(opM * incY * binned_cbsize(fold._int.value));
  float complex alpha = RealAlpha + I * ImagAlpha;
  float complex beta = RealBeta + I * ImagBeta;
  float complex betaY;

  int *P;

  util_cmat_fill(Order, 'n', M, N, A, lda, FillA, RealScaleA, ImagScaleA);
  util_cvec_fill(opN, X, incX, FillX, RealScaleX, ImagScaleX);
  util_cvec_fill(opM, Y, incY, FillY, RealScaleY, ImagScaleY);
  if(beta == 0.0){
    memset(YI, 0, opM * binned_cbsize(fold._int.value));
  }else if (beta == 1.0){
    for(i = 0; i < opM; i++){
      binned_cbcconv(fold._int.value, Y + i * incY, YI + i * incY * binned_cbnum(fold._int.value));
    }
  }else{
    for(i = 0; i < opM; i++){
      betaY = Y[i * incY] * beta;
      binned_cbcconv(fold._int.value, &betaY, YI + i * incY * binned_cbnum(fold._int.value));
    }
  }
  float complex *ref  = (float complex *)malloc(opM * incY * sizeof(float complex));

  //compute with unpermuted data
  memcpy(ref, Y, opM * incY * sizeof(float complex));

  wrap_ref_rcgemv(fold._int.value, Order, TransA, M, N, &alpha, A, lda, X, incX, &beta, ref, incY);

  rc = corroborate_rcgemv(fold._int.value, Order, TransA, M, N, &alpha, A, lda, X, incX, &beta, Y, YI, incY, ref, max_blocks._int.value);
  if(rc != 0){
    return rc;
  }

  P = util_identity_permutation(opN);
  util_cvec_reverse(opN, X, incX, P, 1);
  util_cmat_row_permute(Order, NTransA, M, N, A, lda, P, 1, NULL, 1);
  free(P);

  rc = corroborate_rcgemv(fold._int.value, Order, TransA, M, N, &alpha, A, lda, X, incX, &beta, Y, YI, incY, ref, max_blocks._int.value);
  if(rc != 0){
    return rc;
  }

  P = util_identity_permutation(opN);
  util_cvec_sort(opN, X, incX, P, 1, util_Increasing);
  util_cmat_row_permute(Order, NTransA, M, N, A, lda, P, 1, NULL, 1);
  free(P);

  rc = corroborate_rcgemv(fold._int.value, Order, TransA, M, N, &alpha, A, lda, X, incX, &beta, Y, YI, incY, ref, max_blocks._int.value);
  if(rc != 0){
    return rc;
  }

  P = util_identity_permutation(opN);
  util_cvec_sort(opN, X, incX, P, 1, util_Decreasing);
  util_cmat_row_permute(Order, NTransA, M, N, A, lda, P, 1, NULL, 1);
  free(P);

  rc = corroborate_rcgemv(fold._int.value, Order, TransA, M, N, &alpha, A, lda, X, incX, &beta, Y, YI, incY, ref, max_blocks._int.value);
  if(rc != 0){
    return rc;
  }

  P = util_identity_permutation(opN);
  util_cvec_sort(opN, X, incX, P, 1, util_Increasing_Magnitude);
  util_cmat_row_permute(Order, NTransA, M, N, A, lda, P, 1, NULL, 1);
  free(P);

  rc = corroborate_rcgemv(fold._int.value, Order, TransA, M, N, &alpha, A, lda, X, incX, &beta, Y, YI, incY, ref, max_blocks._int.value);
  if(rc != 0){
    return rc;
  }

  P = util_identity_permutation(opN);
  util_cvec_sort(opN, X, incX, P, 1, util_Decreasing_Magnitude);
  util_cmat_row_permute(Order, NTransA, M, N, A, lda, P, 1, NULL, 1);
  free(P);

  rc = corroborate_rcgemv(fold._int.value, Order, TransA, M, N, &alpha, A, lda, X, incX, &beta, Y, YI, incY, ref, max_blocks._int.value);
  if(rc != 0){
    return rc;
  }

  for(i = 0; i < shuffles._int.value; i++){
    P = util_identity_permutation(opN);
    util_cvec_shuffle(opN, X, incX, P, 1);
    util_cmat_row_permute(Order, NTransA, M, N, A, lda, P, 1, NULL, 1);
    free(P);

    rc = corroborate_rcgemv(fold._int.value, Order, TransA, M, N, &alpha, A, lda, X, incX, &beta, Y, YI, incY, ref, max_blocks._int.value);
    if(rc != 0){
      return rc;
    }
  }

  free(A);
  free(X);
  free(Y);
  free(YI);
  free(ref);

  return rc;
}
