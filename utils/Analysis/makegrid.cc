#include <iostream>
#include <fstream>

#include <cstdlib>
#include <cmath>
#include <limits>

#include "config.h"

#ifdef HAVE_VTK

//
// STL stuff
//
#include <algorithm>
#include <vector>
#include <string>
#include <list>
#include <map>

//
// BOOST stuff
//
#include <boost/program_options.hpp>
#include <boost/random/mersenne_twister.hpp>

namespace po = boost::program_options;

//
// VTK stuff
//
#include <vtkSmartPointer.h>
#include <vtkFloatArray.h>
#include <vtkUnsignedCharArray.h>
#include <vtkStructuredPoints.h>
#include <vtkRectilinearGrid.h>
#include <vtkPoints.h>
#include <vtkPointData.h>
#include <vtkXMLRectilinearGridWriter.h>
#include <vtkLookupTable.h>
#include <vtkVersion.h>

//
// Helper class for diagnostics
//
class fRecord
{
public:
  int index;
  double min, max;
  
  fRecord()      : index(-1), min(std::numeric_limits<double>::max()), max(-std::numeric_limits<double>::max()) {}
  fRecord(int i) : index(i),  min(std::numeric_limits<double>::max()), max(-std::numeric_limits<double>::max()) {}
};

class fPosVel
{
  static const size_t nf = 6;
  static const char* names[];

  std::vector<double> vmin;
  std::vector<double> vmax;
  size_t indx, cnt;

public:

  fPosVel() {
    cnt  = 0;
    indx = -1;
    vmin = std::vector<double>(6,  std::numeric_limits<double>::max());
    vmax = std::vector<double>(6, -std::numeric_limits<double>::max());
  }

  void operator()(double* ps, double* vs)
  {
    for (size_t i=0; i<3; i++) {
      vmin[i]   = std::min<double>(vmin[i],   ps[i]);
      vmax[i]   = std::max<double>(vmax[i],   ps[i]);
      vmin[i+3] = std::min<double>(vmin[i+3], vs[i]);
      vmax[i+3] = std::max<double>(vmax[i+3], vs[i]);
    }
    cnt++;
  }

  void begin()      { indx = -1; }
  bool next()       { indx++; if (indx < 6) return true; else return false; }
  std::string lab() { return names[indx]; }
  double min()      { return vmin [indx]; }
  double max()      { return vmax [indx]; }
  size_t size()     { return cnt; }
};

const char* fPosVel::names[] = {"x", "y", "z", "u", "v", "z"};

//
// PSP stuff
//
#include <StringTok.H>
#include <header.H>
#include <PSP.H>

				// Globals for exputil library
				// Unused here
int myid = 0;
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
string outdir, runtag;
double tpos = 0.0;
double tnow = 0.0;
boost::mt19937 random_gen;


int main(int argc, char**argv)
{
  int numx=20, numy=20, numz=20, numr;
  double xmin=-1.0, xmax=1.0;
  double ymin=-1.0, ymax=1.0;
  double zmin=-1.0, zmax=1.0;
  double vscale = 1.0, rmin, rmax, zcut = -100.0;
  string infile("OUT.bin");
  string outfile("OUT");
  string cname, dname, sname;
  double time = 1.0;
  unsigned long initial_dark = 0, final_dark = std::numeric_limits<long>::max();
  unsigned long initial_star = 0, final_star = std::numeric_limits<long>::max();
  unsigned long initial_gas  = 0, final_gas  = std::numeric_limits<long>::max();

  bool COM      = false;
  bool mask     = false;
  bool verbose  = false;
  bool monopole = false;
  bool relative = false;
				// Boltzmann constant (cgs)
  const double boltz = 1.3810e-16;
				// Hydrogen fraction
  const double f_H = 0.76;
  				// Proton mass (g)
  const double mp = 1.67262158e-24;
				// Mean mass
  const double mm = f_H*mp + (1.0-f_H)*4.0*mp;
				// Cp/Cv; isentropic expansion factor
  const double gamma = 5.0/3.0;

  double Vconv = 120*1e5;

  // Construct help string
  //
  std::ostringstream sout;
  sout << std::string(60, '-') << std::endl
       << "This utility computes VTK volume files (rectangular grids)"
       << std::endl
       << "of density, velocity, and possible gas properties for"
       << std::endl
       << "specified components.  You can specify restricted ranges"
       << std::endl
       << "ranges of particle indices for each component"
       << std::endl
       << std::string(60, '-') << std::endl
       << std::endl << "Allowed options";

  po::options_description desc(sout.str());
  desc.add_options()
    ("help,h", "produce this help message")
    ("verbose,v", "verbose output")
    ("mask,b", "blank empty cells")
    ("monopole,M", "subtract tabulated monopole")
    ("relative,D", "density relative to tabulated monopole")
    ("OUT", "assume that PSP files are in original format")
    ("SPL", "assume that PSP files are in split format")
    ("COM,C", "use COM as origin")
    ("numx,1", po::value<int>(&numx)->default_value(20), 
     "number of bins in x direction")
    ("numy,2", po::value<int>(&numy)->default_value(20), 
     "number of bins in y direction")
    ("numz,3", po::value<int>(&numz)->default_value(20), 
     "number of bins in z direction")
    ("numr,0", po::value<int>(&numr),
     "number of bins in each coordinate direction")
    ("xmin,x", po::value<double>(&xmin)->default_value(-1.0), 
     "minimum x value")
    ("xmax,X", po::value<double>(&xmax)->default_value(1.0), 
     "maximum x value")
    ("ymin,y", po::value<double>(&ymin)->default_value(-1.0), 
     "minimum y value")
    ("ymax,Y", po::value<double>(&ymax)->default_value(1.0), 
     "maximum y value")
    ("zmin,z", po::value<double>(&zmin)->default_value(-1.0), 
     "minimum z value")
    ("zmax,Z", po::value<double>(&zmax)->default_value(1.0), 
     "maximum z value")
    ("rmin,r", po::value<double>(&rmin),
     "minimum coord value for all dimensions")
    ("rmax,R", po::value<double>(&rmax),
     "maximum coord value for all dimensions")
    ("vscale,V", po::value<double>(&vscale)->default_value(1.0), 
     "vertical scale factor")
    ("planecut,P", po::value<double>(&zcut)->default_value(-100.0), 
     "vertical plane cut")
    ("time,t", po::value<double>(&time)->default_value(0.0), 
     "desired PSP time")
    ("dark-name,d", po::value<string>(&dname),
     "PSP dark component name")
    ("star-name,s", po::value<string>(&sname),
     "PSP star component name")
    ("gas-name,g", po::value<string>(&cname),
     "PSP gas component name")
    ("input,i", po::value<string>(&infile)->default_value("OUT.bin"),
     "input file name")
    ("output,o", po::value<string>(&outfile)->default_value("OUT"),
     "output file ename")
    ("initial-gas", po::value<unsigned long>(&initial_gas)->default_value(0), 
     "initial gas particle index")
    ("final-gas", po::value<unsigned long>(&final_gas)->default_value(std::numeric_limits<long>::max()), 
     "initial gas particle index")
    ("initial-star", po::value<unsigned long>(&initial_star)->default_value(0), 
     "initial star particle index")
    ("final-star", po::value<unsigned long>(&final_star)->default_value(std::numeric_limits<long>::max()), 
     "initial star particle index")
    ("initial-dark", po::value<unsigned long>(&initial_dark)->default_value(0), 
     "initial dark particle index")
    ("final-dark", po::value<unsigned long>(&final_dark)->default_value(std::numeric_limits<long>::max()), 
     "initial dark particle index")
    ;
  
  po::variables_map vm;

  try {
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);    
  } catch (po::error& e) {
    std::cout << "Option error: " << e.what() << std::endl;
    exit(-1);
  }

  if (vm.count("help")) {
    cout << desc << "\n";
    return 1;
  }
 
  if (vm.count("verbose")) verbose = true;

  if (vm.count("mask"))     mask     = true;
  if (vm.count("monopole")) monopole = true;
  if (vm.count("relative")) relative = true;
  if (vm.count("COM"))      COM      = true;
  
  if (vm.count("rmin")) { xmin = ymin = zmin = rmin; }
  if (vm.count("rmax")) { xmax = ymax = zmax = rmax; }
  if (vm.count("numr")) { numx = numy = numz = numr; }


  std::ifstream in(infile);
  if (in) {
    cerr << "Error opening file <" << infile << "> for input\n";
    exit(-1);
  }

  if (verbose) cerr << "Using filename: " << infile << endl;

				// Parse the PSP file
				// ------------------
  PSPptr psp;
  if (vm.count("SPL")) psp = std::make_shared<PSPspl>(infile);
  else                 psp = std::make_shared<PSPout>(infile);

				// Now write a summary
				// -------------------
  if (verbose) {

    psp->PrintSummary(std::cerr);
    
    std::cerr << std::endl << "Best fit dump to <" << time << "> has time <" 
	      << psp->CurrentTime() << ">" << std::endl;
  }

				// Make the arrays
				// -----------------------------

  double dx = (xmax - xmin)/numx;
  double dy = (ymax - ymin)/numy;
  double dz = (zmax - zmin)/numz;

  cout << endl;
  cout << "Grid size:    [" << numx << ", " << numy << ", " << numz << "]"
       << endl;
  cout << "Grid bounds:  "
       << "[" << xmin << ", " << xmax << "] "
       << "[" << ymin << ", " << ymax << "] "
       << "[" << zmin << ", " << zmax << "] "
       << endl;
  cout << "Grid spacing: [" << dx << ", " << dy << ", " << dz << "]"
       << endl;
  if (zcut>0.0) cout << "Plane cut:    " << zcut << endl;       

  double smax = 0.0;

  smax = std::max<double>(smax, fabs(xmin));
  smax = std::max<double>(smax, fabs(xmax));

  smax = std::max<double>(smax, fabs(ymin));
  smax = std::max<double>(smax, fabs(ymax));

  smax = std::max<double>(smax, fabs(zmin));
  smax = std::max<double>(smax, fabs(zmax));

  smax *= 1.8;			// Round up of sqrt(3)

  int nums = 0;
  nums  = std::max<int>(nums, numx);
  nums  = std::max<int>(nums, numy);
  nums  = std::max<int>(nums, numz);
  nums *= 4;

  double dr = smax/nums;
  vector<float> dshell(nums, 0.0);

  float ftmp;
  vector<float> xyz(3), uvw(3);
  vector< vector< vector<float> > > mass(numx),  gdens(numx), gtemp(numx);
  vector< vector< vector<float> > > gknud(numx), gstrl(numx), gmach(numx);
  vector< vector< vector<float> > > sdens(numx), ddens(numx), gnumb(numx);
  vector< vector< vector< vector<double> > > > pos(numx);
  vector< vector< vector< vector<float > > > > vel(numx), veld(numx), vels(numx);
  
  for (int i=0; i<numx; i++) {
    
    mass [i] = vector< vector<float> >(numy);
    gtemp[i] = vector< vector<float> >(numy);
    gdens[i] = vector< vector<float> >(numy);
    gknud[i] = vector< vector<float> >(numy);
    gstrl[i] = vector< vector<float> >(numy);
    gmach[i] = vector< vector<float> >(numy);
    sdens[i] = vector< vector<float> >(numy);
    ddens[i] = vector< vector<float> >(numy);
    gnumb[i] = vector< vector<float> >(numy);

    pos  [i] = vector< vector< vector<double> > >(numy);
    vel  [i] = vector< vector< vector<float> > >(numy);
    veld [i] = vector< vector< vector<float> > >(numy);
    vels [i] = vector< vector< vector<float> > >(numy);
    
    for (int j=0; j<numy; j++) {
      
      mass[i][j]  = vector<float>(numz, 0.0);
      gtemp[i][j] = vector<float>(numz, 0.0);
      gdens[i][j] = vector<float>(numz, 0.0);
      gknud[i][j] = vector<float>(numz, 0.0);
      gstrl[i][j] = vector<float>(numz, 0.0);
      gmach[i][j] = vector<float>(numz, 0.0);
      sdens[i][j] = vector<float>(numz, 0.0);
      ddens[i][j] = vector<float>(numz, 0.0);
      gnumb[i][j] = vector<float>(numz, 0.0);

      pos [i][j]   = vector< vector<double> >(numz);
      vel [i][j]   = vector< vector<float > >(numz);
      veld[i][j]   = vector< vector<float > >(numz);
      vels[i][j]   = vector< vector<float > >(numz);

      for (int k=0; k<numz; k++) {
	pos [i][j][k] = vector<double>(3);
	vel [i][j][k] = vector<float> (3, 0.0);
	veld[i][j][k] = vector<float> (3, 0.0);
	vels[i][j][k] = vector<float> (3, 0.0);

	pos[i][j][k][0] = xmin + dx*(0.5 + i);
	pos[i][j][k][1] = ymin + dy*(0.5 + j);
	pos[i][j][k][2] = zmin + dz*(0.5 + k);
	pos[i][j][k][2] *= vscale;
      }
    }
  }
				// Reopen file to get component
				// -----------------------------
  double ms, ps[3], vs[3];
  double com[3] = {0.0, 0.0, 0.0};
  size_t indx;

  bool found_gas  = false;
  bool found_star = false;
  bool found_dark = false;

  bool btemp = false, bdens = false, bknud = false, bstrl = false;

  map<string, fPosVel> posvel;
  map<string, fRecord> fields;

  fields["Temp"] = fRecord(0);
  fields["Dens"] = fRecord(1);
  fields["Knud"] = fRecord(4);
  fields["Strl"] = fRecord(5);

  vtkSmartPointer<vtkPoints>    part  = vtkPoints    ::New();
  vtkSmartPointer<vtkDataArray> dens  = vtkFloatArray::New();
  vtkSmartPointer<vtkDataArray> temp  = vtkFloatArray::New();
  vtkSmartPointer<vtkDataArray> knud  = vtkFloatArray::New();
  vtkSmartPointer<vtkDataArray> strl  = vtkFloatArray::New();
  vtkSmartPointer<vtkDataArray> mach  = vtkFloatArray::New();
  vtkSmartPointer<vtkDataArray> velo  = vtkFloatArray::New();
  
  dens->SetName("density");
  temp->SetName("temperature");
  knud->SetName("Knudsen");
  strl->SetName("Strouhal");
  mach->SetName("Mach");

  velo->SetName("velocity");
  velo->SetNumberOfComponents(3);

  int offset = 0;
  
  PSPstanza* its;
  SParticle* prt;

  //
  // Compute COM for the entire phase space
  //
  if (COM) {

    double tot_mass = 0.0;

    for (its=psp->GetStanza(); its!=0; its=psp->NextStanza()) {

      indx = 0;
      for (prt=psp->GetParticle(); prt!=0; prt=psp->NextParticle()) {
	
	if (its->index_size) indx = prt->indx();
	else                 indx++;
	
	ms = prt->mass();
	for (int i=0; i<3; i++) com[i] += ms*prt->pos(i);
	tot_mass += ms;
      }
    }
    
    if (tot_mass>0.0) {
      for (int i=0; i<3; i++) com[i] /= tot_mass;
    }

    cout << "COM:          [" << com[0] << ", "
	 << com[1] << ", " << com[2] << "]" << std::endl;
  }
  

  for (its=psp->GetStanza(); its!=0; its=psp->NextStanza()) {

    if (dname.compare(its->name) == 0) {

      found_dark = true;

      if (posvel.find("dark") == posvel.end()) posvel["dark"] = fPosVel();

      indx = 0;
      for (prt=psp->GetParticle(); prt!=0; prt=psp->NextParticle()) {

	if (its->index_size) indx = prt->indx();
	else                 indx++;

	ms = prt->mass();
	for (int i=0; i<3; i++) ps[i] = prt->pos(i) - com[i];
	for (int i=0; i<3; i++) vs[i] = prt->vel(i);
	if (verbose) posvel["dark"](ps, vs);

				// Accumulate
				// 
	if (indx > initial_dark && indx <= final_dark) {
	  if (ps[0] >= xmin && ps[0] < xmax       &&
	      ps[1] >= ymin && ps[1] < ymax       &&
	      ps[2] >= zmin && ps[2] < zmax       &&
	      (ps[2] > zcut or ps[2] < -zcut) ) {
	    
	    int ii = (ps[0] - xmin)/dx;
	    int jj = (ps[1] - ymin)/dy;
	    int kk = (ps[2] - zmin)/dz;
	    
	    ddens[ii][jj][kk] += ms;
	    for (int i=0; i<3; i++) veld[ii][jj][kk][i] += ms*vs[i];
	  }

	  double rr = sqrt(ps[0]*ps[0] + ps[1]*ps[1] + ps[2]*ps[2]);
	  if (rr < smax) {
	    int uu = floor(rr/dr);
	    dshell[uu] += ms;
	  }
	}
      }

    } else if (sname.compare(its->name) == 0) {

      found_star = true;

      if (posvel.find("star") == posvel.end()) posvel["star"] = fPosVel();

      
      indx = 0;
      for (prt=psp->GetParticle(); prt!=0; prt=psp->NextParticle()) {

	if (its->index_size) indx = prt->indx();
	else                 indx++;

	ms = prt->mass();
	for (int i=0; i<3; i++) ps[i] = prt->pos(i) - com[i];
	for (int i=0; i<3; i++) vs[i] = prt->vel(i);
	if (verbose) posvel["star"](ps, vs);

	  
				// Accumulate
				// 
	if (indx > initial_star && indx <= final_star &&
	    ps[0] >= xmin && ps[0] < xmax       &&
	    ps[1] >= ymin && ps[1] < ymax       &&
	    ps[2] >= zmin && ps[2] < zmax       ) {
	  
	  int ii = (ps[0] - xmin)/dx;
	  int jj = (ps[1] - ymin)/dy;
	  int kk = (ps[2] - zmin)/dz;
	  
	  sdens[ii][jj][kk] += ms;
	  for (int i=0; i<3; i++) vels[ii][jj][kk][i] += ms*vs[i];
	}
      }

      } else if (cname.compare(its->name) == 0) {

      found_gas = true;

      if (posvel.find("gas") == posvel.end()) posvel["gas"] = fPosVel();

      indx = 0;
      for (prt=psp->GetParticle(); prt!=0; prt=psp->NextParticle()) {

	if (its->index_size) indx = prt->indx();
	else                 indx++;

	ms = prt->mass();
	for (int i=0; i<3; i++) ps[i]  = prt->pos(i) - com[i];
	for (int i=0; i<3; i++) xyz[i] = prt->pos(i) - com[i];
	for (int i=0; i<3; i++) vs[i]  = prt->vel(i);
	for (int i=0; i<3; i++) uvw[i] = prt->vel(i);
      
				// Coordinate limits
				// 
	if (verbose) posvel["gas"](ps, vs);

				// Accumulate
				// 
	if (indx > initial_gas && indx <= final_gas &&
	    ps[0] >= xmin && ps[0] < xmax       &&
	    ps[1] >= ymin && ps[1] < ymax       &&
	    ps[2] >= zmin && ps[2] < zmax       ) {
	  
	  int ii = (ps[0] - xmin)/dx;
	  int jj = (ps[1] - ymin)/dy;
	  int kk = (ps[2] - zmin)/dz;
	  
	  mass [ii][jj][kk] += ms;
	  gnumb[ii][jj][kk] += 1.0;
	  if (its->comp.ndatr>0) 
	    { gtemp[ii][jj][kk] += ms*prt->datr(0); btemp=true; }
	  if (its->comp.ndatr>1) 
	    { gdens[ii][jj][kk] += ms*prt->datr(1); bdens=true; }
	  if (its->comp.ndatr>4) 
	    { gknud[ii][jj][kk] += ms*prt->datr(4); bknud=true; }
	  if (its->comp.ndatr>5) 
	    { gstrl[ii][jj][kk] += ms*prt->datr(5); bstrl=true; }
	  for (int ll=0; ll<3; ll++) vel[ii][jj][kk][ll] += ms*vs[ll];
	  
	  // Get ranges
	  //
	  if (verbose) {
	    for (auto i : fields) {
	      string f = i.first;
	      int id   = i.second.index;
	      if (id>=0 && its->comp.ndatr>id) {
		fields[f].min = min<double>(prt->datr(id), fields[f].min);
		fields[f].max = max<double>(prt->datr(id), fields[f].max);
	      }
	    }
	  }

	  // Pack gas arrays
	  //
	  part->InsertPoint(offset, &xyz[0]);
	  if (btemp) temp->InsertTuple(offset, &(ftmp=prt->datr(0)));
	  if (bdens) dens->InsertTuple(offset, &(ftmp=prt->datr(1)));
	  if (bknud) knud->InsertTuple(offset, &(ftmp=prt->datr(4)));
	  if (bstrl) strl->InsertTuple(offset, &(ftmp=prt->datr(5)));
	  velo->InsertTuple(offset, &uvw[0]);
	  offset++;
	}
      }
    }
  }

  if (!found_dark && dname.size() > 0) {
    cerr << "Could not find dark component named <" << dname << ">\n";
    exit(-1);
  }

  if (!found_star && sname.size() > 0) {
    cerr << "Could not find star component named <" << sname << ">\n";
    exit(-1);
  }

  if (!found_gas && cname.size() > 0) {
    cerr << "Could not find gas component named <" << cname << ">\n";
    exit(-1);
  }

  if (found_dark) {
    for (int k=0; k<nums; k++) 
      dshell[k] /= 4.0*M_PI/3.0*(std::pow(dr*(k+1), 3.0) - std::pow(dr*(k+0), 3.0));
  }

  for (int i=0; i<numx; i++) {
    for (int j=0; j<numy; j++) {
      for (int k=0; k<numz; k++) {
	if (found_gas && mass[i][j][k]>0.0) {
	  for (int l=0; l<3; l++) vel[i][j][k][l] /= mass[i][j][k];
	  if (btemp) gtemp[i][j][k] /= mass[i][j][k];
	  if (bdens) gdens[i][j][k] /= mass[i][j][k];
	  if (bknud) {
	    if (isinf(gknud[i][j][k])) gknud[i][j][k] = 100.0;
	    else gknud[i][j][k] /= mass[i][j][k];
	  }
	  if (bstrl) gstrl[i][j][k] /= mass[i][j][k];
	  if (btemp) {
	    double vt = 0.0;
	    for (int l=0; l<3; l++) vt += vel[i][j][k][l]*vel[i][j][k][l];
	    gmach[i][j][k] = sqrt(vt*Vconv*Vconv /
				  (gamma*boltz/mm*gtemp[i][j][k]));
	  }
	  mass [i][j][k] /= dx*dy*dz;
	}
	if (found_dark) {
	  if (ddens[i][j][k]>0.0)
	    for (int s=0; s<3; s++) veld[i][j][k][s] /= ddens[i][j][k];
	  ddens[i][j][k] /= dx*dy*dz;

	  double xx = xmin + dx*(0.5 + i);
	  double yy = ymin + dy*(0.5 + j);
	  double zz = zmin + dz*(0.5 + k);
	  double rr = std::sqrt(xx*xx + yy*yy + zz*zz);

	  if (monopole and rr < smax)
	    ddens[i][j][k] -= dshell[std::floor(rr/dr)];

	  if (relative and rr < smax) {
	    if (dshell[std::floor(rr/dr)] > 0.0)
	      ddens[i][j][k] /= dshell[std::floor(rr/dr)];
	    else
	      ddens[i][j][k]  = 0.0;
	  }
	}
	if (found_star) {
	  if (sdens[i][j][k]>0.0)
	    for (int s=0; s<3; s++) vels[i][j][k][s] /= sdens[i][j][k];
	  sdens[i][j][k] /= dx*dy*dz;
	}
      }
    }
  }

  
  vtkSmartPointer<vtkFloatArray> XX = vtkFloatArray::New();
  vtkSmartPointer<vtkFloatArray> YY = vtkFloatArray::New();
  vtkSmartPointer<vtkFloatArray> ZZ = vtkFloatArray::New();
  float f;

  for (int i=0; i<numx; i++)  XX->InsertTuple(i, &(f=xmin + dx*(0.5 + i)));
  for (int j=0; j<numy; j++)  YY->InsertTuple(j, &(f=ymin + dy*(0.5 + j)));
  for (int k=0; k<numz; k++)  ZZ->InsertTuple(k, &(f=zmin + dz*(0.5 + k)));
    
  vtkSmartPointer<vtkRectilinearGrid> dataSet = vtkRectilinearGrid::New();
  dataSet->SetDimensions(numx, numy, numz);
  dataSet->SetXCoordinates (XX);
  dataSet->SetYCoordinates (YY);
  dataSet->SetZCoordinates (ZZ);

  vtkSmartPointer<vtkDataArray> Numb;
  vtkSmartPointer<vtkDataArray> Temp;
  vtkSmartPointer<vtkDataArray> Dens;
  vtkSmartPointer<vtkDataArray> Knud;
  vtkSmartPointer<vtkDataArray> Strl;
  vtkSmartPointer<vtkDataArray> Mach;
  vtkSmartPointer<vtkDataArray> density;
  vtkSmartPointer<vtkDataArray> velocity;
  vtkSmartPointer<vtkPoints>    points;
  vtkSmartPointer<vtkDataArray> dRho, dVel;
  vtkSmartPointer<vtkDataArray> sRho, sVel;


  if (found_gas) {
    Numb       = vtkFloatArray::New();
    Temp       = vtkFloatArray::New();
    Dens       = vtkFloatArray::New();
    Knud       = vtkFloatArray::New();
    Strl       = vtkFloatArray::New();
    Mach       = vtkFloatArray::New();
    density    = vtkFloatArray::New();
    velocity   = vtkFloatArray::New();
    points     = vtkPoints::New();

    Temp      -> SetName("Gas temp");
    Dens      -> SetName("Gas dens");
    Knud      -> SetName("Knudsen");
    Strl      -> SetName("Strouhal");
    Mach      -> SetName("Mach");
    Numb      -> SetName("Count");
    density   -> SetName("density");
    velocity  -> SetName("velocity");
    velocity  -> SetNumberOfComponents(3);
  }

  if (found_dark) {
    dRho = vtkFloatArray::New();
    dRho->SetName("Dark density");
    dVel = vtkFloatArray::New();
    dVel->SetName("Dark velocity");
    dVel->SetNumberOfComponents(3);
  }

  if (found_star) {
    sRho = vtkFloatArray::New();
    sRho->SetName("Star density");
    sVel = vtkFloatArray::New();
    sVel->SetName("Star velocity");
    sVel->SetNumberOfComponents(3);
  }

  vtkSmartPointer<vtkUnsignedCharArray> visible = vtkUnsignedCharArray::New();

  unsigned activ = 0;
  unsigned blank = 0;

  for (int k=0; k<numz; k++) {

    for (int j=0; j<numy; j++) {

      for (int i=0; i<numx; i++) {

	double x0 = min<double>(
				max<double>(pos[i][j][k][0], xmin+0.501*dx),
				xmax - 0.501*dx
				);

	double y0 = min<double>(
				max<double>(pos[i][j][k][1], ymin+0.501*dy),
				ymax - 0.501*dy
				);

	double z0 = min<double>(
				max<double>(pos[i][j][k][2], zmin+0.501*dz),
				zmax - 0.501*dz
				);

	vtkIdType n = dataSet->FindPoint(x0, y0, z0);

	if (found_gas) {

	  density->InsertTuple(n, &mass[i][j][k]);
	
	  velocity->InsertTuple(n, &vel[i][j][k][0]);
	
	  Numb->InsertTuple(n, &gnumb[i][j][k]);

	  if (btemp) Temp->InsertTuple(n, &gtemp[i][j][k]);
	  if (bdens) Dens->InsertTuple(n, &gdens[i][j][k]);
	  if (bknud) Knud->InsertTuple(n, &gknud[i][j][k]);
	  if (bstrl) Strl->InsertTuple(n, &gstrl[i][j][k]);
	  if (btemp) Mach->InsertTuple(n, &gmach[i][j][k]);

	  if (mass[i][j][k]>0.0 || !mask) {
	    visible->InsertValue(n, 1);
	    activ++;
	  }
	  else {
	    visible->InsertValue(n, 0);
	    blank++;
	  }
	}

	if (found_dark) {
	  dRho->InsertTuple(n, &ddens[i][j][k]);
	  dVel->InsertTuple(n, &veld[i][j][k][0]);
	}
	
	if (found_star) {
	  sRho->InsertTuple(n, &sdens[i][j][k]);
	  sVel->InsertTuple(n, &vels[i][j][k][0]);
	}

	offset++;
      }
    }
  }

  // Add all the fields

  if (found_gas) {
    if (btemp) dataSet->GetPointData()->AddArray(Temp);
    if (bdens) dataSet->GetPointData()->AddArray(Dens);
    if (bknud) dataSet->GetPointData()->AddArray(Knud);
    if (bstrl) dataSet->GetPointData()->AddArray(Strl);
    if (btemp) dataSet->GetPointData()->AddArray(Mach);
    dataSet->GetPointData()->AddArray(Numb);
    dataSet->GetPointData()->AddArray(density);
    dataSet->GetPointData()->SetVectors(velocity);
    // dataSet->SetPointVisibilityArray(visible);
  }
    
  if (found_dark) {
    dataSet->GetPointData()->AddArray(dRho);
    dataSet->GetPointData()->SetVectors(dVel);
  }
  
  if (found_star) {
    dataSet->GetPointData()->AddArray(sRho);
    dataSet->GetPointData()->SetVectors(sVel);
  }
  
  dataSet->SetDimensions(numx, numy, numz);

  // Print out the VTK file (in XML)

  string gname = outfile + ".vtr";
  vtkSmartPointer<vtkXMLRectilinearGridWriter> writer = 
    vtkXMLRectilinearGridWriter::New();
#if VTK_MAJOR_VERSION >= 6
    writer->SetInputData(dataSet);
#else
    writer->SetInput(dataSet);
#endif
    writer->SetFileName(gname.c_str());
    writer->Write();

  if (mask)
    cout << blank << " blank voxels and " << activ << " active ones" << endl;

  if (verbose) {
    cout << endl
	 << setw(42) << left << setfill('-') << '-' << endl << setfill(' ')
	 << setw(10) << left << "Field"
	 << setw(15) << left << "Minimum"
	 << setw(15) << left << "Maximum"
	 << endl
      	 << setw(42) << left << setfill('-') << '-' << endl << setfill(' ');

    for (auto i : posvel) {
      i.second.begin();
      while (i.second.next()) {
	ostringstream lab;
	lab << i.second.lab() << "(" << i.first << ")";
	cout << setw(10) << left << lab.str()
	     << setw(15) << left << i.second.min()
	     << setw(15) << left << i.second.max()
	     << endl;
      }
      cout << setw(42) << left << setfill('-') << '-' << endl << setfill(' ');
    }

    if (fields.size()) {
      for (auto i : fields)
	cout << setw(8)  << left << i.first
	     << setw(15) << left << i.second.min
	     << setw(15) << left << i.second.max
	     << endl;

      cout << setw(42) << left << setfill('-') << '-' << endl << setfill(' ');
    }
  }

  return (0);
}

#else

// Necessary globals for linking
//
std::string outdir, runtag;
char threading_on = 0;
int myid = 0;
pthread_mutex_t mem_lock;

int main()
{
  std::cout << "You need to have VTK installed to use this tool"
	    << std::endl;

  return (-1);
}


#endif
