
#include "expand.h"

void do_step(int);
void clean_up(void);

#include <signal.h>
#include <sys/time.h>
#include <sys/resource.h>

/*
static char rcsid[] = "$Id$";
*/

//! Abort the time stepping and checkpoint when signaled
void signal_handler(int sig) 
{
  if (myid==0) {
    stop_signal = 1;
    cout << endl << "Process 0: user signaled a stop at step=" << this_step << " . . . quitting on next step after output" << endl;
  } else {
    cout << endl << "Process " << myid << ": user signaled a stop but only the root process can stop me . . . continuing" << endl;
  }
}

//! Sync argument lists on all processes
void MPL_parse_args(int argc, char** argv);

/**
   The MAIN routine
*/
int 
main(int argc, char** argv)
{
  const int hdbufsize=1024;
  char hdbuffer[hdbufsize];

  int *nslaves, n, retdir, retdir0;
  MPI_Group world_group, slave_group;


  //===================
  // MPI preliminaries 
  //===================

  MPI_Init(&argc,&argv);
  MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
  MPI_Comm_rank(MPI_COMM_WORLD, &myid);
  MPI_Get_processor_name(processor_name, &proc_namelen);

				// Make SLAVE group 
  slaves = numprocs - 1;
  MPI_Comm_group(MPI_COMM_WORLD, &world_group);
  nslaves = new int [slaves];
  if (!nslaves) {
    cerr << "main: problem allocating <nslaves>\n";
    MPI_Abort(MPI_COMM_WORLD, 10);
    exit(-1);
  }
  for (n=1; n<numprocs; n++) nslaves[n-1] = n;
  MPI_Group_incl(world_group, slaves, nslaves, &slave_group);
  MPI_Comm_create(MPI_COMM_WORLD, slave_group, &MPI_COMM_SLAVE);
  delete [] nslaves;
    
  // Debug id 
  MPI_Group_rank ( slave_group, &n );
  cerr << "Process " << myid << " on " << processor_name
       << "   rank in SLAVE: " << n << "\n";
  
  MPI_Barrier(MPI_COMM_WORLD);

#ifdef MPE_PROFILE
  MPE_Init_log();

  if (myid == 0) {
    MPE_Describe_state(1, 2, "Distribute particles", "red:dimple3" );
    MPE_Describe_state(3, 4, "Gather particles", "green:dllines3" );
    MPE_Describe_state(5, 6, "Gather coefs", "cyan:hlines2" );
    MPE_Describe_state(7, 8, "Distribute coefs", "yellow:drlines4" );
    MPE_Describe_state(9, 10, "Compute coefs", "magenta:vlines3" );
    MPE_Describe_state(11, 12, "Compute forces", "orange3:gray" );
    MPE_Describe_state(13, 14, "Advance time", "purple:boxes" );
    MPE_Describe_state(15, 16, "Send energies", "blue:dllines4" );
  }
#endif


  //====================================
  // Set signal handler on HUP and TERM
  //====================================

  if (signal(SIGTERM, signal_handler) == SIG_ERR) {
    cerr << endl 
	 << "Process " << myid
	 << ": Error setting signal handler [TERM]" << endl;
    MPI_Abort(MPI_COMM_WORLD, -1);
  }
#ifdef DEBUG
  else {
    cerr << endl 
	 << "Process " << myid
	 << ": SIGTERM error handler set" << endl;
  }
#endif

  if (signal(SIGHUP, signal_handler) == SIG_ERR) {
    cerr << endl 
	 << "Process " << myid
	 << ": Error setting signal handler [HUP]" << endl;
    MPI_Abort(MPI_COMM_WORLD, -1);
  }
#ifdef DEBUG
  else {
    cerr << endl 
	 << "Process " << myid
	 << ": SIGHUP error handler set" << endl;
  }
#endif


  //================
  // Print welcome  
  //================

  if (myid==0) {
    cout << setw(50) << setfill('-') << "-" << setfill(' ') << endl;
    cout << endl << "This is " << PACKAGE << " " << VERSION
	 << " " << version_id << endl << endl;
    cout << setw(50) << setfill('-') << "-" << setfill(' ') << endl;
  }


  //============================
  // Parse command line:        
  // broadcast to all processes 
  //============================

  MPL_parse_args(argc, argv);

  
  //========================
  // Change to desired home 
  // directory              
  //========================

  if (use_cwd) {
				// Get Node 0 working directory 
    if (myid == 0) getcwd(hdbuffer, (size_t)hdbufsize);
    MPI_Bcast(hdbuffer, hdbufsize, MPI_CHAR, 0, MPI_COMM_WORLD);

    homedir.erase(homedir.begin(), homedir.end());
    homedir = hdbuffer;
    if (myid == 0) cout << "Process 0: homedir=" << homedir << "\n";
  }
  
  retdir = chdir(homedir.c_str());
  if (retdir) {
    cerr << "Process " << myid << ": could not change to home directory "
	 << homedir << "\n";
    retdir = 1;
  }
				// For exit if some nodes can't find 
				// their home 
  MPI_Allreduce(&retdir, &retdir0, 1, MPI_INT, MPI_SUM, MPI_COMM_WORLD);
  if (retdir0) {
    MPI_Finalize();
    exit(-1);
  }

  //=======
  // DEBUG 
  //=======
  if (myid) {
    getcwd(hdbuffer, (size_t)hdbufsize);
    cout << "Process " << myid << ": homedir=" << hdbuffer << "\n";
  }


  //================
  // Nice process ? 
  //================

  if (NICE>0) setpriority(PRIO_PROCESS, 0, NICE);


  //==============================================
  // Read in points and initialize expansion grid 
  //==============================================

  begin_run();


  //===========
  // MAIN LOOP 
  //===========

  for (this_step=1; this_step<=nsteps; this_step++) {

    do_step(this_step);
    
    //
    // Synchronize and check for the term signal
    //
    MPI_Bcast(&stop_signal, 1, MPI_UNSIGNED_CHAR, 0, MPI_COMM_WORLD);
    if (stop_signal) {
      cout << "Process " << myid << ": have stop signal\n";
      this_step++; 
      break;
    }

  }


  //===========
  // Finish up 
  //===========

  clean_up();

  return 0;
}


