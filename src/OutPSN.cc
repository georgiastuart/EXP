#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <chrono>

#include "expand.h"
#include <global.H>

#include <AxisymmetricBasis.H>
#include <OutPSN.H>

OutPSN::OutPSN(const YAML::Node& conf) : Output(conf)
{
  initialize();
}

void OutPSN::initialize()
{
  try {
				// Get file name
    if (Output::conf["filename"])
      filename = Output::conf["filename"].as<std::string>();
    else{
      filename.erase();
      filename = outdir + "OUT." + runtag;
    }
    
    if (Output::conf["nint"])
      nint = Output::conf["nint"].as<int>();
    else
      nint = 100;

    if (Output::conf["nbeg"])
      nbeg = Output::conf["nbeg"].as<int>();
    else
      nbeg = 0;

    if (Output::conf["real4"])
      real4 = Output::conf["real4"].as<bool>();
    else
      real4 = false;

    if (Output::conf["timer"])
      timer = Output::conf["timer"].as<bool>();
    else
      timer = false;
  }
  catch (YAML::Exception & error) {
    if (myid==0) std::cout << "Error parsing parameters in OutPSN: "
			   << error.what() << std::endl
			   << std::string(60, '-') << std::endl
			   << "Config node"        << std::endl
			   << std::string(60, '-') << std::endl
			   << conf                 << std::endl
			   << std::string(60, '-') << std::endl;
    MPI_Finalize();
    exit(-1);
  }


				// Determine last file

  if (restart && nbeg==0 && myid==0) {

    for (nbeg=0; nbeg<100000; nbeg++) {

				// Output name
      ostringstream fname;
      fname << filename << "." << setw(5) << setfill('0') << nbeg;

				// See if we can open file
      ifstream in(fname.str().c_str());

      if (!in) {
	cout << "OutPSN: will begin with nbeg=" << nbeg << endl;
	break;
      }
    }
  }
}


void OutPSN::Run(int n, bool last)
{
  if (n % nint && !last && !dump_signal) return;
  if (restart  && n==0  && !dump_signal) return;

  std::chrono::high_resolution_clock::time_point beg, end;
  if (timer) beg = std::chrono::high_resolution_clock::now();
  
  std::ofstream out;
  std::ostringstream fname;

  psdump = n;

  int nOK = 0;

  if (myid==0) {
				// Output name
    fname << filename << "." << setw(5) << setfill('0') << nbeg++;

				// Open file and write master header
    out.open(fname.str());

    if (out.fail()) {
      std::cerr << "OutPSN: can't open file <" << fname.str() 
		<< "> . . . quitting" << std::endl;
      nOK = 1;
    }
				// Used by OutCHKPT to not duplicate a dump
    if (not real4) lastPS = fname.str();

				// Open file and write master header
    if (nOK==0) {
      struct MasterHeader header;
      header.time  = tnow;
      header.ntot  = comp->ntot;
      header.ncomp = comp->ncomp;

      out.write((char *)&header, sizeof(MasterHeader));
    }
  }
  
  MPI_Bcast(&nOK, 1, MPI_INT, 0, MPI_COMM_WORLD);
  if (nOK) {
    MPI_Finalize();
    exit(33);
  }

  for (auto c : comp->components) {
				// Write floats rather than doubles
    c->write_binary(&out, real4);
  }

  if (myid==0) {
    try {
      out.close();
    }
    catch (const ofstream::failure& e) {
      std::cout << "OutPSN: exception closing file <" << fname.str()
		<< ": " << e.what() << std::endl;
    }
  }

  chktimer.mark();

  dump_signal = 0;

  if (timer) {
    end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> intvl = end - beg;
    if (myid==0)
      std::cout << "OutPSN [T=" << tnow << "] timing=" << intvl.count()
		<< std::endl;
  }
}

