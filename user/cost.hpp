#ifndef COST_HPP
#define COST_HPP

namespace sac {

  //! \warning Class MUST BE MODIFIED BY USER to accomodate NON-QUADRATIC terminal cost

  /*! 
    Class keeps track of the trajectory tracking cost.  It stores the current
    state through a reference to a state interpolator object and thus only
    requires calling an update method to re-compute trajectory cost \f$J_1 = 
    \int_{t_0}^{t_f} l(x(t))dt + m(x(t_f))\f$ after state / control updates.
  */
  class cost {
    state_type J1_, x_tf_;  // init w/ terminal cost then holds current cost
    double t0_, tf_;
    size_t J1steps_;
    Eigen::Matrix< double, xlen, 1 > mx_tf_;
    Eigen::Matrix< double, xlen, 1 >  mxdes_tf_;
    size_t indx_;
    const int wrap_size_;//number of states to be wrapped
  
  public:
    inc_cost m_lofx;          // incremental trajectory cost
    state_intp & m_x_intp;

    //! \todo: Make constructor reference and pointer inputs const.
    /*!
      Constructs a cost object from a state interpolation object and desired
      state trajectory.
      \param[in] x_intp state interpolation object
    */
    cost( state_intp & x_intp ) : 
		J1_(1), x_tf_( xlen ), t0_( 0.0 ), tf_( 0.0 ), 
	      J1steps_(0),
	      m_lofx( x_intp ), m_x_intp( x_intp ),
		mxdes_tf_(Eigen::Matrix< double,xlen,1 >::Zero(xlen,1)),
		wrap_size_(sizeof(x_wrap)/sizeof(x_wrap[0])) { }

    /*! 
      Get the cost at the terminal time, \f$m(x(t_f))\f$.
      \return \f$(x(t_f)-x_{des}(t_f))^T\;P_1\;(x(t_f)-x_{des}(t_f))\f$, 
      a quadratic form for \f$m(x(t_f))\f$.
    */
    inline double get_term_cost( ) {
      t0_ = m_lofx.begin();
      tf_ = m_lofx.end();
      m_x_intp( tf_, x_tf_ );//state at final time
	for (indx_ = 0; indx_ < wrap_size_; ++indx_ ) {AngleWrap( x_tf_[x_wrap[indx_]] );} // Angle wrapping (if any)
      State2Mat( x_tf_, mx_tf_ );
	get_DesTraj( tf_, mxdes_tf_ ); // Store desired trajectory point in m_mxdes
      return ((mx_tf_-mxdes_tf_).transpose()*P*(mx_tf_-mxdes_tf_))[0];
    }
  
    /*! 
      Get the derivative of the terminal time, \f$D_x m(x(t_f))\f$.
      \return \f$(x(t_f)-x_{des}(t_f))^T\;P_1\f$, which is \f$D_x m(x(t_f))\f$
      assuming a quadratic form for the terimal cost.
    */
    inline Eigen::Matrix< double, 1, xlen > get_dmdx( ) { 
      t0_ = m_lofx.begin();
      tf_ = m_lofx.end();
      m_x_intp( tf_, x_tf_ );
	for (indx_ = 0; indx_ < wrap_size_; ++indx_ ) {AngleWrap( x_tf_[x_wrap[indx_]] );} // Angle wrapping (if any)
      State2Mat( x_tf_, mx_tf_ );
	get_DesTraj( tf_, mxdes_tf_ ); // Store desired trajectory point in m_mxdes
      return (mx_tf_-mxdes_tf_).transpose()*P;
    }
  
    /*!
      The function computes the integral of \f$l(x)\f$ and appends it to a 
      provided terminal cost to return cost \f$J_1 = \int_{t_0}^{t_f} l(x(t)) 
      dt + m(x(t_f))\f$.
    */
    size_t compute_cost( state_type& term_cost );
  
    /*!
      Implicitly converts cost object to a double.
      \return cost \f$J_1 = \int_{t_0}^{t_f} l(x(t))dt + m(x(t_f))\f$.
    */
    operator double() { return J1_[0]; }

    /*!
      \return Returns # of integration steps in computing cost \f$J_1 = 
      \int_{t_0}^{t_f} l(x(t))dt + m(x(t_f))\f$.
    */
    size_t steps( ) { return J1steps_; }

    /*!
      Re-computes the cost stored in the cost object.  This should be called 
      to update the cost object after state / controls have been modified.
    */
    void update( ) {
      t0_ = m_lofx.begin();
      tf_ = m_lofx.end();
      J1_[0] = get_term_cost();  // terminal cost to be added on
      J1steps_ = compute_cost( J1_ );
    }
  };


  /*!
    \param[in,out] term_cost the input terminal cost that gets updated with 
    the total cost after integration.
    \return The number of integration steps required.
  */
  size_t cost::compute_cost( state_type& term_cost ) {
    using namespace std;
    using namespace boost::numeric::odeint;
    typedef runge_kutta_dopri5< state_type > stepper_type;
    
    double eps = 1E-7;
    size_t J1_steps = integrate_adaptive( make_controlled( 1E-5 , 1E-5 , 
							   stepper_type( ) ) , 
					  m_lofx , term_cost , t0_ , tf_-eps , 0.01 );
					  
    //size_t J1_steps = integrate_const( runge_kutta4< state_type >() , m_lofx , term_cost , t0_ , tf_-eps , 0.01 );

    //std::cout << "boost: " <<term_cost[0]<<"\n";
    return J1_steps;
  }
  



  /*!Alternative implementation of cost calculation based on trapezoidal rule.
    \param[in,out] term_cost the input terminal cost that gets updated with 
    the total cost after integration.
    \return The number of integration steps required.
  */
 /* size_t cost::compute_cost( state_type & term_cost ) {
	using namespace std;
	double integral;
	state_type * t_vec= m_x_intp.time_vec();//get reference to time vector from integration
	size_t length = t_vec->size();

	state_type l_of_x_vec (length); //preallocate this space (as large as the time vector from the integration))

	m_lofx(l_of_x_vec, t_vec);//calculates a vector of l_of_x values using the time and state vector from the integration

	integral = trapezoid( l_of_x_vec, *t_vec );//integration of incremental cost using trapezoidal rule
    
	term_cost[0] = term_cost[0] + integral;//final cost

	//std::cout << "trapezoidal: "<< term_cost[0]<<"\n";
    return length;
  }*/
  

}

#endif  // COST_HPP
