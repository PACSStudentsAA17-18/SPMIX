// Generated by using Rcpp::compileAttributes() -> do not edit by hand
// Generator token: 10BE3573-1514-4C36-9D1C-5A225CD40393

#include <RcppEigen.h>
#include <Rcpp.h>

using namespace Rcpp;

// timesTwo
int timesTwo(int x);
RcppExport SEXP _SPMIX_timesTwo(SEXP xSEXP) {
BEGIN_RCPP
    Rcpp::RObject rcpp_result_gen;
    Rcpp::RNGScope rcpp_rngScope_gen;
    Rcpp::traits::input_parameter< int >::type x(xSEXP);
    rcpp_result_gen = Rcpp::wrap(timesTwo(x));
    return rcpp_result_gen;
END_RCPP
}
// stan_HelloWorld
void stan_HelloWorld();
RcppExport SEXP _SPMIX_stan_HelloWorld() {
BEGIN_RCPP
    Rcpp::RNGScope rcpp_rngScope_gen;
    stan_HelloWorld();
    return R_NilValue;
END_RCPP
}

static const R_CallMethodDef CallEntries[] = {
    {"_SPMIX_timesTwo", (DL_FUNC) &_SPMIX_timesTwo, 1},
    {"_SPMIX_stan_HelloWorld", (DL_FUNC) &_SPMIX_stan_HelloWorld, 0},
    {NULL, NULL, 0}
};

RcppExport void R_init_SPMIX(DllInfo *dll) {
    R_registerRoutines(dll, NULL, CallEntries, NULL, NULL);
    R_useDynamicSymbols(dll, FALSE);
}