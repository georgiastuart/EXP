// #define DEBUG

/*
  Generates a Monte Carlo realization of a halo with an embedded
  disk using Eddington inversion and quadratic programming with a
  Gaussian DF basis in E and J/J_max.

  Assumptions:

  1) Spherical halo supplied in an input table

  2) Axisymmetric (but 3-dimensional) exponential disk with sech^2(Z/z)
     vertical profile

  3) Halo as spherical velocity ellipsoid

  4) Disk DF is determined using quadratic programming

  10/03/10 MDW

*/
                                // System libs
#include <unistd.h>
#include <getopt.h>
#include <values.h>

                                // C++/STL headers
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

                                // MDW classes
#include <numerical.h>
#include <gaussQ.h>
#include <isothermal.h>
#include <hernquist.h>
#include <model3d.h>
#include <biorth.h>
#include <SphericalSL.h>
#include <interp.h>
#include <EmpCylSL.h>

#include <boost/program_options.hpp>

namespace po = boost::program_options;

#include <norminv.H>

#define M_SQRT1_3 (0.5773502691896257645091487)

                                // For debugging
#ifdef DEBUG
#include <fpetrap.h>
#endif

                                // Local headers
#include "SphericalSL.h"
#include "DiskHalo3.h" 
#include "localmpi.h"

//
// Global variables
//
int          LMAX;
int          NMAX;
int          NUMR;
double       RMIN;
double       RCYLMIN;
double       RCYLMAX;
double       SCSPH;
double       RSPHSL;
double       ASCALE;
double       ASHIFT;
double       HSCALE;
double       DMFAC;
double       X0;
double       Y0;
double       Z0;
double       U0;
double       V0;
double       W0;
int          RNUM;
int          PNUM;
int          TNUM;
int          VFLAG;
int          DFLAG;
bool         expcond;
bool         CONSTANT;
bool         GAUSSIAN;
bool         PLUMMER;
bool         CMAP;
bool         LOGR;
bool         CHEBY;
int          NDR;
int          NDZ;
int          NHR;
int          NHT;
double       SHFAC;
int          NMAX2;
int          LMAX2;
int          MMAX;
int          NUMX;
int          NUMY;
int          NORDER;
int          DIVERGE;
double       DIVERGE_RFAC;
int          DIVERGE2;
double       DIVERGE_RFAC2;
int          DF;
double       R_DF;
double       DR_DF;
double       scale_height;
double       scale_length;
double       scale_lenfkN;
double       disk_mass;
double       gas_mass;
double       gscal_length;
double       ToomreQ;
double       Temp;
double       Tmin;
bool         const_height;
bool         images;
bool         multi;
int          SEED;
bool         DENS;
bool         basis;
bool         zero;
int          nhalo;
int          ndisk;
int          ngas;
int          ngparam;
string       hbods;
string       dbods;
string       gbods;
string       suffix;
string       centerfile;
string       halofile1;
string       halofile2;

// Hydrogen fraction
//
const double f_H = 0.76;


// Global variables

#include <Particle.H>

int VERBOSE = 4;
int nthrds = 1;
int this_step = 0;
unsigned multistep = 0;
unsigned maxlev = 100;
int mstep = 1;
int Mstep = 1;
vector<int> stepL(1, 0), stepN(1, 1);
char threading_on = 0;
pthread_mutex_t mem_lock;
pthread_mutex_t coef_lock;
double tpos = 0.0;
double tnow = 0.0;
string outdir, runtag;
  
double DiskDens(double R, double z, double phi)
{
  double ans = 0.0;

  if (CONSTANT) {
      
    if (R < ASCALE && fabs(z) < HSCALE)
      ans = 1.0/(2.0*HSCALE*M_PI*ASCALE*ASCALE);
  }
  else if (GAUSSIAN) {
      
    if (fabs(z) < HSCALE)
      ans = 1.0/(2.0*HSCALE*2.0*M_PI*ASCALE*ASCALE)*
	exp(-R*R/(2.0*ASCALE*ASCALE));
    
  } else {			// EXPONENTIAL

    double f = cosh(z/HSCALE);
    ans = exp(-R/ASCALE)/(4.0*M_PI*ASCALE*ASCALE*HSCALE*f*f);
    
  }

  return ans;
}

double dcond(double R, double z, double phi, int M)
{
  //
  // No shift for M==0
  //
  if (M==0) return DiskDens(R, z, phi);

  //
  // Fold into [-PI/M, PI/M] for M>=1
  //
  double dmult = M_PI/M, phiS;
  if (phi>M_PI)
    phiS = phi + dmult*(int)((2.0*M_PI - phi)/dmult);
  else
    phiS = phi - dmult*(int)(phi/dmult);
  
  //
  // Apply a shift along the x-axis
  //
  double x = R*cos(phiS) - ASHIFT*ASCALE;
  double y = R*sin(phiS);
  return DiskDens(sqrt(x*x + y*y), z, atan2(y, x));
}

int 
main(int argc, char **argv)
{
  //====================
  // Inialize MPI stuff
  //====================
  local_init_mpi(argc, argv);
  
  //====================
  // Parse command line 
  //====================

  po::options_description desc("Generates a Monte Carlo realization of a halo\nwith an embedded disk using Jeans\' equations.\nAllowed options");
  desc.add_options()
    ("help,h",                                                                          "Print this help message")
    ("LMAX",                po::value<int>(&LMAX)->default_value(4),
     "Number of harmonics for halo expansion")
    ("NMAX",                po::value<int>(&NMAX)->default_value(10),
     "Number of radial basis functions for halo expansion")
    ("NUMR",                po::value<int>(&NUMR)->default_value(2000),
     "Size of radial grid for Spherical SL")
    ("RMIN",                po::value<double>(&RMIN)->default_value(0.005),
     "Minimum halo radius")
    ("RCYLMIN",             po::value<double>(&RCYLMIN)->default_value(0.001),
     "Minimum disk radius")
    ("RCYLMAX",             po::value<double>(&RCYLMAX)->default_value(20.0),
     "Maximum disk radius")
    ("SCSPH",               po::value<double>(&SCSPH)->default_value(1.0),
     "Scale for Spherical SL coordinate mapping")
    ("RSPHSL",              po::value<double>(&RSPHSL)->default_value(47.5),
     "Maximum halo expansion radius")
    ("ASCALE",              po::value<double>(&ASCALE)->default_value(1.0),
     "Radial scale length for disk basis construction")
    ("ASHIFT",              po::value<double>(&ASHIFT)->default_value(0.0),
     "Fraction of scale length for shift in conditioning function")
    ("HSCALE",              po::value<double>(&HSCALE)->default_value(0.1),
     "Vertical scale length for disk basis construction")
    ("DMFAC",               po::value<double>(&DMFAC)->default_value(1.0),
     "Disk mass scaling factor for spherical deprojection model")
    ("X0",                  po::value<double>(&X0)->default_value(0.0),
     "Disk-Halo x center position")
    ("Y0",                  po::value<double>(&Y0)->default_value(0.0),
     "Disk-Halo y center position")
    ("Z0",                  po::value<double>(&Z0)->default_value(0.0),
     "Disk-Halo z center position")
    ("U0",                  po::value<double>(&U0)->default_value(0.0),
     "Disk-Halo x velocity center position")
    ("V0",                  po::value<double>(&V0)->default_value(0.0),
     "Disk-Halo y velocity center position")
    ("W0",                  po::value<double>(&W0)->default_value(0.0),
     "Disk-Halo z velocity center position")
    ("RNUM",                po::value<int>(&RNUM)->default_value(200),
     "Number of radial knots for EmpCylSL basis construction quadrature")
    ("PNUM",                po::value<int>(&PNUM)->default_value(80),
     "Number of azimthal knots for EmpCylSL basis construction quadrature")
    ("TNUM",                po::value<int>(&TNUM)->default_value(80),
     "Number of cos(theta) knots for EmpCylSL basis construction quadrature")
    ("CMAP",                po::value<bool>(&CMAP)->default_value(false),
     "Map coordinates from radius to tabled grid")
    ("LOGR",                po::value<bool>(&LOGR)->default_value(false),
     "Make a logarithmic coordinate mapping")
    ("CHEBY",               po::value<bool>(&CHEBY)->default_value(false),
     "Use Chebyshev smoothing for epicyclic and asymmetric drift")
    ("NDR",                 po::value<int>(&NDR)->default_value(1600),
     "Number of points in DiskHalo radial table for disk")
    ("NDZ",                 po::value<int>(&NDZ)->default_value(400),
     "Number of points in DiskHalo vertical table for disk")
    ("NHR",                 po::value<int>(&NHR)->default_value(1600),
     "Number of points in DiskHalo radial table for halo")
    ("NHT",                 po::value<int>(&NHT)->default_value(200),
     "Number of points in DiskHalo cos(theta) table for halo")
    ("SHFAC",               po::value<double>(&SHFAC)->default_value(16.0),
     "Scale height factor for assigning vertical table size")
    ("NMAX2",               po::value<int>(&NMAX2)->default_value(36),
     "Number of radial basis functions in Spherical SL for determining disk basis")
    ("LMAX2",               po::value<int>(&LMAX2)->default_value(36),
     "Number of harmonics for Spherial SL for determining disk basis")
    ("MMAX",                po::value<int>(&MMAX)->default_value(4),
     "Number of azimuthal harmonics for disk basis")
    ("NUMX",                po::value<int>(&NUMX)->default_value(256),
     "Radial grid size for disk basis table")
    ("NUMY",                po::value<int>(&NUMY)->default_value(128),
     "Vertical grid size for disk basis table")
    ("NORDER",              po::value<int>(&NORDER)->default_value(16),
     "Number of disk basis functions per M-order")
    ("DIVERGE",             po::value<int>(&DIVERGE)->default_value(0),
     "Cusp extrapolation for primary halo model")
    ("DIVERGE_RFAC",        po::value<double>(&DIVERGE_RFAC)->default_value(1.0),
     "Extrapolation exponent for primary mass model")
    ("DIVERGE2",            po::value<int>(&DIVERGE2)->default_value(0),
     "Cusp extrapolation for number model")
    ("DIVERGE_RFAC2",       po::value<double>(&DIVERGE_RFAC2)->default_value(1.0),
     "Extrapolation exponent for number model")
    ("DF",                  po::value<int>(&DF)->default_value(0),
     "Use change-over from Jeans to Eddington")
    ("R_DF",                po::value<double>(&R_DF)->default_value(20.0),
     "Change over radius for Eddington")
    ("DR_DF",               po::value<double>(&DR_DF)->default_value(5.0),
     "Width of change for to Eddington")
    ("scale_height",        po::value<double>(&scale_height)->default_value(0.1),
     "Scale length for disk realization")
    ("scale_length",        po::value<double>(&scale_length)->default_value(2.0),
     "Scale height for disk realization")
    ("scale_lenfkN",        po::value<double>(&scale_lenfkN)->default_value(-1.0),
     "Scale for multimass gas")
    ("disk_mass",           po::value<double>(&disk_mass)->default_value(1.0),
     "Mass of stellar adisk")
    ("gas_mass",            po::value<double>(&gas_mass)->default_value(1.0),
     "Mass of gaseous disk")
    ("gscal_length",        po::value<double>(&gscal_length)->default_value(4.0),
     "Gas disk scale length")
    ("ToomreQ",             po::value<double>(&ToomreQ)->default_value(1.2),
     "Toomre Q parameter for stellar disk generation")
    ("Temp",                po::value<double>(&Temp)->default_value(2000.0),
     "Gas temperature (in K)")
    ("Tmin",                po::value<double>(&Tmin)->default_value(500.0),
     "Temperature floor (in K) for gas disk generation")
    ("const_height",        po::value<bool>(&const_height)->default_value(true),
     "Use constant disk scale height")
    ("images",              po::value<bool>(&images)->default_value(false),
     "Print out reconstructed disk profiles")
    ("multi",               po::value<bool>(&multi)->default_value(false),
     "Use multimass halo")
    ("SEED",                po::value<int>(&SEED)->default_value(11),
     "Random number seed")
    ("DENS",                po::value<bool>(&DENS)->default_value(true),
     "Compute the density basis functions")
    ("basis",               po::value<bool>(&basis)->default_value(false),
     "Print out disk and halo basis")
    ("zero",                po::value<bool>(&zero)->default_value(false),
     "zero center of mass and velocity")
    ("nhalo",               po::value<int>(&nhalo)->default_value(1000),
     "Number of halo particles")
    ("ndisk",               po::value<int>(&ndisk)->default_value(1000),
     "Number of disk particles")
    ("ngas",                po::value<int>(&ngas)->default_value(1000),
     "Number of gas particles")
    ("ngparam",             po::value<int>(&ngparam)->default_value(3),
     "Number of gas particle parameters")
    ("hbods",               po::value<std::string>(&hbods)->default_value("halo.bods"),
     "Halo particle output file")
    ("dbods",               po::value<std::string>(&dbods)->default_value("disk.bods"),
     "Disk particle output file")
    ("gbods",               po::value<std::string>(&gbods)->default_value("gas.bods"),
     "Gas particle output file")
    ("suffix",              po::value<std::string>(&suffix)->default_value(".bod"),
     "Suffix appended for body files")
    ("VFLAG",               po::value<int>(&VFLAG)->default_value(0),
     "Output flags for EmpCylSL")
    ("DFLAG",               po::value<int>(&DFLAG)->default_value(0),
     "Output flags for DiskHalo")
    ("expcond",             po::value<bool>(&expcond)->default_value(true),
     "Use analytic density function for computing EmpCylSL basis")
    ("CONSTANT",            po::value<bool>(&CONSTANT)->default_value(false),
     "Check basis with a constant density")
    ("GAUSSIAN",            po::value<bool>(&GAUSSIAN)->default_value(false),
     "Use Gaussian disk profile rather than exponential disk profile")
    ("PLUMMER",             po::value<bool>(&PLUMMER)->default_value(false),
     "Use Plummer disk profile rather than exponential disk profile")
    ("centerfile",          po::value<std::string>(&centerfile)->default_value("center.dat"),
     "Read position and velocity center from this file")
    ("halofile1",           po::value<std::string>(&halofile1)->default_value("SLGridSph.model"),
     "File with input halo model")
    ("halofile2",           po::value<std::string>(&halofile2)->default_value("SLGridSph.model.fake"),
     "File with input halo model for multimass")
    ;

  po::variables_map vm;
  
  // Parse command line for control and critical parameters
  //
  try {
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);    
  } catch (po::error& e) {
    if (myid==0) std::cout << "Option error on command line: "
			   << e.what() << std::endl;
    MPI_Finalize();
    return -1;
  }
  
  // Print help message and exit
  //
  if (vm.count("help")) {
    if (myid == 0) {
      std::cout << desc << std::endl << std::endl;
    }
    MPI_Finalize();
    return 1;
  }

#ifdef DEBUG                    // For gdb . . . 
  sleep(20);
  set_fpu_handler();            // Make gdb trap FPU exceptions
#endif

  int n_particlesH, n_particlesD, n_particlesG;

  if (suffix.size()>0) {
    hbods = hbods + "." + suffix;
    dbods = dbods + "." + suffix;
    gbods = gbods + "." + suffix;
  }

                                // Divvy up the particles
  n_particlesH = nhalo/numprocs;
  if (myid==0) n_particlesH = nhalo - n_particlesH*(numprocs-1);
  
  n_particlesD = ndisk/numprocs;
  if (myid==0) n_particlesD = ndisk - n_particlesD*(numprocs-1);

  n_particlesG = ngas/numprocs;
  if (myid==0) n_particlesG = ngas  - n_particlesG*(numprocs-1);


#ifdef DEBUG  
  cout << "Processor " << myid << ": n_particlesH=" << n_particlesH << endl;
  cout << "Processor " << myid << ": n_particlesD=" << n_particlesD << endl;
  cout << "Processor " << myid << ": n_particlesG=" << n_particlesG << endl;
#endif

  if (n_particlesH + n_particlesD + n_particlesG <= 0) {
    if (myid==0) cout << "You have specified zero particles!" << endl;
    MPI_Abort(MPI_COMM_WORLD, 3);
    exit(0);
  }

                                // Vectors to contain phase space
                                // Particle structure is defined in
                                // Particle.H
  vector<Particle> dparticles, hparticles;

				//
                                // Disk halo grid parameters
				//
  DiskHalo::RDMIN       = RCYLMIN*scale_length;
  DiskHalo::RHMIN       = RMIN;
  DiskHalo::RHMAX       = RSPHSL;
  DiskHalo::RDMAX       = RCYLMAX*scale_length;
  DiskHalo::NDR         = NDR;
  DiskHalo::NDZ         = NDZ;
  DiskHalo::NHR         = NHR;
  DiskHalo::NHT         = NHT;
  DiskHalo::SHFACTOR    = SHFAC;
  DiskHalo::COMPRESSION = DMFAC;
  DiskHalo::LOGSCALE    = 1;
  DiskHalo::NUMDF       = 4000;
  DiskHalo::Q           = ToomreQ;
  DiskHalo::R_DF        = R_DF;
  DiskHalo::DR_DF       = DR_DF;
  DiskHalo::SEED        = SEED;
  DiskHalo::VFLAG       = static_cast<unsigned int>(DFLAG);
  DiskHalo::CHEBY       = CHEBY;
  if (suffix.size())
      DiskHalo::RUNTAG  = suffix;

  AddDisk::use_mpi      = true;
  AddDisk::Rmin         = RMIN;

  //===========================Spherical expansion=============================

  // SLGridSph::diverge = DIVERGE;
  // SLGridSph::divergexp = DIVERGE_RFAC;
    
  SphericalSL::RMIN = RMIN;
  SphericalSL::RMAX = RSPHSL;
  SphericalSL::NUMR = NUMR;
                                // Create expansion only if needed . . .
  SphericalSL *expandh = NULL;
  if (n_particlesH) {
    expandh = new SphericalSL(LMAX, NMAX, SCSPH);
#ifdef DEBUG
    string dumpname("debug");
    expandh->dump_basis(dumpname);
#endif
  }

  //===========================Cylindrical expansion===========================


  EmpCylSL::RMIN        = RCYLMIN;
  EmpCylSL::RMAX        = RCYLMAX;
  EmpCylSL::NUMX        = NUMX;
  EmpCylSL::NUMY        = NUMY;
  EmpCylSL::NUMR        = NUMR;
  EmpCylSL::CMAP        = CMAP;
  EmpCylSL::VFLAG       = VFLAG;
  EmpCylSL::logarithmic = LOGR;
  EmpCylSL::DENS        = DENS;

  if (basis) EmpCylSL::DENS = true;

                                // Create expansion only if needed . . .
  EmpCylSL* expandd = NULL;
  if (n_particlesD) {
    expandd = new EmpCylSL(NMAX2, LMAX2, MMAX, NORDER, ASCALE, HSCALE);
#ifdef DEBUG
    cout << "Process " << myid << ": "
	 << " rmin=" << EmpCylSL::RMIN
	 << " rmax=" << EmpCylSL::RMAX
	 << " a=" << ASCALE
	 << " h=" << HSCALE
	 << " nmax2=" << NMAX2
	 << " lmax2=" << LMAX2
	 << " mmax=" << MMAX
	 << " nordz=" << NORDER
	 << endl << flush;
#endif

    if (expandd->read_cache() == 0) {
      if (expcond)
	expandd->generate_eof(RNUM, PNUM, TNUM, dcond);
    }

  }


  //====================Create the disk & halo model===========================

  DiskHalo *diskhalo;

  if (multi) {
    if (myid==0) cout << "Initializing a MULTIMASS halo . . . " << flush;
    diskhalo = new DiskHalo (expandh, expandd,
			     scale_height, scale_length, disk_mass, 
			     halofile1, DIVERGE,  DIVERGE_RFAC,
			     halofile2, DIVERGE2, DIVERGE_RFAC2);
    if (myid==0) cout << "done" << endl;

  } else {

    if (myid==0) cout << "Initializing a SINGLE halo . . . " << flush;
    diskhalo = new DiskHalo (expandh, expandd,
			     scale_height, scale_length, 
			     disk_mass, halofile1,
			     DF, DIVERGE, DIVERGE_RFAC);
    if (myid==0) cout << "done" << endl;
  }
  
  ifstream center(centerfile.c_str());
  if (center) {

    bool ok = true;

    center >> X0;
    if (center.fail()) ok = false;

    center >> Y0;
    if (center.fail()) ok = false;

    center >> Z0;
    if (center.fail()) ok = false;

    if (ok) {
      diskhalo->set_pos_origin(X0, Y0, Z0);
      if (myid==0) cout << "Using position origin: " 
			<< X0 << ", " << Y0 << ", " << Z0 << endl;
    }

    center >> U0;
    if (center.fail()) ok = false;

    center >> V0;
    if (center.fail()) ok = false;

    center >> W0;
    if (center.fail()) ok = false;

    if (ok) {
      diskhalo->set_vel_origin(U0, V0, W0);
      if (myid==0) cout << "Using velocity origin: " 
			<< U0 << ", " << V0 << ", " << W0 << endl;
    }
  }

                                // Make zero center of mass and
                                // center of velocity
  diskhalo->zero_com(zero);
  diskhalo->zero_cov(zero);
  
  //===========================================================================

                                // Open output file (make sure it exists
                                // before realizing a large phase space)
  ofstream out_halo, out_disk;
  if (myid==0) {
    out_halo.open(hbods.c_str());
    if (!out_halo) {
      cout << "Could not open <" << hbods << "> for output" << endl;
      MPI_Abort(MPI_COMM_WORLD, 4);
      exit(0);
    }

    out_disk.open(dbods.c_str());
    if (!out_disk) {
      cout << "Could not open <" << dbods << "> for output" << endl;
      MPI_Abort(MPI_COMM_WORLD, 4);
      exit(0);
    }
  }

  //=================Make the phase space coordinates==========================

  if (n_particlesH) {
    if (multi) {
      if (myid==0) cout << "Generating halo phase space . . . " << flush;
      diskhalo->set_halo(hparticles, nhalo, n_particlesH);
    } else {
      if (myid==0) cout << "Generating halo coordinates . . . " << flush;
      diskhalo->set_halo_coordinates(hparticles, nhalo, n_particlesH);
      MPI_Barrier(MPI_COMM_WORLD);
    }
    MPI_Barrier(MPI_COMM_WORLD);
    if (myid==0) cout << "done" << endl;
  }

  if (n_particlesH) {
    if (myid==0) cout << "Beginning halo accumulation . . . " << flush;
    expandh->accumulate(hparticles);
    MPI_Barrier(MPI_COMM_WORLD);
    if (myid==0) cout << "done" << endl;
  }
  
  if (n_particlesD) {
    if (myid==0) cout << "Generating disk distribution function . . . " << flush;
    diskhalo->make_disk_DF(true);
    if (myid==0) cout << "done" << endl;
    if (myid==0) cout << "Generating disk phase space . . . " << flush;
    diskhalo->set_disk(dparticles, ndisk, n_particlesD);
    MPI_Barrier(MPI_COMM_WORLD);
    if (myid==0) cout << "done" << endl;
    if (myid==0) cout << "Beginning disk accumulation . . . " << flush;
    if (!expcond) {
      expandd->setup_eof();
      expandd->setup_accumulation();
      expandd->accumulate_eof(dparticles);
      MPI_Barrier(MPI_COMM_WORLD);

      if (myid==0) cout << "done" << endl;
  
      if (myid==0) cout << "Making the EOF . . . " << flush;
      expandd->make_eof();
      MPI_Barrier(MPI_COMM_WORLD);
      if (myid==0) cout << "done" << endl;
    }
  
    if (myid==0) cout << "Making disk coefficients . . . " << flush;
    expandd->make_coefficients();
    MPI_Barrier(MPI_COMM_WORLD);
    if (myid==0) cout << "done" << endl;

    if (myid==0) cout << "Reexpand . . . " << flush;
    expandd->accumulate(dparticles);
    expandd->make_coefficients();
    MPI_Barrier(MPI_COMM_WORLD);
    if (myid==0) cout << "done" << endl;

    if (images && myid==0) {
      cout << "Images . . . " << flush;
      ostringstream dumpname;
      dumpname << "images.0";
      expandd->dump_images(dumpname.str(), 5.0*ASCALE, 5.0*HSCALE, 64, 64, true);
      cout << "done" << endl;
    }
  }
  

  //===========================Diagnostics=====================================

                                // For examining the coverage, etc.
                                // Images can be contoured in SM using
                                // the "ch" file type
  if (myid==0 && basis) {
    
    cout << "Dumping basis images . . . " << flush;
    
    if (n_particlesD) {
      int nout = 200;
      char dumpname[] = "basis.dump";
      expandd->dump_basis(dumpname, 0);
      string prefix = "gendisk3";
      expandd->dump_images(prefix, 5.0*scale_length, 5.0*scale_height,
			   nout, nout, false);
      expandd->dump_images_basis(prefix, 5.0*scale_length, 5.0*scale_height,
				 nout, nout, false, 0, MMAX, 0, NORDER-1);
    }


    if (n_particlesH) {
      string extn("test");
      expandh->dump_basis(extn);
    }
    
    if (n_particlesH) {
      
      const int nstr = 5;
      const char *names[nstr] = {".dens", ".potl", ".potr", ".pott", ".potp"};
      ofstream *out = new ofstream [nstr];
      
      int nout = 200;
      double rmax = 6.0*scale_length;
      double x, y, dr = 2.0*rmax/(nout-1);
      float f;
    
      for (int i=0; i<nstr; i++) {
        string name("halo");
        name += names[i];
        out[i].open(name.c_str());
        
        out[i].write((char *)&nout, sizeof(int));
        out[i].write((char *)&nout, sizeof(int));
        out[i].write((char *)&(f=-rmax), sizeof(float));
        out[i].write((char *)&(f= rmax), sizeof(float));
        out[i].write((char *)&(f=-rmax), sizeof(float));
        out[i].write((char *)&(f= rmax), sizeof(float));
      }
      
      double r, theta, phi;
      double dens, potl, potr, pott, potp;
    
      for (int j=0; j<nout; j++) {
        y = -rmax + dr*j;
      
        for (int i=0; i<nout; i++) {
          x = -rmax + dr*i;
        
          r = sqrt(x*x + y*y);
          theta = 0.5*M_PI;
          phi = atan2(y, x);
        
          expandh->determine_fields_at_point(r, theta, phi,
                                             &dens, &potl, 
                                             &potr, &pott, &potp);
        
          out[0].write((char *)&(f=dens), sizeof(float));
          out[1].write((char *)&(f=potl), sizeof(float));
          out[2].write((char *)&(f=potr), sizeof(float));
          out[3].write((char *)&(f=pott), sizeof(float));
          out[4].write((char *)&(f=potp), sizeof(float));
        }
        
      }
    
      for (int i=0; i<nstr; i++) out[i].close();
      delete [] out;
    }

    if (n_particlesD) {

      const int nstr = 5;
      const char *names[nstr] = {".dens", ".pot", ".fr", ".fz", ".fp"};
      ofstream *out = new ofstream [nstr];
    
      int nout = 200;
      double rmax = DiskHalo::RDMAX;
      double x, y, dr = 2.0*rmax/(nout-1);
      float f;
    
      for (int i=0; i<nstr; i++) {
        string name("disk");
        name += names[i];
        out[i].open(name.c_str());
        
        out[i].write((char *)&nout, sizeof(int));
        out[i].write((char *)&nout, sizeof(int));
        out[i].write((char *)&(f=-rmax), sizeof(float));
        out[i].write((char *)&(f= rmax), sizeof(float));
        out[i].write((char *)&(f=-rmax), sizeof(float));
        out[i].write((char *)&(f= rmax), sizeof(float));
      }
    
      double z = 0.0, d0, p0, d, p, fr, fz, fp;
    
      for (int j=0; j<nout; j++) {
        y = -rmax + dr*j;
      
        for (int i=0; i<nout; i++) {
          x = -rmax + dr*i;
        
	  if (x<0.0)
	    expandd->accumulated_eval(fabs(x), y, M_PI, p0, p, fr, fz, fp);
	  else
	    expandd->accumulated_eval(x, y,  0.0, p0, p, fr, fz, fp);


          d = expandd->accumulated_dens_eval(sqrt(x*x + y*y), z, atan2(y, x), d0);
        
          
          out[0].write((char *)&(f=d ), sizeof(float));
          out[1].write((char *)&(f=p ), sizeof(float));
          out[2].write((char *)&(f=fr), sizeof(float));
          out[3].write((char *)&(f=fz), sizeof(float));
          out[4].write((char *)&(f=fp), sizeof(float));
        }
        
      }
    
      for (int i=0; i<nstr; i++) out[i].close();
      delete [] out;
    }
    
    cout << "done" << endl;
  }
  
  MPI_Barrier(MPI_COMM_WORLD);

  //====================Make the phase space velocities========================

  if (!multi) {
    if (myid==0) cout << "Generating halo velocities . . . " << flush;
    diskhalo->set_vel_halo(hparticles);
    if (myid==0) cout << "done" << endl;
  }
  
  //====================All done: write it out=================================

  if (myid==0) cout << "Writing phase space file . . . " << flush;

  diskhalo->write_file(out_halo, out_disk, hparticles, dparticles);
  if (myid==0) cout << "done" << endl;

  out_halo.close();
  out_disk.close();
                                // Diagnostic . . .
  diskhalo->virial_ratio(hparticles, dparticles);

  //====================Compute gas particles==================================

  if (myid==0 && n_particlesG) {
    cout << "Computing gas particles . . . " << endl;

				// UNITS
				// -------------------

				// cm
    const double pc = 3.08568025e18;
				// proton mass
    const double m_p = 1.67262158e-24;
				// g
    const double msun = 1.98892e33; //
				// cgs
    const double G = 6.67300e-08;
				// cgs
    const double boltz = 1.3806503e-16;

    double T = Temp;

    
    double Lunit = 3.0e5*pc;	// Virial radius
    double Munit = 1.0e12*msun;	// Virial mass
    double Tunit = sqrt(Lunit*Lunit*Lunit/(Munit*G));
    double Vunit = Lunit/Tunit;

    // Fac = kT*R_vir/(G*m_p*M_vir)
    // where M_vir = 1e12 Msun, R_vir=300 kpc
    //
    double fac = T/(G*m_p*Munit/(Lunit*boltz));

    // Thermal velocity in system units
    //
    double mm   = f_H*m_p + (1.0-f_H)*4.0*m_p;
    double vthermal = sqrt( (boltz*T)/mm ) / Vunit;
    double vmin2 = (boltz*Tmin/mm) / (Vunit*Vunit);

    // Adjust scale for multimass gas
    //
    double Scale_Length = gscal_length;
    if (scale_lenfkN > 0.0) gscal_length = scale_lenfkN;

    // Compute using Jeans theorem
    //
    double rmin = RMIN;
    double rmax = 10.0*gscal_length;
    double zmin = 0.001*scale_height;
    int nrint = 200;
    int nzint = 400;
    vector< vector<double> > zrho, zmas, vcir;
    double r, R, dR = (rmax - rmin)/(nrint-1);
    double z, dz = (log(rmax) - log(zmin))/(nzint-1);

    double p0, p, fr, fz, fp, dens, potl, potr, pott, potp;

    cout << "Const_height=" << (const_height ? "True" : "False") << endl;

    if (const_height) {

      for (int i=0; i<nrint; i++) {
	R = rmin + dR*i;

	vector<double> lrho(nzint), trho(nzint), tcir(nzint), tmas(nzint, 0);

	for (int j=0; j<nzint; j++) {
	  z = zmin*exp(dz*j);
	  r = sqrt(R*R + z*z);
	  
	  double pot=0.0, frt0=0.0, fzt0=0.0;
	  if (expandd) {
	    expandd->accumulated_eval(R, z, 0, p0, p, fr, fz, fp);
	    frt0 += -fr;
	    fzt0 += -fz;
	    pot += p0;
	  }
	  if (expandh) {
	    expandh->determine_fields_at_point(r, acos(z/(r+1.0e-8)), 0.0,
					       &dens, &potl, 
					       &potr, &pott, &potp);
	    
	    frt0 += potr;
	    fzt0 += (potr*z + pott*R*R/(r*r))/r;
	    pot += potl;
	  }
	  
	  trho[j] = fzt0*scale_height;
	  tcir[j] = sqrt(max<double>(R*frt0-R*trho[j]/Scale_Length, 0.0));
	}
	
	for (int j=0; j<nzint; j++) 
	  tmas[j] = 1.0 - exp(-zmin*exp(dz*j)/scale_height);
	
	zrho.push_back(trho);
	zmas.push_back(tmas);
	vcir.push_back(tcir);
      }

      //
      // Vertical table
      //
      string ztable("ztable.dat");
      cout << "Writing " << setw(15) << right << ztable
	   << " [gas] . . . " << flush;
      ofstream ztest(ztable.c_str());
      for (int i=0; i<nrint; i++) {
	for (int j=0; j<nzint; j++) {
	  ztest << setw(15) << rmin + dR*i
		<< setw(15) << zmin*exp(dz*j)
		<< setw(15) << zrho[i][j]
		<< setw(15) << zrho[i][j]*Vunit*Vunit*mm/boltz
		<< setw(15) << zmas[i][j]
		<< setw(15) << vcir[i][j]
		<< endl;
	}
	ztest << endl;
      }
      ztest.close();
      cout << "done" << endl;
      
    } else {

      for (int i=0; i<nrint; i++) {
	R = rmin + dR*i;



	vector<double> lrho(nzint), trho(nzint), tcir(nzint), tmas(nzint, 0);

	for (int j=0; j<nzint; j++) {
	  z = zmin*exp(dz*j);
	  r = sqrt(R*R + z*z);
	  
	  double frt0=0.0, fzt0=0.0;
	  if (expandd) {
	    expandd->accumulated_eval(R, z, 0, p0, p, fr, fz, fp);
	    frt0 += -fr;
	    fzt0 += -fz;
	  }
	  if (expandh) {
	    expandh->determine_fields_at_point(r, acos(z/(r+1.0e-8)), 0.0,
					       &dens, &potl, 
					       &potr, &pott, &potp);
	    frt0 += potr;
	    fzt0 += (potr*z + pott*R*R/(r*r))/r;
	  }
	  
	  trho[j] = -fzt0/(vthermal*vthermal);
	  tcir[j] = sqrt(max<double>(R*frt0-R*vthermal*vthermal/Scale_Length, 0.0));
	}
	
	double mass = 0.0;
	double zfac = 1.0 - exp(-dz);
				    
	lrho[0] = 0.0;
	for (int j=1; j<nzint; j++) 
	  lrho[j] = lrho[j-1] + 0.5*(trho[j-1] + trho[j]) * zmin*exp(dz*j)*zfac;
	
	for (int j=1; j<nzint; j++) 
	  tmas[j] = tmas[j-1] + 0.5*(exp(lrho[j-1]) + exp(lrho[j])) * zmin*exp(dz*j)*zfac;
	
	for (int j=0; j<nzint; j++) {
	  if (tmas[nzint-1]>0.0 && !std::isnan(tmas[nzint-1])) {
	    trho[j]  = exp(lrho[j])/tmas[nzint-1];
	    tmas[j] /= tmas[nzint-1];
	  } else {
	    trho[j] = 0.0;
	    if (j==0) tmas[j] = 0.0;
	    else      tmas[j] = 1.0;
	  }
	}
	zrho.push_back(trho);
	zmas.push_back(tmas);
	vcir.push_back(tcir);
      }


      //
      // Vertical table
      //
      cout << "Writing ztable.dat [gas] . . . " << flush;
      ofstream ztest("ztable.dat");
      for (int i=0; i<nrint; i++) {
	for (int j=0; j<nzint; j++) {
	  ztest << setw(15) << rmin + dR*i
		<< setw(15) << zmin*exp(dz*j)
		<< setw(15) << zrho[i][j]
		<< setw(15) << zmas[i][j]
		<< setw(15) << vcir[i][j]
		<< endl;
	}
	ztest << endl;
      }
      ztest.close();
      cout << "done" << endl;
      
    }

    // 
    // Prepare output stream
    //
    ofstream outps(gbods.c_str());
    if (!outps) {
      cerr << "Couldn't open <" << gbods << "> for output" << endl;
      exit (-1);
    }

    const int ITMAX=1000;
    const int NREPORT=1000;
    
    //
    // Maximum enclosed disk mass given rmax
    //
    double rmx2 = 1.5*rmax;
    double mmx2 = 1.0 - (1.0 + rmx2/gscal_length)*exp(-rmx2/gscal_length);
    double mmax = 1.0 - (1.0 + rmax/gscal_length)*exp(-rmax/gscal_length);
    double mfac = 1.0 - (1.0 + rmax/Scale_Length)*exp(-rmax/Scale_Length);

    //
    // Random generators
    //
    ACG gen(10, 20);
    Uniform unit(0.0, 1.0, &gen);

    //
    // Trimmed Gaussian
    //
    double minK=0.0, maxK=1.0, sigma = 3.0;
    if (sigma>0) {
      minK = 0.5*(1.0+erf(-0.5*sigma));
      maxK = 0.5*(1.0+erf( 0.5*sigma));
    }
    Uniform unitN(minK, maxK, &gen);


    double gmass, gmass0 = gas_mass/ngas;
    double KE=0.0, VC=0.0;
    vector<double> mc2(nzint);

    gmass = gmass0;
    fr = fz = potr = 0.0;

    outps << setw(8) << ngas
	  << setw(6) << 0 << setw(6) << ngparam << endl;

    for (int n=0; n<ngas; n++) {

      double F, dF, M=mmax*unit(), Z=unit();
      double R = M*rmax, phi=2.0*M_PI*unit(), x, y, z, rr, vc;
      double ax, ay, az;

				// Narrow with bisection
      double rm = 0.0, rp = rmx2;
      double fm = -M, fp = mmx2 - M;
      for (int j=0; j<15; j++) {
	R = 0.5*(rm + rp);
	F = 1.0 - M - (1.0 + R/gscal_length)*exp(-R/gscal_length);
	if (fm*F<0.0) {
	  rp = R;
	  fp = F;
	} else {
	  rm = R;
	  fm = F;
	}
      }
				// Polish with Newton-Raphson
      for (int j=0; j<ITMAX; j++) {
	F = 1.0 - M - (1.0 + R/gscal_length)*exp(-R/gscal_length);
	dF = R/(gscal_length*gscal_length)*exp(-R/gscal_length);
	R += -F/dF;
	if (fabs(F/dF)<1.0e-12) break;
      }
    
      int indr = static_cast<int>(floor(R/dR));
      if (indr<0) indr=0;
      if (indr>nrint-2) indr=nrint-2;
      double a = (dR*(indr+1) - R)/dR;
      double b = (R - indr*dR)/dR;

      vector<double> mz(nzint), vz(nzint);
      for (int j=0; j<nzint; j++) {
	mz[j] = a*zmas[indr][j] + b*zmas[indr+1][j];
	vz[j] = a*vcir[indr][j] + b*vcir[indr+1][j];
      }
      for (int j=0; j<nzint; j++) mz[j] /= mz[nzint-1];
      
      if (const_height) {
	for (int j=0; j<nzint; j++) 
	  mc2[j] = max<double>(a*zrho[indr][j] + b*zrho[indr+1][j], vmin2);
      }

      int indz = max<int>(0, min<int>(nzint-2, Vlocate(Z, mz)));

      a = (mz[indz+1] - Z)/(mz[indz+1] - mz[indz]);
      b = (Z - mz[indz  ])/(mz[indz+1] - mz[indz]);

      vc = fabs(a*vz[indr] + b*vz[indr+1]);

      z = zmin*exp(dz*(a*indz + b*(indz+1)));
      if (unit()<0.5) z *= -1.0;
      rr = sqrt(R*R + z*z);

      if (const_height) {
	vthermal = a*mc2[indz] + b*mc2[indz+1];
	vthermal = sqrt(max<double>(vmin2, vthermal));
      }

      double sinp = sin(phi), cosp = cos(phi);
      x = R*cosp;
      y = R*sinp;

      double u = -vc*sinp + vthermal*norminv(unitN());
      double v =  vc*cosp + vthermal*norminv(unitN());
      double w =  vthermal*norminv(unitN());
      
      gmass = gmass0*exp(-R*(1.0/Scale_Length - 1.0/gscal_length)) * 
	mmax*gscal_length*gscal_length/(mfac*Scale_Length*Scale_Length);

      outps << setw(18) << gmass
	    << setw(18) << R*cos(phi)
	    << setw(18) << R*sin(phi)
	    << setw(18) << z
	    << setw(18) << u
	    << setw(18) << v
	    << setw(18) << w;
      for (int k=0; k<ngparam; k++) outps << setw(18) << 0.0;
      outps << endl;
    
      if (expandd)
	expandd->accumulated_eval(R, z, phi, p0, p, fr, fz, fp);

      if (expandh)
	expandh->determine_fields_at_point(rr, acos(z/(rr+1.0e-8)), 0.0,
					   &dens, &potl, 
					   &potr, &pott, &potp);
      KE += 0.5*gmass*(u*u + v*v + w*w);

      VC += gmass*(-rr*potr + R*fr + z*fz);

      if (!((n+1)%NREPORT)) cout << "\r." << n+1 << flush;
    }

    cout << endl << "Done!" << endl;

    cout << "****************************" << endl
	 << "  Gas disk" << endl
	 << "----------------------------" << endl
	 << "  KE       = " << KE << endl
	 << "  VC       = " << VC << endl;
    if (VC<0.0)
      cout << " -2T/W     = " << -2.0*KE/VC << endl;
    cout << "****************************" << endl;
  }

  //===========================================================================

  MPI_Barrier(MPI_COMM_WORLD);

  delete expandh;
  delete expandd;

  MPI_Finalize();

  return 0;
}

