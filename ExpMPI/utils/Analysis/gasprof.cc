/*****************************************************************************
 *  Description:
 *  -----------
 *
 *  Read in PSP files for a run and compute
 *  cylindrical gas distribution
 *
 *
 *  Call sequence:
 *  -------------
 *
 *  Parameters:
 *  ----------
 *
 *
 *  Returns:
 *  -------
 *
 *
 *  Notes:
 *  -----
 *
 *
 *  By:
 *  --
 *
 *  MDW 11/28/08
 *
 ***************************************************************************/

				// C++/STL headers
#include <cstdlib>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <cmath>
#include <string>

using namespace std;

                                // System libs
#include <sys/time.h>
#include <sys/resource.h>

				// MDW classes
#include <Vector.h>
#include <numerical.h>
#include <Particle.h>
#include <PSP.H>
#include <interp.h>
#include <massmodel.h>
#include <SphereSL.H>

#include <localmpi.h>
#include <ProgramParam.H>
#include <foarray.H>

program_option init[] = {
  {"RMIN",		"double",	"0.0",		"minimum radius for output"},
  {"RMAX",		"double",	"0.1",		"maximum radius for output"},
  {"ZCENTER",		"double",	"0.0",		"gas disk midplane"},
  {"ZWIDTH",		"double",	"0.05",		"gas disk halfwidth"},
  {"NBINS",		"int",		"0",		"number of bins"},
  {"IBEG",		"int",		"0",		"first PSP index"},
  {"IEND",		"int",		"100",		"last PSP index"},
  {"ISKIP",		"int",		"1",		"skip PSP interval"},
  {"PBEG",		"int",		"0",		"first particle index"},
  {"PEND",		"int",		"-1",		"last particle index"},
  {"LOG",		"bool",		"false",	"use logarithmic scaling for radial axis"},
  {"OUTFILE",		"string",	"gasprof",	"filename prefix"},
  {"INFILE",		"string",	"OUT",		"phase space file"},
  {"RUNTAG",		"string",	"run",		"file containing desired indices for PSP output"},
  {"",			"",		"",		""}
};


const char desc[] = "Compute disk potential, force and density profiles from PSP phase-space output files\n";


ProgramParam config(desc, init);

				// Variables not used but needed for linking
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
  
int
main(int argc, char **argv)
{
  
#ifdef DEBUG
  sleep(20);
#endif  
  
  // ==================================================
  // Parse command line or input parameter file
  // ==================================================
  
  if (config.parse_args(argc,argv)) return -1;
  
  
  // ==================================================
  // MPI preliminaries
  // ==================================================

  local_init_mpi(argc, argv);
  
  // ==================================================
  // Do round robin grid assignment of nodes
  // ==================================================

  ofstream indx;
  ifstream in;

  vector<string> files;
				// Root nodes looks for existence of files
  if (myid==0) {
    for (int i=config.get<int>("IBEG"); i<=config.get<int>("IEND"); i++) {
      ostringstream lab;
      lab << config.get<string>("INFILE") << "." 
	  << config.get<string>("RUNTAG") << "." 
	  << setw(5) << right << setfill('0') << i;
      ifstream in(lab.str().c_str());
      if (in) files.push_back(lab.str());
      else break;
      cout << "." << i << flush;
    }
    cout << endl;
  }

  unsigned nfiles = files.size();
  MPI_Bcast(&nfiles, 1, MPI_UNSIGNED, 0, MPI_COMM_WORLD);
  for (unsigned n=0; n<nfiles; n++) {
    unsigned sz;
    if (myid==0) sz = files[n].size();
    MPI_Bcast(&sz, 1, MPI_UNSIGNED, 0, MPI_COMM_WORLD);
    if (myid==0) {
      char *c = const_cast<char*>(files[n].c_str());
      MPI_Bcast(c, sz+1, MPI_CHAR, 0, MPI_COMM_WORLD);
    } else {
      char l[sz+1];
      MPI_Bcast(&l[0], sz+1, MPI_CHAR, 0, MPI_COMM_WORLD);
      files.push_back(l);
    }
    MPI_Barrier(MPI_COMM_WORLD);
  }

  double rmin = config.get<double>("RMIN");
  double rmax = config.get<double>("RMAX");
  int   nbins = config.get<int>   ("NBINS");
  bool   rlog = config.get<bool>  ("LOG");
  double zcen = config.get<double>("ZCENTER");
  double zwid = config.get<double>("ZWIDTH");
  int    pbeg = config.get<int>   ("PBEG");
  int    pend = config.get<int>   ("PEND");

  if (rmin>0 && rmax > 0 && rlog) {
    rmin = log(rmin);
    rmax = log(rmax);
  }

  double dR = (rmax - rmin)/(nbins-1);

  vector< vector<double> > histo(nfiles);
  for (int n=0; n<nfiles; n++) histo[n] = vector<double>(nbins, 0.0);

  vector<double> times(nfiles);
  vector<double> rvals(nbins);

  for (int n=0; n<nbins; n++) {
    rvals[n] = rmin + dR*n;
    if (rlog) rvals[n] = exp(rvals[n]);
  }

  for (int n=0; n<nfiles; n++) {

    if (n % numprocs == myid) {

      ifstream in(files[n].c_str());
      PSPDump psp(&in, true);

      Dump *dump = psp.GetDump();
      
      if (dump) {

	times[n] = psp.CurrentTime();

	// Do we need to close and reopen?
	if (in.rdstate() & ios::eofbit) {
	  in.close();
	  in.open(files[n].c_str());
	}

	int icnt = 0;
	vector<Particle> particles;

	PSPstanza *gas = psp.GetGas();
	SParticle *p = psp.GetParticle(&in);
	
	while (p) {

	  if (icnt > pbeg) {
	    if (p->pos[2] >= zcen-zwid && p->pos[2] <= zcen+zwid) {
	      double R = sqrt(p->pos[0]*p->pos[0] + p->pos[1]*p->pos[1]);
	      if (rlog) {
		if (R>0.0) {
		  R = log(R);
		  int indx = static_cast<int>(floor( (R - rmin)/dR ));
		  if (indx >=0 && indx<nbins) histo[n][indx] += p->mass;
		}
	      } else {
		int indx = static_cast<int>(floor( (R - rmin)/dR ));
		if (indx >=0 && indx<nbins) histo[n][indx] += p->mass;
	      }
	    }
	  }
	    
	  if (pend>0 && icnt>pend) break;
	  p = psp.NextParticle(&in);
	  icnt++;
	}
      }
    }
  }
  
  if (myid==0) {
    MPI_Reduce(MPI_IN_PLACE, &times[0], nfiles,
	       MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    for (int n=0; n<nfiles; n++)
      MPI_Reduce(MPI_IN_PLACE, &histo[n][0], nbins, 
		 MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
  } else {
    MPI_Reduce(&times[0], 0, nfiles,
	       MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    for (int n=0; n<nfiles; n++)
      MPI_Reduce(&histo[n][0], 0, nbins, 
		 MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
  }

  if (myid==0) {
    string outf = config.get<string>("OUTFILE") + ".dat";
    ofstream out(outf.c_str());

    for (int n=0; n<nfiles; n++) {
      double sum = 0.0;
      for (unsigned j=0; j<nbins; j++)
	out << setw(18) << times[n] << setw(18) << rvals[j]
	    << setw(18) << histo[n][j]
	    << setw(18) << (sum += histo[n][j])
	    << endl;
      out << endl;
    }
    
  }

 

  MPI_Finalize();

  return 0;
}

