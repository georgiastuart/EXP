// This is -*- C++ -*-

#ifndef _massmodel_h
#define _massmodel_h

#include <vector>

#include <boost/shared_ptr.hpp>

#include <ACG.h>
#include <Uniform.h>
#include <Normal.h>
#include <Vector.h>
#include <orbit.h>

class QPDistF;

//! A one-dimensional profile
class RUN 
{
public:
  Vector x;
  Vector y;
  Vector y2;
  int num;
};

//! A one-dimensional Merritt-Osipkov distribution
class FDIST 
{
public:
  Vector Q;
  Vector fQ;
  Vector ffQ;
  Vector fQ2;
  Vector ffQ2;
  double ra2;
  double off;
  int num;
};

//! A three-dimensional density-potential model
class MassModel
{
public:

  //! The number of dimensions (degrees of freedom)
  int dim;
  //! Model identifier
  string ModelID;
  //! True if model is assigned
  bool defined;

  //! Destructor
  virtual ~MassModel() {}

  /** Get the mass inside of position (x, y, z) (this is ambiguous in
      general and will depend on the model) */
  virtual double get_mass(const double, const double, const double) = 0;
  //! The density at (x, y, z)
  virtual double get_density(const double, const double, const double) = 0;
  //! The potential at (x, y, z)
  virtual double get_pot(const double, const double, const double) = 0;

  //! Return the number of dimensions (degrees of freedom)
  int dof() { return dim; }

  //! Exception handling
  void bomb(const char *s) {
    cerr << "ERROR from " << ModelID << ": " << s << '\n';
    exit(-1);
  }

};
  

//! A specification of a MassModel to the axisymmetric case
class AxiSymModel : public MassModel
{
protected:
  //@{
  //! Stuff for gen_point
  ACG *gen;
  Uniform *Unit;
  Normal *Gauss;
  bool gen_firstime;
  bool gen_firstime_E;
  bool gen_firstime_jeans;
  Vector gen_rloc, gen_mass, gen_fmax;
  SphericalOrbit gen_orb;
  double gen_fomax, gen_ecut;
  //@}

  Vector gen_point_2d(int& ierr);
  Vector gen_point_2d(double r, int& ierr);
  Vector gen_point_3d(int& ierr);
  Vector gen_point_3d(double Emin, double Emax, double Kmin, double Kmax, int& ierr);
  Vector gen_point_jeans_3d(int& ierr);
  
  double Emin_grid, Emax_grid, dEgrid, dKgrid;
  vector<double> Egrid, Kgrid, EgridMass;
  vector<double> Jmax;
  
  class WRgrid
  {
  public:
    vector<double> w1;
    vector<double> r;
  };
  
  typedef vector<WRgrid> wrvector;
  vector<wrvector> Rgrid;
  
public:
  //@{
  //! Stuff for gen_point
  static bool gen_EJ;
  static int numr, numj;
  static int gen_N;
  static int gen_E;
  static int gen_K;
  static int gen_itmax;
  static int gen_logr;
  static double gen_rmin;
  static double gen_kmin;
  static double gen_tolE, gen_tolK;
  static unsigned int gen_seed;
  //@}
  
  //! The distribution is defined and computed
  bool dist_defined;
  
  //! Constructor
  AxiSymModel(void) { 
    ModelID = "AxiSymModel";
    gen_firstime = true;
    gen_firstime_E = true;
    gen_firstime_jeans = true;
    gen_ecut = 1.0;
  }
  
  //! Destructor
  virtual ~AxiSymModel() {}
  
  //! Set the random number seed
  void set_seed(int s)  { gen_seed = s;}

  //! Maximum number of iterations for obtaining a valid variate
  void set_itmax(int s) { gen_itmax = s;}
  
  //@{
  //! Access the profile
  virtual double get_mass(const double) = 0;
  virtual double get_density(const double) = 0;
  virtual double get_pot(const double) = 0;
  virtual double get_dpot(const double) = 0;
  virtual double get_dpot2(const double) = 0;
  virtual void get_pot_dpot(const double, double&, double&) = 0;
  
  double get_mass(const double x1, const double x2, const double x3)
  { return get_mass(sqrt(x1*x1 + x2*x2 + x3*x3)); }
  
  double get_density(const double x1, const double x2, const double x3)
  { return get_density(sqrt(x1*x1 + x2*x2 + x3*x3)); }
  
  double get_pot(const double x1, const double x2, const double x3)
  { return get_pot(sqrt(x1*x1 + x2*x2 + x3*x3)); }
  //@}
  
  //@{
  // Addiional member access functions
  virtual double get_min_radius(void) = 0;
  virtual double get_max_radius(void) = 0;
  virtual double distf(double, double) = 0;
  virtual double dfde(double, double) = 0;
  virtual double dfdl(double, double) = 0;
  virtual double d2fde2(double, double) = 0;
  //@}
  
  //! Set cutoff on multimass realization grid
  void set_Ecut(double cut) { gen_ecut = cut; }

  //! Generate a phase-space point
  virtual Vector gen_point(int& ierr) {
    if (dof()==2)
      return gen_point_2d(ierr);
    else if (dof()==3)
      return gen_point_3d(ierr);
    else
      bomb( "dof must be 2 or 3" );
    
    return Vector();
  }
  
  //! Generate a phase-space point using Jeans' equations
  virtual Vector gen_point_jeans(int& ierr) {
    if (dof()==2)
      bomb( "AxiSymModel: gen_point_jeans_2d(ierr) not implemented!" );
    else if (dof()==3)
      return gen_point_jeans_3d(ierr);
    else
      bomb( "dof must be 2 or 3" );
    
    return Vector();
  }
  
  //! Generate a phase-space point at a particular radius
  virtual Vector gen_point(double r, int& ierr) {
    if (dof()==2)
      return gen_point_2d(r, ierr);
    else if (dof()==3)
      bomb( "AxiSymModel: gen_point(r, ierr) not implemented!" );
    else
      bomb( "AxiSymModel: dof must be 2 or 3" );
    
    return Vector();
  }
  
  //! Generate a phase-space point at a particular energy and angular momentum
  virtual Vector gen_point(double Emin, double Emax, double Kmin, double Kmax, int& ierr) {
    if (dof()==2)
      bomb( "AxiSymModel: gen_point(r, ierr) not implemented!" );
    else if (dof()==3)
      return gen_point_3d(Emin, Emax, Kmin, Kmax, ierr);
    else
      bomb( "AxiSymModel: dof must be 2 or 3" );
    
    return Vector();
  }
  
  //! Generate a the velocity variate from a position
  virtual void gen_velocity(double *pos, double *vel, int& ierr);
  
};

class EmbeddedDiskModel : public AxiSymModel
{
private:
  AxiSymModel **t;
  double *m_scale;
  double *r_scale;
  int number;

  double rmin, rmax;

  QPDistF *df;

public:
  EmbeddedDiskModel(AxiSymModel **T, double *M_scale, double *R_scale, 
		      int NUMBER);

  virtual ~EmbeddedDiskModel();

  virtual double get_mass(const double);
  virtual double get_density(const double);
  virtual double get_pot(const double);
  virtual double get_dpot(const double);
  virtual double get_dpot2(const double);
  virtual void get_pot_dpot(const double, double&, double&);

  double get_mass(const double x1, const double x2, const double x3)
    { return get_mass(sqrt(x1*x1 + x2*x2 + x3*x3)); }

  double get_density(const double x1, const double x2, const double x3)
    { return get_density(sqrt(x1*x1 + x2*x2 + x3*x3)); }

  double get_pot(const double x1, const double x2, const double x3)
    { return get_pot(sqrt(x1*x1 + x2*x2 + x3*x3)); }

  // Additional member functions

  double get_min_radius(void) { return rmin; }
  double get_max_radius(void) { return rmax; }

  void setup_df(int egrid=10, int kgrid=5, int mgrid=20,
		double lambda=0.0, double alpha=-6.0, double beta=1.0,
		double gama=1.0, double sigma=2.0, double rmmax=-1, 
		double roff=0.05, double eoff=0.5, double koff=0.5, 
		double kmin=0.0, double kmax=1.0,
		int nint=20, int numt=20);

  // Read in from state file

  void setup_df(string& file);

  void verbose_df(void);
  double distf(double E, double L);
  double dfde(double E, double L);
  double dfdl(double E, double L);
  double d2fde2(double E, double L);
  void save_df(string& file);

};


/** Describe a spherical model from a four-column table of radius,
    density, enclosed mass, and gravitational potential

    The file may have any number of leading comment lines, any line
    that starts with a "!" or a "#".  The first non-comment record is
    assumed to be an integer containing the number of lines.  The
    subseuqnet lines are assumed to be records beginning the radius
    and followed by the triple of (density, enclosed mass,
    gravitational potential).  
*/
class SphericalModelTable : public AxiSymModel
{
private:
  RUN mass;
  RUN density;
  RUN pot;
  struct FDIST df;
  int num;
  int numdf;
  int num_params;
  double *params;
  double diverge_rfac;
  int diverge;
  int external;

public:

  //! Count the number of instantiations (for debugging)
  static int count;		

  //! Assume even spacing in the mass model table (default: yes)
  static int even;		

  //! Log scale in df computation (default: yes)
  static int logscale;		

  //! Linear interpolation in model (default: no)
  static int linear;		

  //! Constructor (from a table)
  SphericalModelTable(string filename, 
		 int DIVERGE = 0, double DIVERGE_RFAC = 1.0, int EXTERNAL = 0);

  //! Constructor (from array pointers)
  SphericalModelTable(int num, 
		 double *r, double *d, double *m, double *p,
	         int DIVERGE = 0, double DIVERGE_RFAC = 0, int EXTERNAL = 0,
					   string ID = "" );

  //! Destructor
  virtual ~SphericalModelTable();

  //@{
  //! Evaluate the profile at a particular radius
  virtual double get_mass(const double) override;
  virtual double get_density(const double) override;
  virtual double get_pot(const double) override;
  virtual double get_dpot(const double) override;
  virtual double get_dpot2(const double) override;
  virtual void   get_pot_dpot(const double, double&, double&) override;
  //@}
  
  //@{
  //! Get info about the model
  int get_num_param(void) { return num_params; }
  double get_param(int i) { return params[i-1]; }

  double get_min_radius(void) { return mass.x[1]; }
  double get_max_radius(void) { return mass.x[mass.num]; }
  int grid_size(void) { return num; }
  void print_model(char const *name);
  //@}

  //@{
  //! Set up and get info about the phase-space distribution
  void setup_df(int NUM, double RA=1.0e20);
  void print_df(char const *name);
  double get_ra2(void) { return df.ra2; }
  //@}

  //@{
  //! Evaluate the distribution function
  double distf(double E, double L);
  double dfde(double E, double L);
  double dfdl(double E, double L);
  double d2fde2(double E, double L);
  //@}
};


/** 
    This uses two SphericalModelTables to define a mass profile and a
    number profile for generating a distribution with variable mass
    per particle.
*/
class SphericalModelMulti : public AxiSymModel
{
protected:
  AxiSymModel* real;
  AxiSymModel* fake;

  SphericalOrbit orb, gen_orb;

  double rmin_gen, rmax_gen;

public:

  //! Constructor
  SphericalModelMulti(AxiSymModel* Real, AxiSymModel* Fake);

  //@{
  //! Evaluate the profile at a radial point
  virtual double get_mass(const double r) override
  { return real->get_mass(r); }

  virtual double get_density(const double r) override
  { return real->get_density(r); }
  
  virtual double get_pot(const double r) override
  { return real->get_pot(r); }

  virtual double get_dpot(const double r)  override
  { return real->get_dpot(r); }

  virtual double get_dpot2(const double r)  override
  { return real->get_dpot2(r); }

  virtual void get_pot_dpot(const double r, double& p, double& dp) override
  { real->get_pot_dpot(r, p, dp); }
  //@}
  
  //@{
  //! Additional member functions
  double get_min_radius(void) { return real->get_min_radius(); }
  double get_max_radius(void) { return real->get_max_radius(); }
  //@}

  //@{
  //! Evaluate distribution function and its partial derivatives
  double distf(double E, double L)  { return real->distf(E, L);  }
  double dfde (double E, double L)  { return real->dfde(E, L);   }
  double dfdl (double E, double L)  { return real->dfdl(E, L);   }
  double d2fde2(double E, double L) { return real->d2fde2(E, L); }
  //@}

  //@{
  //! Overloaded to provide mass distribution from Real and Number distribution from Fake
  Vector gen_point(int& ierr);
  Vector gen_point(double r, int& ierr);
  Vector gen_point(double Emin, double Emax, double Kmin, double Kmax, int& ierr);
  //@}


  //@{
  //! Set new minimum and maximum for realization
  void set_min_radius(const double& r) { rmin_gen = r; }
  void set_max_radius(const double& r) { rmax_gen = r; }
  //@}
};

typedef boost::shared_ptr<AxiSymModel>         AxiSymModPtr;
typedef boost::shared_ptr<EmbeddedDiskModel>   EmbDiskModPtr;
typedef boost::shared_ptr<SphericalModelMulti> SphModMultPtr;
typedef boost::shared_ptr<SphericalModelTable> SphModTblPtr;

#include <QPDistF.h>

#endif
