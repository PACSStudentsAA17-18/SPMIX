// [[Rcpp::depends(BH)]]
// [[Rcpp::depends(RcppEigen)]]
// [[Rcpp::depends(RcppParallel)]]
// [[Rcpp::depends(StanHeaders)]]
#define STRICT_R_HEADERS
#include <stan/math/fwd/mat.hpp>
#include <stan/math/mix/mat.hpp>
#include <stan/math.hpp>
#include <Rcpp.h>
#include <RcppEigen.h>

#include <Eigen/Dense>
#include <unsupported/Eigen/KroneckerProduct>
#include <string>
#include <memory>
#include <functional>

#include "PolyaGamma.h"
#include "sampler_params.pb.h"
#include "univariate_mixture_state.pb.h"
#include "optimization_options.pb.h"
#include "recordio.h"
#include "utils.h"
#include "newton_method.h"
#include "gradient_ascent.h"
#include "functors.h"
#include "sampler_rjmcmc.h"

//' Simple test with stan/math C++ library
//'
//' Simply computes logN(1|2,3)
//' @export
// [[Rcpp::export]]
void stan_HelloWorld() {
	Rcpp::Rcout << "log normal(1 | 2, 3) = " << stan::math::normal_log(1, 2, 3) << std::endl;
	PolyaGamma pg;
	Rcpp::Rcout << "A PolyaGamma object has been instanciated!" << std::endl;
	Rcpp::Rcout << "Testing for KroneckerProduct..." << std::endl;
	Eigen::MatrixXd A(3,3); A << 1, 2, 3, 4, 5, 6, 7, 8, 9;
	Eigen::MatrixXd B(2,2); B << 4, 3, 2, 1;
	Rcpp::Rcout << "A:\n" << A << std::endl << std::endl;
	Rcpp::Rcout << "B:\n" << B << std::endl << std::endl;
	Eigen::MatrixXd C = Eigen::kroneckerProduct(A,B).eval();
	Rcpp::Rcout << "A x B:\n" << C << std::endl << std::endl;
  return;
}


//' Serialization testing
//' @export
// [[Rcpp::export]]
std::vector<Rcpp::RawVector> fromProto_tostring() {

	Rcpp::Rcout << "Reading params..." << std::endl;
	SamplerParams params;
    params = loadTextProto<SamplerParams>("/home/m_gianella/Documents/R-Packages/SPMIX/inst/input_files/sampler_params.asciipb");
    Rcpp::Rcout << params.DebugString() << std::endl;

    Rcpp::Rcout << "Creating states..." << std::endl;
    std::vector<UnivariateMixtureState> states;
    UnivariateMixtureAtom* atom;
    for (int i = 0; i < 5; i++) {
        UnivariateMixtureState curr;
        curr.set_num_components(i + 3);
        for (int j = 0; j < i+3; j++) {
            atom = curr.add_atoms();
            atom->set_mean(-1.0 * i);
            atom->set_stdev(0.5 * i);
        }
        states.push_back(curr);
    }

    for (int i = 0; i < states.size(); ++i) {
    	Rcpp::Rcout << "Printing state " << i+1 << std::endl;
    	Rcpp::Rcout << states[i].DebugString() << std::endl << std::endl;
    }

    Rcpp::Rcout << "Serializing messages... ";
    std::vector<std::string> tmp;
    for (int i = 0; i < states.size(); ++i) {
    	std::string s;
		states[i].SerializeToString(&s);
		tmp.push_back(s);
    }

    Rcpp::Rcout << "done!" << std::endl;

    std::vector<Rcpp::RawVector> out;
    for (std::string elem : tmp)
      out.push_back(utils::str2raw(elem));

    return out;
}


//' Translation from serialization testing
//' @export
// [[Rcpp::export]]
void readingStates(std::vector<Rcpp::RawVector> raw_vect){

	Rcpp::Rcout << "Restoring original messages from raw vectors:" << std::endl;
  size_t i = 1;
	for (Rcpp::RawVector elem : raw_vect) {
		Rcpp::Rcout << "Message " << i++ << " is:" << std::endl;
		UnivariateMixtureState curr;
		curr.ParseFromString(utils::raw2str(elem));
		Rcpp::Rcout << curr.DebugString() << std::endl << std::endl;
	}
	return;
}

//' Simple test to see if messages in R can be passed to a C++ function
//' @export
// [[Rcpp::export]]
void messageFromR(Rcpp::S4 params) {
    Rcpp::Rcout << std::boolalpha << "Has pointer: " << params.hasSlot("pointer") << std::endl;
    Rcpp::Rcout << "S4 Class of type Message? " << params.is("Message") << std::endl;
    Rcpp::XPtr<SamplerParams> pt = params.slot("pointer");
    std::string tmp; SamplerParams obj; pt->SerializeToString(&tmp);
    obj.ParseFromString(tmp); Rcpp::Rcout << obj.DebugString() << std::endl;
    //Rcpp::Rcout << (*(pt.get())).DebugString() << std::endl;
    return;
}


//' Test to evaluate times and correctness of newton method for optimization
//' @export
// [[Rcpp::export]]
void newton_opt_test(const Rcpp::S4 & state, const std::vector<std::vector<double>> & data,
                     const Eigen::MatrixXd & W, const Rcpp::S4 & params, const Rcpp::S4 & options) {

	// Check S4 class for state
    if (not(state.is("Message") and Rcpp::as<std::string>(state.slot("type")) == "UnivariateState")) {
        throw std::runtime_error("Input 'state' is not of type Message::UnivariateState.");
    }

    // Check S4 class for params
    if (not(params.is("Message") and Rcpp::as<std::string>(params.slot("type")) == "SamplerParams")) {
        throw std::runtime_error("Input 'params' is not of type Message::SamplerParams.");
    }

    // Check S4 class for options
    if (not(options.is("Message") and Rcpp::as<std::string>(options.slot("type")) == "OptimOptions")) {
        throw std::runtime_error("Input 'options' is not of type Message::OptimOptions.");
    }

    // Create a deep-copy of the messages with the workaround
    std::string tmp;

    // State copy
    UnivariateState state_cp;
    Rcpp::XPtr<UnivariateState>(Rcpp::as<Rcpp::XPtr<UnivariateState>>(state.slot("pointer")))
    	->SerializeToString(&tmp); state_cp.ParseFromString(tmp);

    // Params copy
    SamplerParams params_cp;
    Rcpp::XPtr<SamplerParams>(Rcpp::as<Rcpp::XPtr<SamplerParams>>(params.slot("pointer")))
    	->SerializeToString(&tmp); params_cp.ParseFromString(tmp);

    // Options copy
    OptimOptions options_cp;
    Rcpp::XPtr<OptimOptions>(Rcpp::as<Rcpp::XPtr<OptimOptions>>(options.slot("pointer")))
    	->SerializeToString(&tmp); options_cp.ParseFromString(tmp);

    function::spmixLogLikelihood fun(data, W, params_cp, state_cp);
    //function::test_function fun;
    optimization::NewtonMethod<decltype(fun)> solver(fun, options_cp);

    // Initialize and executing Newton Method
    Eigen::VectorXd x0 = fun.init();
    Rcpp::Rcout << "x0: " << x0.transpose() << std::endl;
    Rcpp::Rcout << "Solving..." << std::endl;
    auto start = std::chrono::high_resolution_clock::now();
    solver.solve(x0);
    auto end = std::chrono::high_resolution_clock::now();
    double duration = std::chrono::duration<double>(end - start).count();
    Rcpp::Rcout << "Duration: " << duration << std::endl;
    optimization::OptimState currstate = solver.get_state();
    Rcpp::Rcout << "minimizer: " << currstate.current_minimizer.transpose() << std::endl;
    Rcpp::Rcout << "||grad_f(x)||: " << currstate.current_gradient.norm() << std::endl;
    Rcpp::Rcout << "||hess_f(x)||: " << currstate.current_hessian.norm() << std::endl;

	return;
}

//' Test to evaluate times and correctness of gradient ascent method for optimization on a test function
//' @export
// [[Rcpp::export]]
void grad_ascent_test(const Rcpp::S4 & state, const std::vector<std::vector<double>> & data,
                      const Eigen::MatrixXd & W, const Rcpp::S4 & params, const Rcpp::S4 & options) {

    // Check S4 class for state
    if (not(state.is("Message") and Rcpp::as<std::string>(state.slot("type")) == "UnivariateState")) {
        throw std::runtime_error("Input 'state' is not of type Message::UnivariateState.");
    }

    // Check S4 class for params
    if (not(params.is("Message") and Rcpp::as<std::string>(params.slot("type")) == "SamplerParams")) {
        throw std::runtime_error("Input 'params' is not of type Message::SamplerParams.");
    }

    // Check S4 class for options
    if (not(options.is("Message") and Rcpp::as<std::string>(options.slot("type")) == "OptimOptions")) {
        throw std::runtime_error("Input 'options' is not of type Message::OptimOptions.");
    }

    // Create a deep-copy of the messages with the workaround
    std::string tmp;

    // State copy
    UnivariateState state_cp;
    Rcpp::XPtr<UnivariateState>(Rcpp::as<Rcpp::XPtr<UnivariateState>>(state.slot("pointer")))
        ->SerializeToString(&tmp); state_cp.ParseFromString(tmp);

    // Params copy
    SamplerParams params_cp;
    Rcpp::XPtr<SamplerParams>(Rcpp::as<Rcpp::XPtr<SamplerParams>>(params.slot("pointer")))
        ->SerializeToString(&tmp); params_cp.ParseFromString(tmp);

    // Options copy
    OptimOptions options_cp;
    Rcpp::XPtr<OptimOptions>(Rcpp::as<Rcpp::XPtr<OptimOptions>>(options.slot("pointer")))
        ->SerializeToString(&tmp); options_cp.ParseFromString(tmp);

    // Instanciating functor and solver
    //function::test_function fun;
    function::spmixLogLikelihood fun(data, W, params_cp, state_cp);
    optimization::GradientAscent<decltype(fun)> solver(fun, options_cp);

    // Initialize and executing Gradient Ascent Method
    Eigen::VectorXd x0 = fun.init();
    Rcpp::Rcout << "x0: " << x0.transpose() << std::endl;
    Rcpp::Rcout << "Solving..." << std::endl;
    auto start = std::chrono::high_resolution_clock::now();
    solver.solve(x0);
    auto end = std::chrono::high_resolution_clock::now();
    double duration = std::chrono::duration<double>(end - start).count();
    Rcpp::Rcout << "Duration: " << duration << std::endl;
    optimization::OptimState currstate = solver.get_state();
    Rcpp::Rcout << "minimizer: " << currstate.current_minimizer.transpose() << std::endl;
    Rcpp::Rcout << "||grad_f(x)||: " << currstate.current_gradient_norm << std::endl;

	return;
}

//' Test to evaluate the acceptance rate in case of extension move
//' @export
// [[Rcpp::export]]
void IncreaseMove_test(const std::vector<std::vector<double>> & data,
                       const Eigen::MatrixXd & W, const Rcpp::S4 & params,
                       const Rcpp::S4 & state, const Rcpp::S4 & options) {

    // Check S4 class for params
    if (not(params.is("Message") and Rcpp::as<std::string>(params.slot("type")) == "SamplerParams")) {
        throw std::runtime_error("Input 'params' is not of type Message::SamplerParams.");
    }

    // Check S4 class for state
    if (not(state.is("Message") and Rcpp::as<std::string>(state.slot("type")) == "UnivariateState")) {
        throw std::runtime_error("Input 'state' is not of type Message::UnivariateState.");
    }

    // Check S4 class for options
    if (not(options.is("Message") and Rcpp::as<std::string>(options.slot("type")) == "OptimOptions")) {
        throw std::runtime_error("Input 'options' is not of type Message::OptimOptions.");
    }

    // Create a deep-copy of the messages with the workaround
    std::string tmp;

    // Params copydata
    SamplerParams params_cp;
    Rcpp::XPtr<SamplerParams>(Rcpp::as<Rcpp::XPtr<SamplerParams>>(params.slot("pointer")))
        ->SerializeToString(&tmp); params_cp.ParseFromString(tmp);

    // State copy
    UnivariateState state_cp;
    Rcpp::XPtr<UnivariateState>(Rcpp::as<Rcpp::XPtr<UnivariateState>>(state.slot("pointer")))
        ->SerializeToString(&tmp); state_cp.ParseFromString(tmp);

    // Options copy
    OptimOptions options_cp;
    Rcpp::XPtr<OptimOptions>(Rcpp::as<Rcpp::XPtr<OptimOptions>>(options.slot("pointer")))
        ->SerializeToString(&tmp); options_cp.ParseFromString(tmp);

    // Help quantities
    int numGroups = data.size();
    int numComponents = state_cp.num_components();
    std::mt19937_64 rng{213513435};
    double alpha;

    // Eliciting the approximated optimal proposal parameters
    function::spmixLogLikelihood loglik_extended(data, W, params_cp, state_cp);
    optimization::GradientAscent<decltype(loglik_extended)> solver(loglik_extended, options_cp);
    Eigen::VectorXd x0 = loglik_extended.init();
    solver.solve(x0);
    Eigen::VectorXd optMean = solver.get_state().current_minimizer;
    Eigen::MatrixXd optCov = -solver.get_state().current_hessian.inverse();

    // Simulating from the approximated optimal posterior
    Eigen::VectorXd x = stan::math::multi_normal_rng(optMean, optCov, rng);
    /*Eigen::VectorXd weightsMean = optMean.head(numGroups);
    Eigen::MatrixXd weightsCov = optCov.topLeftCorner(numGroups, numGroups);
    Eigen::VectorXd weights_new = stan::math::multi_normal_rng(weightsMean, weightsCov, rng);
    double means_new = stan::math::normal_rng(optMean(numGroups), optCov(numGroups, numGroups), rng);
    double var = optMean(numGroups+1);//*optMean(numGroups+1)*optMean(numGroups+1)*optMean(numGroups+1);
    double varvar = optCov(numGroups+1,numGroups+1);//*optCov(numGroups+1,numGroups+1)*optCov(numGroups+1,numGroups+1)*optCov(numGroups+1,numGroups+1);
    double stddevs_new = std::sqrt(1./stan::math::gamma_rng((var*var)/(varvar), var/varvar, rng));*/

    //Computing Acceptance Rate
    //Eigen::VectorXd x(numGroups+2); x << weights_new, means_new, stddevs_new;
    Rcpp::Rcout << "optMean: " << optMean.transpose() << "\n"
    			<< "optCov:\n " << optCov << "\n"
    			<< "x: " << x.transpose() << "\n"
    			<< "loglik_extended(x): " << loglik_extended(x) << "\n"
    			<< "poisson_num: " << stan::math::poisson_lpmf(numComponents+1, 1) << "\n"
    			<< "loglik_extended(): " << loglik_extended() << "\n"
    			<< "poisson_den: " << stan::math::poisson_lpmf(numComponents, 1) << "\n"
    			<< "approx_post: " << stan::math::multi_normal_lpdf(x, optMean, optCov) << std::endl;

    alpha = std::exp( loglik_extended(x)+stan::math::poisson_lpmf(numComponents+1, 1)
                     -loglik_extended()-stan::math::poisson_lpmf(numComponents, 1)
                     -stan::math::multi_normal_lpdf(x, optMean, optCov) );
    alpha = std::min(1., alpha);
    		/*-stan::math::multi_normal_lpdf(weights_new, weightsMean, weightsCov)
            -stan::math::normal_lpdf(means_new,optMean(numGroups), optCov(numGroups, numGroups))
            -stan::math::gamma_lpdf(1./(stddevs_new*stddevs_new), (var*var)/(varvar), var/varvar) );*/

    // Print acceptance rate
    Rcpp::Rcout << "alpha: " << alpha << std::endl << std::endl;
    return;
}

//' Test to evaluate the acceptance rate in case of reduction move
//' @export
// [[Rcpp::export]]
void ReduceMove_test(const std::vector<std::vector<double>> & data,
                     const Eigen::MatrixXd & W, const Rcpp::S4 & params,
                     const Rcpp::S4 & state, const Rcpp::S4 & options) {

    // Check S4 class for params
    if (not(params.is("Message") and Rcpp::as<std::string>(params.slot("type")) == "SamplerParams")) {
        throw std::runtime_error("Input 'params' is not of type Message::SamplerParams.");
    }

    // Check S4 class for state
    if (not(state.is("Message") and Rcpp::as<std::string>(state.slot("type")) == "UnivariateState")) {
        throw std::runtime_error("Input 'state' is not of type Message::UnivariateState.");
    }

    // Check S4 class for options
    if (not(options.is("Message") and Rcpp::as<std::string>(options.slot("type")) == "OptimOptions")) {
        throw std::runtime_error("Input 'options' is not of type Message::OptimOptions.");
    }

    // Create a deep-copy of the messages with the workaround
    std::string tmp;

    // Params copydata
    SamplerParams params_cp;
    Rcpp::XPtr<SamplerParams>(Rcpp::as<Rcpp::XPtr<SamplerParams>>(params.slot("pointer")))
        ->SerializeToString(&tmp); params_cp.ParseFromString(tmp);

    // State copy
    UnivariateState state_cp;
    Rcpp::XPtr<UnivariateState>(Rcpp::as<Rcpp::XPtr<UnivariateState>>(state.slot("pointer")))
        ->SerializeToString(&tmp); state_cp.ParseFromString(tmp);

    // Options copy
    OptimOptions options_cp;
    Rcpp::XPtr<OptimOptions>(Rcpp::as<Rcpp::XPtr<OptimOptions>>(options.slot("pointer")))
        ->SerializeToString(&tmp); options_cp.ParseFromString(tmp);

    // Help quantities
    int numGroups = data.size();
    int numComponents = state_cp.num_components();
    std::mt19937_64 rng{213513435};
    double alpha;
    Eigen::MatrixXd transformed_weights(state_cp.groupparams().size(), numComponents-1);
    Eigen::MatrixXd weights(state_cp.groupparams().size(), numComponents);
    for (int i = 0; i < state_cp.groupparams().size(); ++i) {
        Eigen::VectorXd tmp(numComponents);
        for (int j = 0; j < numComponents; ++j) {
            tmp(j) = state_cp.groupparams()[i].weights()[j];
            weights(i,j) = state_cp.groupparams()[i].weights()[j];
        }
        transformed_weights.row(i) = utils::Alr(tmp, false);
    }
    double rho = state_cp.rho();
    Eigen::VectorXd means(numComponents);
    Eigen::VectorXd sqrt_stddevs(numComponents);
    for (int i = 0; i < numComponents; ++i) {
        means(i) = state_cp.atoms()[i].mean();
        sqrt_stddevs(i) = std::sqrt(state_cp.atoms()[i].stdev());
    }
    Eigen::MatrixXd Sigma(state_cp.sigma().rows(), state_cp.sigma().cols());
    for (int i = 0; i < state_cp.sigma().rows(); ++i) {
        for (int j = 0; j < state_cp.sigma().cols(); ++j) {
            Sigma(i,j) = state_cp.sigma().data()[i*state_cp.sigma().rows()+j];
        }
    }

    // DEBUG
    /*Rcpp::Rcout << "transformed_weights:\n" << transformed_weights << "\n"
                << "means: " << means.transpose() << "\n"
                << "sqrt_stddev: " << sqrt_stddevs.transpose() << std::endl;*/

    // Building the reduced loglikelihood
    Eigen::VectorXd transformed_weights_vect_reduced =
        Eigen::Map<Eigen::VectorXd>(transformed_weights.block(0,0, numGroups, numComponents - 2).data(),
                                    transformed_weights.block(0,0, numGroups, numComponents - 2).size());
    // Rcpp::Rcout << "transformed_weights_vect_reduced: " << transformed_weights_vect_reduced.transpose() << std::endl;
    function::spmixLogLikelihood
        loglik_reduced(data, W, params_cp, numGroups, numComponents-1, rho,
                       means.head(numComponents-1), sqrt_stddevs.head(numComponents-1),
                       transformed_weights_vect_reduced, Sigma.block(0,0, numComponents-2, numComponents-2));

    // Eliciting the approximated optimal proposal parameters
    optimization::GradientAscent<decltype(loglik_reduced)> solver(loglik_reduced, options_cp);
    Eigen::VectorXd x0(numGroups+2);
    x0 << transformed_weights.col(numComponents-2), means.tail(1), sqrt_stddevs.tail(1);
    // Rcpp::Rcout << "x0: " << x0.transpose() << std::endl;
    solver.solve(x0);
    Eigen::VectorXd optMean = solver.get_state().current_minimizer;
    Eigen::MatrixXd optCov = -solver.get_state().current_hessian.inverse();

    // Compute Acceptance rate
    Rcpp::Rcout << "optMean: " << optMean.transpose() << "\n"
                << "optCov:\n " << optCov << "\n"
                << "x: " << x0.transpose() << "\n"
                << "loglik_reduced(x0): " << loglik_reduced(x0) << "\n"
                << "poisson_num: " << stan::math::poisson_lpmf(numComponents-1, 1) << "\n"
                << "loglik_reduced(): " << loglik_reduced() << "\n"
                << "poisson_den: " << stan::math::poisson_lpmf(numComponents, 1) << "\n"
                << "approx_post: " << stan::math::multi_normal_lpdf(x0, optMean, optCov) << std::endl;

    alpha = std::exp( loglik_reduced()+stan::math::poisson_lpmf(numComponents-1, 1)
                     -loglik_reduced(x0)-stan::math::poisson_lpmf(numComponents, 1)
                     +stan::math::multi_normal_lpdf(x0, optMean, optCov) );
    alpha = std::min(1., alpha);

    // Print acceptance rate
    Rcpp::Rcout << "alpha: " << alpha << std::endl << std::endl;
    return;
}

//' Test fot the RJSampler
//' @export
// [[Rcpp::export]]
void RJsampler_test(const std::vector<std::vector<double>> & data,
                    const Eigen::MatrixXd & W, const Rcpp::S4 & params, const Rcpp::S4 & options) {

    // Check S4 class for params
    if (not(params.is("Message") and Rcpp::as<std::string>(params.slot("type")) == "SamplerParams")) {
        throw std::runtime_error("Input 'params' is not of type Message::SamplerParams.");
    }

    // Check S4 class for options
    if (not(options.is("Message") and Rcpp::as<std::string>(options.slot("type")) == "OptimOptions")) {
        throw std::runtime_error("Input 'options' is not of type Message::OptimOptions.");
    }

    // Create a deep-copy of the messages with the workaround
    std::string tmp;

    // Params copy
    SamplerParams params_cp;
    Rcpp::XPtr<SamplerParams>(Rcpp::as<Rcpp::XPtr<SamplerParams>>(params.slot("pointer")))
        ->SerializeToString(&tmp); params_cp.ParseFromString(tmp);

    // Options copy
    OptimOptions options_cp;
    Rcpp::XPtr<OptimOptions>(Rcpp::as<Rcpp::XPtr<OptimOptions>>(options.slot("pointer")))
        ->SerializeToString(&tmp); options_cp.ParseFromString(tmp);

    // Various tests
    SpatialMixtureRJSampler sampler(params_cp, data, W, options_cp);
    sampler.init();
    sampler.sampleSigma();
    //sampler.betweenModelMove();

    return;
}
