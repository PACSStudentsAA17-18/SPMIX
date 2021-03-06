#ifndef FUNCTORS_HPP
#define FUNCTORS_HPP

#define STRICT_R_HEADERS
#include <Rcpp.h>
#include <Eigen/Dense>
#include <unsupported/Eigen/KroneckerProduct>
#include <stan/math/prim/mat.hpp>

#include <exception>

#include "univariate_mixture_state.pb.h"
#include "sampler_params.pb.h"
#include "utils.h"
#include "mcmc_utils.h"

namespace function {

template<typename D>
class functorBase {
  private:
  	D & derived(){return static_cast<D &>(*this);}
  	D const & derived() const {return static_cast<D const &>(*this);}
  public:
	template<typename T> T operator()(const Eigen::Matrix<T,Eigen::Dynamic,1> & x) const {return derived().operator()(x);};
	Eigen::VectorXd init() const {return derived().init();};
};

class spmixLogLikelihood : public functorBase<spmixLogLikelihood> {
  private:
  	std::vector<std::vector<double>> data;
	Eigen::MatrixXd W;
	SamplerParams params;
	int numGroups;
	int numComponents;
	double rho;
	Eigen::VectorXd means;
	Eigen::VectorXd sqrt_stddevs;
	Eigen::MatrixXd transformed_weights;
	Eigen::MatrixXd Sigma;
	int dropped_index;
	Eigen::MatrixXd F;
  public:
	/*spmixLogLikelihood(const std::vector<std::vector<double>> & _data, const Eigen::MatrixXd & _W,
					   const SamplerParams & _params, const UnivariateState & _state);*/

	spmixLogLikelihood(const std::vector<std::vector<double>> & _data,
					   const Eigen::MatrixXd & _W,
					   const SamplerParams & _params,
					   int _numGroups,
					   int _numComponents,
					   double _rho,
					   const Eigen::VectorXd & _means,
					   const Eigen::VectorXd & _sqrt_stddevs,
					   const Eigen::MatrixXd & _transformed_weights,
					   const Eigen::MatrixXd & _Sigma,
					   int _dropped_index = -1);

	double operator()() const;
	template<typename T> T operator() (const Eigen::Matrix<T, Eigen::Dynamic, 1> & x) const;
	Eigen::VectorXd init(const int & randComp) const;
};

class test_function : public functorBase<test_function> {
  public:
  	double operator()() const {return 0.;};
	template<typename T> T operator() (const Eigen::Matrix<T,Eigen::Dynamic,1> & x) const;
	Eigen::VectorXd init() const;
};

#include "functors_impl.h"

}; // namespace function

#endif // FUNCTORS_HPP
