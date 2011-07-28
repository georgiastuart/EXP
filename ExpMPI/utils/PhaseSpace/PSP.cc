
#include <PSP.H>

#include <StringTok.H>
extern string trimLeft(const string);
extern string trimRight(const string);


void checkstatus(istream *in)
{
  ios::iostate i = in->rdstate();

  if(i & ios::eofbit) {
    cout << "EOF encountered" << endl;
  }
  else if(i & ios::failbit) {
    cout << "Non-Fatal I/O error" << endl;;
  }  
  else if(i & ios::badbit) {
    cout << "Fatal I/O error" << endl;
  }
}


PSPDump::PSPDump(ifstream *in, bool tipsy, bool verbose)
{
  TIPSY = tipsy;
  VERBOSE = verbose;
  
  int idump = 0;

  while (1) {

    Dump dump;

    dump.pos = in->tellg();
				// Read the header, quit on failure
				// --------------------------------
    try {
      in->read((char *)&dump.header, sizeof(MasterHeader));
    } catch (...) {
      if (VERBOSE) cerr << "Could not read master header for Dump #" << idump
			<< endl;
      break;
    }

    if (!*in) {
      if (VERBOSE) cerr << "End of file (?)\n";
      break;
    }

    bool ok = true;

    for (int i=0; i<dump.header.ncomp; i++) {

      PSPstanza stanza;
      stanza.pos = in->tellg();
      
      ComponentHeader headerC;
      try {
	headerC.read(in);
      } catch (...) {
	cerr << "Error reading component header for time=" 
	     << dump.header.time << " . . . quit reading file" << endl;
	ok = false;
	break;
      }

      if (!*in) {
	cerr << "Error reading component header for time=" 
	     << dump.header.time << " . . . quit reading file ";
	cerr << "(Corrupted file?)" << endl;
	ok = false;
	break;
      }

      stanza.pspos = in->tellg();

				// Parse the info string
				// ---------------------
      StringTok<string> tokens(headerC.info);
      stanza.name = trimLeft(trimRight(tokens(":")));
      stanza.id = trimLeft(trimRight(tokens(":")));
      stanza.cparam = trimLeft(trimRight(tokens(":")));
      stanza.fparam = trimLeft(trimRight(tokens(":")));

				// Check for old style
				// -------------------
      /*
      if (stanza.fparam.size() == 0) {
	stanza.fparam = stanza.cparam;
	stanza.cparam = "";
      }
      */
				// Strip of the tipsy type
      StringTok<string> tipsytype(stanza.name);
      stanza.ttype = trimLeft(trimRight(tipsytype(" ")));
      stanza.nbod = headerC.nbod;
      stanza.niatr = headerC.niatr;
      stanza.ndatr = headerC.ndatr;

      
				// Skip forward to next header
				// ---------------------------
      try {
	in->seekg(headerC.nbod*(8*sizeof(double)             + 
				headerC.niatr*sizeof(int)    +
				headerC.ndatr*sizeof(double)
				), ios::cur);
      } 
      catch(...) {
	cerr << "IO error: can't find next header for time="
	     << dump.header.time << " . . . quit reading file" << endl;
	ok = false;
	break;
      }

      if (!*in) {
	cerr << "IO error: can't find next header for time="
	     << dump.header.time << " . . . quit reading file (corrupted?)" 
	     << endl;
	ok = false;
	break;
      }


      dump.stanzas.push_back(stanza);

      if (TIPSY) {

				// Count up Tipsy types and make
				// linked  lists
				// -----------------------------
	if (!stanza.ttype.compare("gas")) {
	  dump.ngas += stanza.nbod;
	  dump.ntot += stanza.nbod;
	  dump.gas.push_back(stanza);
	}
	if (!stanza.ttype.compare("dark")) {
	  dump.ndark += stanza.nbod;
	  dump.ntot += stanza.nbod;
	  dump.dark.push_back(stanza);
	}
	if (!stanza.ttype.compare("star")) {
	  dump.nstar += stanza.nbod;
	  dump.ntot += stanza.nbod;
	  dump.star.push_back(stanza);
	}
      }

    }
    
    if (!ok) break;

    if (VERBOSE) {
      cerr << "Committing Dump #" << idump << " at Time=" << dump.header.time
	   << ", #N=" << dump.header.ntot
	   << ", #C=" << dump.header.ncomp
	   << endl;
    }
    dumps.push_back(dump);
    idump++;
  }

  if (VERBOSE) {
    cerr << "Cached info fields for " << dumps.size() << endl;
    cerr << "     Initial time=" << dumps.begin()->header.time << endl;
    sdump = dumps.end();
    sdump--;
    cerr << "       Final time=" << sdump->header.time << endl;
  }
  fid = &(*dumps.begin());

}

double PSPDump::SetTime(double time) 
{
  list<Dump>::iterator it;
  double tdif = 1.0e30;

  for (it=dumps.begin(); it!=dumps.end(); it++) {

    if (fabs(time - it->header.time) < tdif) {
      fid = &(*it);
      tdif = fabs(time-it->header.time);
    }
  }
  
  return fid->header.time;
}


void PSPDump::PrintSummary(ifstream *in, ostream &out, bool stats, bool timeonly)
{
  list<Dump>::iterator itd;

  for (itd = dumps.begin(); itd != dumps.end(); itd++) {

    out << "Time=" << itd->header.time << "   [" << itd->pos << "]" << endl;
    if (!timeonly) {
      out << "   Total particle number: " << itd->header.ntot  << endl;
      out << "   Number of components:  " << itd->header.ncomp << endl;
      if (TIPSY) {
	out << "          Gas particles:  " << itd->ngas << endl;
	out << "         Dark particles:  " << itd->ndark << endl;
	out << "         Star particles:  " << itd->nstar << endl;
      }

      int cnt=1;

      for (spos = itd->stanzas.begin(); spos != itd->stanzas.end(); spos++) {
	
				// Print the info for this stanza
				// ------------------------------
	out << setw(60) << setfill('-') << "-" << endl << setfill(' ');
	out << "--- Component #" << setw(2) << cnt++ << endl;
	out << setw(20) << " name :: "  << spos->name   << endl
	    << setw(20) << " id :: "    << spos->id     << endl
	    << setw(20) << " cparam :: " << spos->cparam << endl
	    << setw(20) << " fparam :: " << spos->fparam << endl;
	if (TIPSY) out << setw(20) << " tipsy :: " << spos->ttype  << endl;
	out << setw(20) << " nbod :: "  << spos->nbod  << endl
	    << setw(20) << " niatr :: " << spos->niatr << endl
	    << setw(20) << " ndatr :: " << spos->ndatr << endl;
	if (stats) {
	  ComputeStats(in);
	  out << endl<< setw(20) << "*** Position" 
	      << setw(15) << "X" << setw(15) << "Y" << setw(15) << "Z"
	      << endl;
	  out << setw(20) << "Min :: ";
	  for (unsigned k=0; k<3; k++) out << setw(15) << pmin[k];
	  out << endl;
	  out << setw(20) << "Med :: ";
	  for (unsigned k=0; k<3; k++) out << setw(15) << pmed[k];
	  out << endl;
	  out << setw(20) << "Max :: ";
	  for (unsigned k=0; k<3; k++) out << setw(15) << pmax[k];
	  out << endl;
	  out << endl << setw(20) << "*** Velocity"
	      << setw(15) << "U" << setw(15) << "V" << setw(15) << "W"
	      << endl;
	  out << setw(20) << "Min :: ";
	  for (unsigned k=0; k<3; k++) out << setw(15) << vmin[k];
	  out << endl;
	  out << setw(20) << "Med :: ";
	  for (unsigned k=0; k<3; k++) out << setw(15) << vmed[k];
	  out << endl;
	  out << setw(20) << "Max :: ";
	  for (unsigned k=0; k<3; k++) out << setw(15) << vmax[k];
	  out << endl;
	}      
      }
    }
  }

}
  
void PSPDump::PrintSummaryCurrent(ifstream *in, ostream &out, bool stats, bool timeonly)
{
  out << "Time=" << fid->header.time << "   [" << fid->pos << "]" << endl;
  if (!timeonly) {
    out << "   Total particle number: " << fid->header.ntot  << endl;
    out << "   Number of components:  " << fid->header.ncomp << endl;
    if (TIPSY) {
      out << "          Gas particles:  " << fid->ngas << endl;
      out << "         Dark particles:  " << fid->ndark << endl;
      out << "         Star particles:  " << fid->nstar << endl;
    }
    
    int cnt=1;

    for (spos = fid->stanzas.begin(); spos != fid->stanzas.end(); spos++) {
	
      // Print the info for this stanza
      // ------------------------------
      out << setw(60) << setfill('-') << "-" << endl << setfill(' ');
      out << "--- Component #" << setw(2) << cnt++ << endl;
      out << setw(20) << " name :: "  << spos->name   << endl
	  << setw(20) << " id :: "    << spos->id     << endl
	  << setw(20) << " cparam :: " << spos->cparam  << endl
	  << setw(20) << " fparam :: " << spos->fparam  << endl;
      if (TIPSY) out << setw(20) << " tipsy :: " << spos->ttype  << endl;
      out << setw(20) << " nbod :: "  << spos->nbod  << endl
	  << setw(20) << " niatr :: " << spos->niatr << endl
	  << setw(20) << " ndatr :: " << spos->ndatr << endl;
      out << setw(60) << setfill('-') << "-" << endl << setfill(' ');
	if (stats) {
	  ComputeStats(in);
	  out << endl << setw(20) << "*** Position" 
	      << setw(15) << "X" << setw(15) << "Y" << setw(15) << "Z"
	      << endl;
	  out << setw(20) << "Min :: ";
	  for (unsigned k=0; k<3; k++) out << setw(15) << pmin[k];
	  out << endl;
	  out << setw(20) << "Med :: ";
	  for (unsigned k=0; k<3; k++) out << setw(15) << pmed[k];
	  out << endl;
	  out << setw(20) << "Max :: ";
	  for (unsigned k=0; k<3; k++) out << setw(15) << pmax[k];
	  out << endl;
	  out << endl << setw(20) << "*** Velocity"
 	      << setw(15) << "U" << setw(15) << "Vn" << setw(15) << "W"
	      << endl;
	  out << setw(20) << "Min :: ";
	  for (unsigned k=0; k<3; k++) out << setw(15) << vmin[k];
	  out << endl;
	  out << setw(20) << "Med :: ";
	  for (unsigned k=0; k<3; k++) out << setw(15) << vmed[k];
	  out << endl;
	  out << setw(20) << "Max :: ";
	  for (unsigned k=0; k<3; k++) out << setw(15) << vmax[k];
	  out << endl;
	}      
    }
  }
}

Dump* PSPDump::GetDump()
{
  sdump = dumps.begin();
  fid = &(*sdump);
  if (sdump != dumps.end()) 
    return fid;
  else 
    return 0;
}  

Dump* PSPDump::NextDump()
{
  sdump++;
  fid = &(*sdump);
  if (sdump != dumps.end()) 
    return fid;
  else 
    return 0;
}


PSPstanza* PSPDump::GetStanza()
{
  spos = fid->stanzas.begin();
  cur = &(*spos);
  if (spos != fid->stanzas.end()) 
    return cur;
  else 
    return 0;
}

PSPstanza* PSPDump::NextStanza()
{
  spos++;
  cur = &(*spos);
  if (spos != fid->stanzas.end()) 
    return cur;
  else 
    return 0;
}

SParticle *PSPDump::GetParticle(istream* in)
{
				// Position to beginning of particles
  in->seekg(spos->pspos);
  pcount = 0;
				// Clear particle
  part.iatr.erase(part.iatr.begin(), part.iatr.end()); 
  if (spos->niatr) part.iatr = vector<int>(spos->niatr);

  part.datr.erase(part.datr.begin(), part.datr.end()); 
  if (spos->ndatr) part.datr = vector<double>(spos->ndatr);

  return NextParticle(in);
}

SParticle *PSPDump::NextParticle(istream* in)
{
  checkstatus(in);		// DEBUG

				// Read partcle
  if (pcount < spos->nbod) {
    in->read((char *)&part.mass, sizeof(double));
    for (int i=0; i<3; i++) in->read((char *)&part.pos[i], sizeof(double));
    for (int i=0; i<3; i++) in->read((char *)&part.vel[i], sizeof(double));
    in->read((char *)&part.phi, sizeof(double));
    for (int i=0; i<spos->niatr; i++) in->read((char *)&part.iatr[i], sizeof(int));
    for (int i=0; i<spos->ndatr; i++) in->read((char *)&part.datr[i], sizeof(double));

    pcount++;

    return &part;

  } else
    return 0;
  
}

PSPstanza* PSPDump::GetGas()
{
  spos = fid->gas.begin();
  cur = &(*spos);
  if (spos != fid->gas.end()) 
    return cur;
  else 
    return 0;
}

PSPstanza* PSPDump::NextGas()
{
  spos++;
  cur = &(*spos);
  if (spos != fid->gas.end()) 
    return cur;
  else 
    return 0;
}

PSPstanza* PSPDump::GetDark()
{
  spos = fid->dark.begin();
  cur = &(*spos);
  if (spos != fid->dark.end()) 
    return cur;
  else 
    return 0;
}

PSPstanza* PSPDump::NextDark()
{
  spos++;
  cur = &(*spos);
  if (spos != fid->dark.end()) 
    return cur;
  else 
    return 0;
}

PSPstanza* PSPDump::GetStar()
{
  spos = fid->star.begin();
  cur = &(*spos);
  if (spos != fid->star.end()) 
    return cur;
  else 
    return 0;
}

PSPstanza* PSPDump::NextStar()
{
  spos++;
  cur = &(*spos);
  if (spos != fid->star.end()) 
    return cur;
  else 
    return 0;
}

void PSPDump::ComputeStats(istream *in)
{
  cur = &(*spos);

  // Initialize lists
  vector< vector<float> > plist(3, vector<float>(spos->nbod) );
  vector< vector<float> > vlist(3, vector<float>(spos->nbod) );
  mtot = 0.0;
  
  SParticle *P = GetParticle(in);
  unsigned n=0;
  while (P) {
    mtot += P->mass;
    for (unsigned k=0; k<3; k++) {
      plist[k][n] = P->pos[k];
      vlist[k][n] = P->vel[k];
    }
    P = NextParticle(in);
    n++;
  }

  pmin = vector<float>(3);
  pmed = vector<float>(3);
  pmax = vector<float>(3);

  vmin = vector<float>(3);
  vmed = vector<float>(3);
  vmax = vector<float>(3);

  for (unsigned k=0; k<3; k++) {
    std::sort(plist[k].begin(), plist[k].end());
    pmin[k] = plist[k].front();
    pmed[k] = plist[k][floor(0.5*spos->nbod+0.5)];
    pmax[k] = plist[k].back();
    std::sort(vlist[k].begin(), vlist[k].end());
    vmin[k] = vlist[k].front();
    vmed[k] = vlist[k][floor(0.5*spos->nbod+0.5)];
    vmax[k] = vlist[k].back();
  }
}

