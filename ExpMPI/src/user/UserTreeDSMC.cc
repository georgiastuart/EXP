#include <sys/timeb.h>
#include <stdlib.h>
#include <sys/types.h>
#include <getopt.h>
#include <time.h>

#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>

#include <expand.h>
#include <ExternalCollection.H>
#include <UserTreeDSMC.H>
#include <CollideLTE.H>

#ifdef USE_GPTL
#include <gptl.h>
#endif

#include <BarrierWrapper.H>

using namespace std;

//
// Debugging syncrhonization
//
static bool barrier_debug = true;

//
// Physical units
//

static double pc = 3.086e18;		// cm
static double a0 = 2.0*0.054e-7;	// cm (2xBohr radius)
static double boltz = 1.381e-16;	// cgs
// static double year = 365.25*24*3600;	// seconds
static double mp = 1.67e-24;		// g
static double msun = 1.989e33;		// g

double UserTreeDSMC::Lunit = 3.0e5*pc;
double UserTreeDSMC::Munit = 1.0e12*msun;
double UserTreeDSMC::Tunit = sqrt(Lunit*Lunit*Lunit/(Munit*6.673e-08));
double UserTreeDSMC::Vunit = Lunit/Tunit;
double UserTreeDSMC::Eunit = Munit*Vunit*Vunit;
bool   UserTreeDSMC::use_effort = true;

UserTreeDSMC::UserTreeDSMC(string& line) : ExternalForce(line)
{
  id = "TreeDSMC";		// ID string

				// Default parameter values
  ncell      = 7;		// 
  Ncell      = 64;
  cnum       = 0;
  madj       = 512;		// No tree pruning by default
  wght       = 1;		// Cell time partitioning by default
  epsm       = -1.0;
  diamfac    = 1.0;
  boxsize    = 1.0;
  boxratio   = 1.0;
  jitter     = 0.0;
  comp_name  = "gas disk";
  nsteps     = -1;
  msteps     = -1;
  use_temp   = -1;
  use_dens   = -1;
  use_delt   = -1;
  use_Kn     = -1;
  use_St     = -1;
  use_vol    = -1;
  use_exes   = -1;
  coolfrac   = 0.1;
  remap      = 0;
  frontier   = false;
  tsdiag     = false;
  voldiag    = false;
  tspow      = 4;
  mfpstat    = false;
  cbadiag    = false;
  dryrun     = false;
  nocool     = false;
  use_multi  = false;
  use_pullin = false;
  esol       = false;
  ntc        = true;
  cba        = true;
  tube       = false;
  slab       = false;
  sub_sample = true;
  treechk    = false;
  mpichk     = false;
				// Initialize using input parameters
  initialize();

				// Look for the fiducial component
  bool found = false;
  list<Component*>::iterator cc;
  Component *c;
  for (cc=comp.components.begin(); cc != comp.components.end(); cc++) {
    c = *cc;
    if ( !comp_name.compare(c->name) ) {
      c0 = c;
      found = true;
      break;
    }
  }

  if (!found) {
    cerr << "UserTreeDSMC: process " << myid 
	 << ": can't find fiducial component <" << comp_name << ">" << endl;
    MPI_Abort(MPI_COMM_WORLD, 35);
  }
  
  Vunit = Lunit/Tunit;
  Eunit = Munit*Vunit*Vunit;

				// Diameter*Bohr radius in Lunits
  diam = diamfac*a0/(Lunit);
				// Number of protons per mass unit
  collfrac = Munit/mp;

  pH2OT::sub_sample = sub_sample;

  c0->HOTcreate();

  if (tube) {
    c0->Tree()->setSides (boxsize*boxratio, boxsize, boxsize);
    c0->Tree()->setOffset(0.0,              0.0,     0.0);
  } 
  else if (slab) {
    c0->Tree()->setSides (boxsize, boxsize, boxsize*boxratio);
    c0->Tree()->setOffset(0.0,              0.0,     0.0);
  } 
  else {
    c0->Tree()->setSides (2.0*boxsize, 2.0*boxsize, 2.0*boxsize*boxratio);
    c0->Tree()->setOffset(    boxsize,     boxsize,     boxsize*boxratio);
  }

  pCell::bucket = ncell;
  pCell::Bucket = Ncell;

  volume = pH2OT::box[0] * pH2OT::box[1] * pH2OT::box[2];


  //
  // Sanity check on excess attribute if excess calculation is
  // desired
  //
  if (use_exes>=0) {

    int ok1 = 1, ok;

    PartMapItr p    = c0->Particles().begin();
    PartMapItr pend = c0->Particles().end();
    for (; p!=pend; p++) {
      if (use_exes >= static_cast<int>(p->second.dattrib.size())) {
	ok1 = 0;
	break;
      }
    }

    MPI_Allreduce(&ok1, &ok, 1, MPI_INT, MPI_PROD, MPI_COMM_WORLD);

    // Turn off excess calculation
    // if particles have incompatible attributes
    //
    if (ok==0) {
      if (myid==0) {
	cout << "UserTreeDSMC: excess calculation requested but some" << endl
	     << "particles have incompatible float attribute counts." << endl
	     << "Attribute #" << use_exes << ". Continuing without excess."
	     << endl;
      }
      use_exes = -1;
    }
  }

  //
  // Sanity check on Knudsen number calculation
  //
  if (use_Kn>=0) {

    int ok1 = 1, ok;

    PartMapItr p    = c0->Particles().begin();
    PartMapItr pend = c0->Particles().end();
    for (; p!=pend; p++) {
      if (use_Kn >= static_cast<int>(p->second.dattrib.size())) {
	ok1 = 0;
	break;
      }
    }

    MPI_Allreduce(&ok1, &ok, 1, MPI_INT, MPI_PROD, MPI_COMM_WORLD);

    // Turn off excess calculation
    // if particles have incompatible attributes
    //
    if (ok==0) {
      if (myid==0) {
	cout << "UserTreeDSMC: Knudsen number calculation requested but some" << endl
	     << "particles have incompatible float attribute counts." << endl
	     << "Attribute #" << use_Kn << ". Continuing without Knudsen number computation."
	     << endl;
      }
      use_Kn = -1;
    }
  }

  //
  // Sanity check on Strouhal number calculation
  //
  if (use_St>=0) {

    int ok1 = 1, ok;

    PartMapItr p    = c0->Particles().begin();
    PartMapItr pend = c0->Particles().end();
    for (; p!=pend; p++) {
      if (use_St >= static_cast<int>(p->second.dattrib.size())) {
	ok1 = 0;
	break;
      }
    }

    MPI_Allreduce(&ok1, &ok, 1, MPI_INT, MPI_PROD, MPI_COMM_WORLD);

    // Turn off Strouhal number calculation
    // if particles have incompatible attributes
    //
    if (ok==0) {
      if (myid==0) {
	cout << "UserTreeDSMC: Strouhal number calculation requested but some" << endl
	     << "particles have incompatible float attribute counts." << endl
	     << "Attribute #" << use_St << ". Continuing without Strouhal number computation."
	     << endl;
      }
      use_St = -1;
    }
  }

  //
  // Set collision parameters
  //
  Collide::NTC     = ntc;
  Collide::CBA     = cba;
  Collide::CBADIAG = cbadiag;
  Collide::PULLIN  = use_pullin;
  Collide::CNUM    = cnum;
  Collide::ESOL    = esol;
  Collide::EPSMratio = epsm;
  Collide::DRYRUN  = dryrun;
  Collide::NOCOOL  = nocool;
  Collide::TSDIAG  = tsdiag;
  Collide::VOLDIAG  = voldiag;
  Collide::TSPOW   = tspow;
  Collide::MFPDIAG = mfpstat;
  Collide::EFFORT  = use_effort;
				// Create the collision instance
  collide = new CollideLTE(this, diam, nthrds);
  collide->set_temp_dens(use_temp, use_dens);
  if (esol) collide->set_timestep(-1);
  else      collide->set_timestep(use_delt);
  collide->set_Kn(use_Kn);
  collide->set_St(use_St);
  collide->set_excess(use_exes);
  ElostTotCollide = ElostTotEPSM = 0.0;

  //
  // Type of load balancing
  //

  if (use_effort)
    c0->Tree()->LoadBalanceEffort();
  else
    c0->Tree()->LoadBalanceNumber();

  //
  // Timers: set precision to microseconds
  //
  
  partnTime.Microseconds();
  tree1Time.Microseconds();
  tree2Time.Microseconds();
  tstepTime.Microseconds();
  llistTime.Microseconds();
  clldeTime.Microseconds();
  clldeWait.Microseconds();
  partnWait.Microseconds();
  tree1Wait.Microseconds();
  tree2Wait.Microseconds();
  timerDiag.Microseconds();

  //
  // Quantiles for distribution diagnstic
  //
  quant.push_back(0.0);		// 0
  quant.push_back(0.01);	// 1
  quant.push_back(0.05);	// 2
  quant.push_back(0.1);		// 3
  quant.push_back(0.2);		// 4
  quant.push_back(0.5);		// 5
  quant.push_back(0.8);		// 6
  quant.push_back(0.9);		// 7
  quant.push_back(0.95);	// 8
  quant.push_back(0.99);	// 9
  quant.push_back(1.0);		// 10

  userinfo();
}

UserTreeDSMC::~UserTreeDSMC()
{
  delete collide;
}

void UserTreeDSMC::userinfo()
{
  if (myid) return;		// Return if node master node

  print_divider();

  cout << "** User routine TreeDSMC initialized, "
       << "Lunit=" << Lunit << ", Tunit=" << Tunit << ", Munit=" << Munit
       << ", cnum=" << cnum << ", diamfac=" << diamfac << ", diam=" << diam
       << ", madj=" << madj << ", epsm=" << epsm << ", boxsize=" << boxsize 
       << ", ncell=" << ncell << ", Ncell=" << Ncell << ", wght=" << wght
       << ", boxratio=" << boxratio << ", jitter=" << jitter 
       << ", compname=" << comp_name;
  if (msteps>=0) 
    cout << ", with diagnostic output at levels <= " << msteps;
  else if (nsteps>0) 
    cout << ", with diagnostic output every " << nsteps << " steps";
  if (remap>0)     cout << ", remap every " << remap << " steps";
  if (use_temp>=0) cout << ", temp at pos="   << use_temp;
  if (use_dens>=0) cout << ", dens at pos="   << use_dens;
  if (use_Kn>=0)   cout << ", Kn at pos="     << use_Kn;
  if (use_St>=0)   cout << ", St at pos="     << use_St;
  if (use_vol>=0)  cout << ", cell volume at pos=" << use_vol;
  if (use_exes>=0) cout << ", excess at pos=" << use_exes;
  if (use_pullin)  cout << ", Pullin algorithm enabled";
  if (dryrun)      cout << ", collisions disabled";
  if (nocool)      cout << ", cooling disabled";
  if (epsm>0.0)    cout << ", using EPSM";
  else             cout << ", EPSM disabled";
  if (ntc)         cout << ", using NTC";
  else             cout << ", NTC disabled";
  if (cba)         cout << ", using CBA";
  else             cout << ", CBA disabled";
  if (cba && cbadiag)     
                   cout << " with diagnostics";
  if (tube)        cout << ", using TUBE mode";
  else if (slab)  cout << ", using THIN SLAB mode";
  if (use_effort)  cout << ", with effort-based load";
  else             cout << ", with uniform load";
  if (use_multi) {
    cout << ", multistep enabled";
    if (use_delt>=0) 
      cout << ", time step at pos=" << use_delt << ", coolfrac=" << coolfrac;
  }
  cout << endl;

  print_divider();
}

void UserTreeDSMC::initialize()
{
  string val;

  if (get_value("Lunit", val))		Lunit      = atof(val.c_str());
  if (get_value("Tunit", val))		Tunit      = atof(val.c_str());
  if (get_value("Munit", val))		Munit      = atof(val.c_str());
  if (get_value("cnum", val))		cnum       = atoi(val.c_str());
  if (get_value("madj", val))		madj       = atoi(val.c_str());
  if (get_value("wght", val))		wght       = atoi(val.c_str());
  if (get_value("epsm", val))		epsm       = atof(val.c_str());
  if (get_value("diamfac", val))	diamfac    = atof(val.c_str());
  if (get_value("boxsize", val))	boxsize    = atof(val.c_str());
  if (get_value("boxratio", val))	boxratio   = atof(val.c_str());
  if (get_value("jitter", val))		jitter     = atof(val.c_str());
  if (get_value("coolfrac", val))	coolfrac   = atof(val.c_str());
  if (get_value("nsteps", val))		nsteps     = atoi(val.c_str());
  if (get_value("msteps", val))		msteps     = atoi(val.c_str());
  if (get_value("ncell", val))		ncell      = atoi(val.c_str());
  if (get_value("Ncell", val))		Ncell      = atoi(val.c_str());
  if (get_value("compname", val))	comp_name  = val;
  if (get_value("remap", val))		remap      = atoi(val.c_str());
  if (get_value("use_temp", val))	use_temp   = atoi(val.c_str());
  if (get_value("use_dens", val))	use_dens   = atoi(val.c_str());
  if (get_value("use_delt", val))	use_delt   = atoi(val.c_str());
  if (get_value("use_Kn", val))		use_Kn     = atoi(val.c_str());
  if (get_value("use_St", val))		use_St     = atoi(val.c_str());
  if (get_value("use_vol", val))	use_vol    = atoi(val.c_str());
  if (get_value("use_exes", val))	use_exes   = atoi(val.c_str());
  if (get_value("frontier", val))	frontier   = atol(val);
  if (get_value("tspow", val))		tspow      = atoi(val.c_str());
  if (get_value("tsdiag", val))		tsdiag     = atol(val);
  if (get_value("voldiag", val))	voldiag    = atol(val);
  if (get_value("mfpstat", val))	mfpstat    = atol(val);
  if (get_value("cbadiag", val))	cbadiag    = atol(val);
  if (get_value("dryrun", val))		dryrun     = atol(val);
  if (get_value("nocool", val))		nocool     = atol(val);
  if (get_value("use_multi", val))	use_multi  = atol(val);
  if (get_value("use_pullin", val))	use_pullin = atol(val);
  if (get_value("use_effort", val))	use_effort = atol(val);
  if (get_value("esol", val))		esol       = atol(val);
  if (get_value("cba", val))		cba        = atol(val);
  if (get_value("ntc", val))		ntc        = atol(val);
  if (get_value("tube", val))		tube       = atol(val);
  if (get_value("slab", val))		slab       = atol(val);
  if (get_value("sub_sample", val))	sub_sample = atol(val);
  if (get_value("treechk", val))	treechk    = atol(val);
  if (get_value("mpichk", val))		mpichk     = atol(val);
}


void UserTreeDSMC::determine_acceleration_and_potential(void)
{
  BarrierWrapper barrier(MPI_COMM_WORLD, barrier_debug);

#ifdef USE_GPTL
  GPTLstart("UserTreeDSMC::determine_acceleration_and_potential");
#endif

  static bool firstime = true;
  static unsigned nrep = 0;

  //
  // Only compute DSMC when passed the fiducial component
  //

  if (cC != c0) {
#ifdef USE_GPTL
    GPTLstop("UserTreeDSMC::determine_acceleration_and_potential");
#endif
    return;
  }

  //
  // Get timing for entire step so far to load balancing the partition
  //
  double pot_time = c0->get_time_sofar();


  barrier("TreeDSMC: after initialization");


  //
  // Turn off/on diagnostic sanity checking
  // (scans particle and cell lists and can be very time consuming)
  //

  c0->Tree()->list_check = treechk;

  //
  // Turn off/on diagnostic barrier in the tree
  //

  c0->Tree()->MPIchk(mpichk);

  //
  // Make the cells
  //

  if (firstime) {
    c0->Tree()->setWeights(wght ? true : false);
    c0->Tree()->Repartition(0); nrep++;
    c0->Tree()->makeTree();
    c0->Tree()->checkCellTree();
    if (use_temp || use_dens || use_vol) assignTempDensVol();

    stepnum = 0;
    curtime = tnow;

#ifdef DEBUG
    cout << "Computed partition and tree [firstime on #" 
	 << setw(4) << left << myid << "]" << endl;
#endif
  } else {
    
    if (tnow-curtime < 1.0e-14) {
      if (myid==0) {
	cout << "UserTreeDSMC: attempt to redo step at T=" << tnow << endl;
      }
#ifdef USE_GPTL
      GPTLstop("UserTreeDSMC::determine_acceleration_and_potential");
#endif
      return; 			// Don't do this time step again!
    }

    stepnum++;
    curtime = tnow;
  }

#ifdef DEBUG
  c0->Tree()->densCheck();
#endif
  
#ifdef DEBUG
  if (c0->Tree()->checkParticles()) {
    cout << "After init only: Particle check ok [" << right
	 << setw(3) << mlevel << ", " << setw(3) << myid << "]" << endl;

  } else {
    cout << "After init only: Particle check FAILED [" << right
	 << setw(3) << mlevel << ", " << setw(3) << myid << "]" << endl;
  }
#endif

  barrier("TreeDSMC: after cell computation");

  //
  // Only run diagnostics every nsteps
  //
  bool diagstep = (nsteps>0 && mstep%nsteps == 0);

  //
  // Diagnostics run at levels <= msteps (takes prececdence over nsteps)
  //
  if (msteps>=0) 
    diagstep = (mlevel <= static_cast<unsigned>(msteps)) ? true : false;

  //
  // Compute time step
  //
  // Now, DSMC is computed on the smallest step, every step
  double tau = dtime*mintvl[multistep]/Mstep;


  MPI_Bcast(&tau, 1, MPI_DOUBLE, 0, MPI_COMM_WORLD);

  TimeElapsed partnSoFar, tree1SoFar, tree2SoFar, tstepSoFar, collideSoFar;
  TimeElapsed waitcSoFar, waitpSoFar, wait1SoFar, wait2SoFar, timerSoFar;

  overhead.Start();

  //
  // Load balancing
  //
  if (remap>0 && mlevel==0 && !firstime) {
    if ( (this_step % remap) == 0) {
#ifdef USE_GPTL
      GPTLstart("UserTreeDSMC::remap");
#endif
      c0->Tree()->Remap();
#ifdef USE_GPTL
      GPTLstop("UserTreeDSMC::remap");
#endif
    }
  }

  //
  // Sort the particles into cells
  //
  if (mlevel<=madj) {

#ifdef USE_GPTL
    GPTLstart("UserTreeDSMC::pH2OT");
    GPTLstart("UserTreeDSMC::waiting");
    barrier("TreeDSMC: pHOT waiting");
    GPTLstop ("UserTreeDSMC::waiting");
    GPTLstart("UserTreeDSMC::repart");
#endif

    barrier("TreeDSMC: after pH2OT wait");

    partnTime.start();
    c0->Tree()->Repartition(mlevel); nrep++;
    partnSoFar = partnTime.stop();

    partnWait.start();
    barrier("TreeDSMC: after repartition");
    waitpSoFar = partnWait.stop();

#ifdef USE_GPTL
    GPTLstop ("UserTreeDSMC::repart");
    GPTLstart("UserTreeDSMC::makeTree");
#endif
    tree1Time.start();
    c0->Tree()->makeTree();
    tree1Time.stop();
    tree1Wait.start();
    barrier("TreeDSMC: after makeTree");
    wait1SoFar = tree1Wait.stop();
#ifdef USE_GPTL
    GPTLstop ("UserTreeDSMC::makeTree");
    GPTLstart("UserTreeDSMC::pcheck");
#endif
    tree1Time.start();
    c0->Tree()->checkCellTree();
#ifdef DEBUG
    cout << "Made partition, tree and level list [" << mlevel << "]" << endl;
    if (c0->Tree()->checkParticles()) {
      cout << "Particle check on new tree ok [" << mlevel << "]" << endl;
    } else {
      cout << "Particle check on new tree FAILED [" << mlevel << "]" << endl;
    }
#endif
    tree1SoFar = tree1Time.stop();
    
#ifdef USE_GPTL
    GPTLstop("UserTreeDSMC::pcheck");
    GPTLstop("UserTreeDSMC::pH2OT");
#endif

  } else {

#ifdef USE_GPTL
    GPTLstart("UserTreeDSMC::pHOT_2");
    GPTLstart("UserTreeDSMC::adjustTree");
#endif

#ifdef DEBUG
    cout << "About to adjust tree [" << mlevel << "]" << endl;
#endif
    tree2Time.start();
    c0->Tree()->adjustTree(mlevel);
    tree2SoFar = tree2Time.stop();
    tree2Wait.start();
    barrier("TreeDSMC: after adjustTree");
    wait2SoFar = tree2Wait.stop();

#ifdef USE_GPTL
    GPTLstop ("UserTreeDSMC::adjustTree");
    GPTLstop("UserTreeDSMC::pHOT_2");
#endif

  }

  overhead.Stop();
  pot_time += overhead.getTime();
  pot_time /= max<unsigned>(1, c0->Number());
  
  if (use_effort) {
    PartMapItr pitr = c0->Particles().begin(), pend = c0->Particles().end();
    for (; pitr!= pend; pitr++) pitr->second.effort = pot_time;
  }

  //
  // Evaluate collisions among the particles
  //

  clldeTime.start();
  
  if (0) {
    ostringstream sout;
    sout << "before Collide [" << nrep << "], " 
	 << __FILE__ << ": " << __LINE__;
    c0->Tree()->checkBounds(2.0, sout.str().c_str());
  }

#ifdef USE_GPTL
  GPTLstart("UserTreeDSMC::collide");
#endif

  collide->collide(*c0->Tree(), collfrac, tau, mlevel, diagstep);
    
  collideSoFar = clldeTime.stop();

  clldeWait.start();
  barrier("TreeDSMC: after collide");

#ifdef USE_GPTL
  GPTLstop("UserTreeDSMC::collide");
#endif

  waitcSoFar = clldeWait.stop();

				// Time step request
#ifdef USE_GPTL
  GPTLstart("UserTreeDSMC::collide_timestep");
#endif


  barrier("TreeDSMC: before collide timestep");

  tstepTime.start();
  if (use_multi) collide->compute_timestep(c0->Tree(), coolfrac);
  tstepSoFar = tstepTime.stop();
  
#ifdef USE_GPTL
  GPTLstop("UserTreeDSMC::collide_timestep");
#endif

  //
  // Repartition and remake the tree after first step to adjust load
  // balancing for work queue effort method
  //
  if (0 && firstime && remap>0 && use_effort && mlevel==0) {
    c0->Tree()->Remap();
    c0->Tree()->Repartition(0);
    c0->Tree()->makeTree();
    c0->Tree()->checkCellTree();
  }

  firstime = false;

  //
  // Periodically display the current progress
  //
  //
  if (diagstep) {
#ifdef USE_GPTL
    GPTLstart("UserTreeDSMC::collide_diag");
#endif

				// Uncomment for debug
    // collide->Debug(tnow);

    unsigned medianNumb = collide->medianNumber();
    unsigned collnum=0, coolnum=0;
    if (mfpstat) {
      collide->collQuantile(quant, coll_);
      collide->mfpsizeQuantile(quant, mfp_, ts_, nsel_, cool_, rate_,
			       collnum, coolnum);
    }

    double ExesCOLL, ExesEPSM;
    if (use_exes>=0) collide->energyExcess(ExesCOLL, ExesEPSM);
      
    if (frontier) {
      ostringstream sout;
      sout << outdir << runtag << ".DSMC_frontier";
      string filen = sout.str();
      c0->Tree()->testFrontier(filen);
    }

    vector<unsigned> ncells, bodies;
    c0->Tree()->countFrontier(ncells, bodies);

    if (mfpstat && myid==0) {

				// Generate the file name
      ostringstream sout;
      sout << outdir << runtag << ".DSMC_mfpstat";
      string filen = sout.str();

				// Check for existence
      ifstream in(filen.c_str());
      if (in.fail()) {
				// Write a new file
	ofstream out(filen.c_str());
	if (out) {
	  out << left
	      << setw(14) << "# Time"
	      << setw(14) << "Quantiles" 
	      << setw(14) << "Bodies"
	      << setw(14) << "MFP/size"
	      << setw(14) << "Flight/size"
	      << setw(14) << "Collions/cell"
	      << setw(14) << "Nsel/Number"
	      << setw(14) << "Energy ratio"
	      << setw(14) << "Excess ratio"
	      << endl;
	}
      }
      in.close();

				// Open old file to write a stanza
				// 
      ofstream out(filen.c_str(), ios::app);
      if (out) {
	for (unsigned nq=0; nq<quant.size(); nq++) {
	  out << setw(14) << tnow
	      << setw(14) << quant[nq]
	      << setw(14) << collnum
	      << setw(14) << mfp_[nq] 
	      << setw(14) << ts_[nq] 
	      << setw(14) << coll_[nq] 
	      << setw(14) << nsel_[nq] 
	      << setw(14) << cool_[nq] 
	      << setw(14) << rate_[nq] 
	      << endl;
	}
	out << endl;
      }
    }

    barrier("TreeDSMC: after mfp stats");

				// Overall statistics
				// 
    double KEtotl=collide->Etotal(), KEtot=0.0;
    double Mtotal=collide->Mtotal(), Mtotl=0.0;
    double Elost1, Elost2, ElostC=0.0, ElostE=0.0;

    collide->Elost(&Elost1, &Elost2);

    MPI_Reduce(&KEtotl, &KEtot,  1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&Mtotal, &Mtotl,  1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&Elost1, &ElostC, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&Elost2, &ElostE, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

				// Computing mass-weighted temperature
    const double f_H = 0.76;
    double mm = f_H*mp + (1.0-f_H)*4.0*mp;
    double meanT = 0.0;
    if (Mtotl>0.0) meanT = 2.0*KEtot/Mtotl*Eunit/3.0 * mm/Munit/boltz;

    unsigned cellBods = c0->Tree()->checkNumber();
    unsigned oobBods  = c0->Tree()->oobNumber();

    double Mass = 0.0;
    unsigned Counts = 0;
    // c0->Tree()->totalMass(Counts, Mass);

				// Check frontier for mass at or below 
				// current level
    double cmass1=0.0, cmass=0.0;
    pH2OT_iterator pit(*(c0->Tree()));

    barrier("TreeDSMC: checkAdjust");

    while (pit.nextCell()) {
      pCell *cc = pit.Cell();
      if (cc->maxplev >= mlevel && cc->count > 1) cmass1 += cc->state[0];
    }

				// Collect up info from all processes
    timerDiag.start();
    MPI_Reduce(&cmass1, &cmass, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    c0->Tree()->CollectTiming();
    collide->CollectTiming();
    timerSoFar = timerDiag.stop();

    const int nf = 11;
    vector<double> in(nf);
    vector< vector<double> > out(3);
    for (int i=0; i<3; i++) out[i] = vector<double>(nf);

    in[ 0] = partnSoFar() * 1.0e-6;
    in[ 1] = tree1SoFar() * 1.0e-6;
    in[ 2] = tree2SoFar() * 1.0e-6;
    in[ 3] = tstepSoFar() * 1.0e-6;
    in[ 4] = llistTime.getTime().getRealTime() * 1.0e-6;
    in[ 5] = collideSoFar() * 1.0e-6;
    in[ 6] = timerSoFar() * 1.0e-6;
    in[ 7] = waitpSoFar() * 1.0e-6;
    in[ 8] = waitcSoFar() * 1.0e-6;
    in[ 9] = wait1SoFar() * 1.0e-6;
    in[10] = wait2SoFar() * 1.0e-6;

    MPI_Reduce(&in[0], &out[0][0], nf, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
    MPI_Reduce(&in[0], &out[1][0], nf, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&in[0], &out[2][0], nf, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    vector<double> tot(3, 0.0);
    for (int i=0; i<nf; i++) {
      out[1][i] /= numprocs;
      //
      tot[0] += out[0][i];
      tot[1] += out[1][i];
      tot[2] += out[2][i];
    }

    int pCellTot;
    MPI_Reduce(&pCell::live, &pCellTot, 1, MPI_INT, MPI_SUM, 0, MPI_COMM_WORLD);

    if (myid==0) {

      unsigned sell_total = collide->select();
      unsigned coll_total = collide->total();
      unsigned coll_error = collide->errors();
      unsigned epsm_total = collide->EPSMtotal();
      unsigned epsm_cells = collide->EPSMcells();

      vector<double> disp;
      collide->dispersion(disp);
      double dmean = (disp[0]+disp[1]+disp[2])/3.0;

      ostringstream sout;
      sout << outdir << runtag << ".DSMC_log";
      ofstream mout(sout.str().c_str(), ios::app);

      mout << "Summary:" << endl << left << "--------" << endl << scientific
	   << setw(6) << " " << setw(20) << tnow       << "current time" << endl
	   << setw(6) << " " << setw(20) << mlevel     << "current level" << endl
	   << setw(6) << " " << setw(20) << Counts     << "total counts" << endl
	   << setw(6) << " " << setw(20) << Mass       << "total mass" << endl
	   << setw(6) << " " << setw(20) << meanT      << "mass-weighted temperature" << endl
	   << setw(6) << " " << setw(20) << Mtotl      << "accumulated mass" << endl
	   << setw(6) << " " << setw(20) << cmass      << "mass at this level" << endl
	   << setw(6) << " " << setw(20) << mstep      << "step number" << endl
	   << setw(6) << " " << setw(20) << stepnum    << "step count" << endl
	   << setw(6) << " " << setw(20) << sell_total << "targets" << endl
	   << setw(6) << " " << setw(20) << coll_total << "collisions" << endl
	   << setw(6) << " " << setw(20) << coll_error << "collision errors (" 
	   << setprecision(2) << fixed 
	   << 100.0*coll_error/(1.0e-08+coll_total) << "%)" << endl
	   << setw(6) << " " << setw(20) << oobBods << "out-of-bounds" << endl
	   << endl;

      collide->colldeTime(mout);

      if (epsm>0) mout << setw(6) << " " << setw(20) << epsm_total 
		       << "EPSM particles ("
		       << 100.0*epsm_total/c0->nbodies_tot << "%)" 
		       << scientific << endl;
      mout << setw(6) << " " << setw(20) << medianNumb << "number/cell" << endl
	   << setw(6) << " " << setw(20) << c0->Tree()->TotalNumber() 
	   << "total # cells" << endl;

      if (epsm>0) mout << setw(6) << " " << setw(20) << epsm_cells 
		       << "EPSM cells (" << setprecision(2) << fixed 
		       << 100.0*epsm_cells/c0->Tree()->TotalNumber() 
		       << "%)" << scientific << endl;

      if (mfpstat) {
	mout << setw(6) << " " << setw(20) << "--------" << "--------------------" << endl
	     << setw(6) << " " << setw(20) << nsel_[0] << "collision/body @  0%" << endl
	     << setw(6) << " " << setw(20) << nsel_[2] << "collision/body @  5%" << endl
	     << setw(6) << " " << setw(20) << nsel_[5] << "collision/body @ 50%" << endl
	     << setw(6) << " " << setw(20) << nsel_[8] << "collision/body @ 95%" << endl
	     << setw(6) << " " << setw(20) << nsel_[10] << "collision/body @100%" << endl;

	mout << fixed << setprecision(0)
	     << setw(6) << " " << setw(20) << "--------" << "--------------------" << endl

	     << setw(6) << " " << setw(20) << coll_[0] << "collision/cell @  0%" << endl
	     << setw(6) << " " << setw(20) << coll_[2] << "collision/cell @  5%" << endl
	     << setw(6) << " " << setw(20) << coll_[5] << "collision/cell @ 50%" << endl
	     << setw(6) << " " << setw(20) << coll_[8] << "collision/cell @ 95%" << endl
	     << setw(6) << " " << setw(20) << coll_[10] << "collision/cell @100%" << endl
	     << setw(6) << " " << setw(20) << "--------" << "--------------------" << endl

	     << setw(6) << " " << setw(20) << c0->Tree()->CellCount(0.0) 
	     << "occupation @  0%" << endl
	     << setw(6) << " " << setw(20) << c0->Tree()->CellCount(0.05) 
	     << "occupation @  5%" << endl
	     << setw(6) << " " << setw(20) << c0->Tree()->CellCount(0.50) 
	     << "occupation @ 50%" << endl
	     << setw(6) << " " << setw(20) << c0->Tree()->CellCount(0.95) 
	     << "occupation @ 95%" << endl
	     << setw(6) << " " << setw(20) << c0->Tree()->CellCount(1.0) 
	     << "occupation @100%" << endl
	     << setw(6) << " " << setw(20) << "--------" << "--------------------" << endl
	     << setw(6) << " " << setw(20) << cellBods
	     << "total number in cells" << endl
	     << endl << setprecision(4);
      }
	
      ElostTotCollide += ElostC;
      ElostTotEPSM    += ElostE;

      mout << scientific << "Energy (system):" << endl 
	   << "----------------" << endl 
	   << " Lost collide = " << ElostC << endl;
      if (epsm>0) mout << "    Lost EPSM = " << ElostE << endl;
      mout << "   Total loss = " << ElostTotCollide+ElostTotEPSM << endl;
      if (epsm>0) mout << "   Total EPSM = " << ElostTotEPSM << endl;
      mout << "     Total KE = " << KEtot << endl;
      if (use_exes>=0) {
	mout << "  COLL excess =" << ExesCOLL << endl;
	if (epsm>0) mout << "  EPSM excess = " << ExesEPSM << endl;
      }
      if (KEtot<=0.0) mout << "         Ratio= XXXX" << endl;
      else mout << "   Ratio lost = " << (ElostC+ElostE)/KEtot << endl;
      mout << "     3-D disp = " << disp[0] << ", " << disp[1] 
	   << ", " << disp[2] << endl;
      if (dmean>0.0) {
	mout << "   Disp ratio = " << disp[0]/dmean << ", " 
	     << disp[1]/dmean << ", " << disp[2]/dmean << endl << endl;
      }
	
      unsigned sumcells=0, sumbodies=0;
      mout << endl;
      mout << "-----------------------------------------------------" << endl;
      mout << "-----Cell/body diagnostics---------------------------" << endl;
      mout << "-----------------------------------------------------" << endl;
      mout << right << setw(8) << "Level" << setw(15) << "Scale(x)"
	   << setw(10) << "Cells" << setw(10) << "Bodies" << endl;
      mout << "-----------------------------------------------------" << endl;
      for (unsigned n=0; n<ncells.size(); n++) {
	mout << setw(8) << n << setw(15) << pH2OT::box[0]/(1<<n)
	     << setw(10) << ncells[n] << setw(10) << bodies[n]
	     << endl;
	sumcells  += ncells[n];
	sumbodies += bodies[n];
      }
      mout << "-----------------------------------------------------" << endl;
      mout << setw(8) << "TOTALS" 
	   << setw(15) << "**********"
	   << setw(10) << sumcells << setw(10) << sumbodies << endl;
      mout << "-----------------------------------------------------" << endl;
      mout << left << endl;
      
      vector<float> cstatus, xchange, prepare, convert, cupdate, tadjust;
      vector<float> scatter, repartn, keybods, schecks, waiton0, waiton1;
      vector<float> waiton2, bodlist, celladj, getsta1, getsta2, getsta3;
      vector<float> treebar;
      vector<unsigned> numbods;

      c0->Tree()->Timing(cstatus, xchange, prepare, convert, tadjust, 
			 cupdate, scatter, repartn, keybods, schecks, 
			 waiton0, waiton1, waiton2, bodlist, celladj, 
			 getsta1, getsta2, getsta3, treebar, numbods);

      mout << "-----------------------------" << endl
	   << "Timing (secs) at mlevel="      << mlevel << endl
	   << "-----------------------------" << endl;

      outHelper0(mout, "partition",    0, out, tot);
      outHelper0(mout, "partn wait",   7, out, tot);
      outHelper0(mout, "make tree",    1, out, tot);
      outHelper0(mout, "make wait",    9, out, tot);
      outHelper0(mout, "adjust tree",  2, out, tot);
      outHelper0(mout, "adjust wait", 10, out, tot);
      mout << endl;

      mout << "                    " 
	   << "    " << setw(2) << pH2OT::qtile[0] << "%     " 
	   << "    " << setw(2) << pH2OT::qtile[1] << "%     " 
	   << "    " << setw(2) << pH2OT::qtile[2] << "%" << endl;

      outHelper1<float>(mout, "cstatus", cstatus);
      outHelper1<float>(mout, "keybods", keybods);
      outHelper1<float>(mout, "xchange", xchange);
      outHelper1<float>(mout, "prepare", prepare);
      outHelper1<float>(mout, "convert", convert);
      outHelper1<float>(mout, "tadjust", tadjust);
      outHelper1<float>(mout, "cupdate", cupdate);
      outHelper1<float>(mout, "scatter", scatter);
      outHelper1<float>(mout, "repartn", repartn);
      outHelper1<float>(mout, "schecks", schecks);
      outHelper1<float>(mout, "celladj", celladj);
      outHelper1<float>(mout, "bodlist", bodlist);
      outHelper1<float>(mout, "stats#1", getsta1);
      outHelper1<float>(mout, "stats#2", getsta2);
      outHelper1<float>(mout, "stats#3", getsta3);

      if (mpichk) {
	outHelper1<float>(mout, "wait #0", waiton0);
	outHelper1<float>(mout, "wait #1", waiton1);
	outHelper1<float>(mout, "wait #2", waiton2);
	outHelper1<float>(mout, "barrier", treebar);
      }
      outHelper1<unsigned int>(mout, "numbods", numbods);
      mout << endl;

      outHelper0(mout, "timesteps", 3, out, tot);
      outHelper0(mout, "step list", 4, out, tot);
      outHelper0(mout, "collide  ", 5, out, tot);
      outHelper0(mout, "coll wait", 8, out, tot);
      outHelper0(mout, "overhead ", 6, out, tot);

      collide->tsdiag(mout);
      collide->voldiag(mout);

      partnTime.reset();
      tree1Time.reset();
      tree2Time.reset();
      tstepTime.reset();
      llistTime.reset();
      clldeTime.reset();
      timerDiag.reset();
      partnWait.reset();
      tree1Wait.reset();
      tree2Wait.reset();
      clldeWait.reset();

      //
      // Cell instance diagnostics
      //
      mout << "-----------------------------" << endl
	   << "Object counts at mlevel="      << mlevel << endl
	   << "-----------------------------" << endl
	   << " pCell # = " << pCellTot    << endl
	   << " tCell # = " << tCell::live << endl
	   << " tTree # = " << tTree::live << endl
	   << " pTree # = " << pTree::live << endl
	   << "-----------------------------" << endl;
    }

				// Debugging usage
				//
    collide->EPSMtiming(cout);
    collide->getCPUHog(cout);

#ifdef USE_GPTL
    GPTLstop("UserTreeDSMC::collide_diag");
#endif

  }

#ifdef DEBUG
  if (c0->Tree()->checkParticles()) {
    cout << "Before level list: Particle check ok [" << right
	 << setw(3) << mlevel << ", " << setw(3) << myid << "]" << endl;
  } else {
    cout << "Before level list: Particle check FAILED [" << right
	 << setw(3) << mlevel << ", " << setw(3) << myid << "]" << endl;
  }
#endif

  barrier("TreeDSMC: after collision diags");

				// Remake level lists because particles
				// will (usually) have been exchanged 
				// between nodes
  llistTime.start();
  c0->reset_level_lists();
  llistTime.stop();

#ifdef DEBUG
  if (c0->Tree()->checkParticles()) {
    cout << "After level list: Particle check ok [" << right
	 << setw(3) << mlevel << ", " << setw(3) << myid << "]" << endl;
  } else {
    cout << "After level list: Particle check FAILED [" << right
	 << setw(3) << mlevel << ", " << setw(3) << myid << "]" << endl;

  }
#endif

#ifdef USE_GPTL
  GPTLstop("UserTreeDSMC::determine_acceleration_and_potential");
#endif

				// Debugging disk
				//
  // triggered_cell_body_dump(0.01, 0.002);
  // TempHisto();

  barrier("TreeDSMC: end of accel routine");

}

void UserTreeDSMC::triggered_cell_body_dump(double time, double radius)
{
  static bool done = false;
  if (tnow<time) return;
  if (done) return;

  unsigned cnt=0;
  vector<double> p(3);
  pH2OT_iterator c(*c0->Tree());

  for (unsigned n=0; n<c0->Tree()->Number(); n++) {
    c.nextCell();
    double r2 = 0.0;

    c.Cell()->MeanPos(p);
    for (unsigned k=0; k<3; k++) r2 += p[k]*p[k];

    if (r2 < radius*radius) {
      ostringstream ostr;
      ostr << outdir << runtag << ".testcell." << myid << "." << cnt++;
      ofstream out(ostr.str().c_str());

      // for (set<unsigned long>::iterator j=c.Cell()->bods.begin();
      for (vector<unsigned long>::iterator j=c.Cell()->bods.begin();
	   j!=c.Cell()->bods.end(); j++) {
	for (unsigned k=0; k<3; k++) 
	  out << setw(18) << c0->Tree()->Body(*j)->pos[k];
	for (unsigned k=0; k<3; k++) 
	  out << setw(18) << c0->Tree()->Body(*j)->vel[k];
	out << endl;
      }
    }
  }

  done = true;
}


void UserTreeDSMC::assignTempDensVol()
{
  const double f_H = 0.76;
  double mm = f_H*mp + (1.0-f_H)*4.0*mp;
  double KEtot, KEdsp, T;
  double Tfac = 2.0*UserTreeDSMC::Eunit/3.0 * mm/UserTreeDSMC::Munit/boltz;
  pCell *cell;
#ifdef DEBUG
  unsigned nbod=0, ntot, zbod=0, ztot, pcel=0, ptot;
  unsigned sing=0, stot, ceqs=0, qtot, zero=0, none;
  double n2=0.0, n1=0.0, N2, N1;
  double MinT, MaxT, MeanT, VarT;
  double minT=1e20, maxT=0.0, meanT=0.0, varT=0.0;
#endif

  map<unsigned, tCell*>::iterator n, nb, ne;
  key_cell::iterator it, itb, ite;

  nb = c0->Tree()->trees.frontier.begin(); 
  ne = c0->Tree()->trees.frontier.end(); 
  for (n = nb; n != ne; n++)
    {
      
      itb = n->second->ptree->frontier.begin(); 
      ite = n->second->ptree->frontier.end();;

      for (it = itb; it != ite; it++)
	{
	  cell = it->second;
	  cell->sample->KE(KEtot, KEdsp);

	  T = KEdsp * Tfac;

	  // Assign temp and/or density to particles
	  //
	  if (use_temp>=0 || use_dens>=0 || use_vol>=0) {
#ifdef DEBUG
	    unsigned ssz = cell->sample->count;
#endif
	    unsigned csz = cell->count;
	    double  volm = cell->Volume();
	    double  dens = cell->Mass()/volm;
	    // set<unsigned long>::iterator j = cell->bods.begin();
	    vector<unsigned long>::iterator j = cell->bods.begin();
	    while (j != cell->bods.end()) {
	      if (*j == 0) {
		cout << "proc=" << myid << " id=" << id 
		     << " ptr=" << hex << cell << dec
		     << " indx=" << *j << "/" << csz << endl;
		j++;
		continue;
	      }
	      
	      int sz = cell->Body(j)->dattrib.size();
	      if (use_temp>=0 && use_temp<sz) 
		cell->Body(j)->dattrib[use_temp] = T;
	      if (use_dens>=0 && use_dens<sz) 
		cell->Body(j)->dattrib[use_dens] = dens;
	      if ((use_vol>=0) && use_vol<sz)
		cell->Body(j)->dattrib[use_dens] = volm;
	      j++;
	    }
#ifdef DEBUG
	    if (T>0.0) {
	      nbod += csz;
	      minT = min<double>(T, minT);
	      maxT = max<double>(T, maxT);
	      meanT += csz*T;
	      varT  += csz*T*T;
	    } else {
	      zbod += csz;
	      if (ssz>1) {	// Sample cell has more than 1?
		n1 += ssz;
		n2 += ssz * ssz;
		pcel++;
				// Singletons sample cells
	      } else if (ssz==1) {
		sing++;
		if (cell->sample == cell) ceqs++;
	      } else {		// Empty cells
		zero++;
	      }
	    }
#endif
	  }
	}
    }

#ifdef DEBUG
  MPI_Reduce(&nbod,  &ntot,  1, MPI_UNSIGNED, MPI_SUM, 0, MPI_COMM_WORLD);
  MPI_Reduce(&zbod,  &ztot,  1, MPI_UNSIGNED, MPI_SUM, 0, MPI_COMM_WORLD);
  MPI_Reduce(&pcel,  &ptot,  1, MPI_UNSIGNED, MPI_SUM, 0, MPI_COMM_WORLD);
  MPI_Reduce(&sing,  &stot,  1, MPI_UNSIGNED, MPI_SUM, 0, MPI_COMM_WORLD);
  MPI_Reduce(&ceqs,  &qtot,  1, MPI_UNSIGNED, MPI_SUM, 0, MPI_COMM_WORLD);
  MPI_Reduce(&zero,  &none,  1, MPI_UNSIGNED, MPI_SUM, 0, MPI_COMM_WORLD);
  MPI_Reduce(&n1,    &N1,    1, MPI_DOUBLE,   MPI_SUM, 0, MPI_COMM_WORLD);
  MPI_Reduce(&n2,    &N2,    1, MPI_DOUBLE,   MPI_SUM, 0, MPI_COMM_WORLD);
  MPI_Reduce(&meanT, &MeanT, 1, MPI_DOUBLE,   MPI_SUM, 0, MPI_COMM_WORLD);
  MPI_Reduce(&varT,  &VarT,  1, MPI_DOUBLE,   MPI_SUM, 0, MPI_COMM_WORLD);
  MPI_Reduce(&minT,  &MinT,  1, MPI_DOUBLE,   MPI_MIN, 0, MPI_COMM_WORLD);
  MPI_Reduce(&maxT,  &MaxT,  1, MPI_DOUBLE,   MPI_MAX, 0, MPI_COMM_WORLD);

  if (myid==0) {
    cout << setfill('-') << setw(70) << '-' << setfill(' ') << endl;
    cout << "Non-zero temperature assigned for " << ntot << " bodies"  << endl
	 << stot << " cells are singletons and " << qtot << " are roots" << endl
	 << none << " cells are empty" << endl
	 << "Zero temperature assigned for " << ztot << " bodies" << endl;
    if (ptot>1)
      cout << ", mean(N) = " << N1/ptot << endl
	   << " stdev(N) = " << sqrt( (N2 - N1*N1/ptot)/(ptot-1) ) << endl;
    cout << "MinT = " << MinT << endl << "MaxT = " << MaxT << endl;
    if (ntot>0)
      cout << " mean(T) = " << MeanT/ntot << endl;
    if (ntot>1) 
      cout << "stdev(T) = " 
	   << sqrt( (VarT - MeanT*MeanT/ntot)/(ntot-1) ) << endl;
    cout << setfill('-') << setw(70) << '-' << setfill(' ') << endl;
  }

druv  TempHisto();
#endif
}

void UserTreeDSMC::TempHisto()
{
  if (use_temp<0) return;

  pCell *cell;
  double Tlog, T, M, V, totalM1 = 0.0, totalM0;
  const int numT = 40;
  const double TlogMin = 3.0, TlogMax = 8.0;
  vector<double> td1(numT+2, 0), td0(numT+2);
  vector<double> vd1(numT+2, 0), vd0(numT+2);

  map<unsigned, tCell*>::iterator n, nb, ne;
  key_cell::iterator it, itb, ite;

  nb = c0->Tree()->trees.frontier.begin(); 
  ne = c0->Tree()->trees.frontier.end(); 
  for (n = nb; n != ne; n++)
    {
      
      itb = n->second->ptree->frontier.begin(); 
      ite = n->second->ptree->frontier.end();;

      for (it = itb; it != ite; it++)
	{
	  cell = it->second;
	  // set<unsigned long>::iterator j = cell->bods.begin();
	  vector<unsigned long>::iterator j = cell->bods.begin();
	  V = cell->Volume();
	  while (j != cell->bods.end()) {
	    T = cell->Body(j)->dattrib[use_temp];
	    if (T>0.0) {
	      M = cell->Body(j)->mass;
	      totalM1 += M;
	      Tlog = log(T)/log(10.0);
	      if (Tlog<TlogMin) {
		td1[0] += M;
		vd1[0] += V*M;
	      } else if (Tlog>=TlogMax) {
		td1[numT+1] += M;
		vd1[numT+1] += V*M;
	      } else {
		int indx = floor((log(T)/log(10.0) - TlogMin) /
				 (TlogMax - TlogMin)*numT) + 1;
		td1[indx] += M;
		vd1[indx] += V*M;
	      }
	    }
	    j++;
	  }
	}
    }

  
  MPI_Reduce(&totalM1, &totalM0, 1,      MPI_DOUBLE, MPI_SUM, 0, 
	     MPI_COMM_WORLD);

  MPI_Reduce(&td1[0],  &td0[0],  numT+2, MPI_DOUBLE, MPI_SUM, 0, 
	     MPI_COMM_WORLD);

  MPI_Reduce(&vd1[0],  &vd0[0],  numT+2, MPI_DOUBLE, MPI_SUM, 0, 
	     MPI_COMM_WORLD);

  if (myid==0) {

    for (int i=0; i<numT+2; i++) {
      if (td0[i]>0.0) vd0[i] /= td0[i];
    }

    cout << "----------------" << endl
	 << "Temperature dist" << endl
	 << "Time=" << tnow    << endl
	 << "----------------" << endl
	 << setw(10) << "<1000" 
	 << setw(10) << setprecision(2) << td0[0]/totalM0
	 << setw(10) << setprecision(2) << vd0[0] << endl;
    for (int i=0; i<numT; i++) 
      cout << setw(10) << setprecision(2) 
	   << pow(10.0, TlogMin + (TlogMax-TlogMin)/numT*(0.5+i))
	   << setw(10) << setprecision(2) << td0[i+1]/totalM0
	   << setw(10) << setprecision(2) << vd0[i+1] << endl;
    cout << setw(10) << ">1e8" << setw(10) 
	 << setprecision(2) << td0[numT+1]/totalM0
	 << setw(10) << setprecision(2) << vd0[numT+1] << endl;
  }
}

extern "C" {
  ExternalForce *makerTreeDSMC(string& line)
  {
    return new UserTreeDSMC(line);
  }
}

class proxytreedsmc { 
public:
  proxytreedsmc()
  {
    factory["usertreedsmc"] = makerTreeDSMC;
  }
};

proxytreedsmc p;
