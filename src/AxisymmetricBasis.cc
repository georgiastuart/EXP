#include <limits>
#include "expand.h"
#include <AxisymmetricBasis.H>
#include <VtkPCA.H>

AxisymmetricBasis:: AxisymmetricBasis(const YAML::Node& conf) : Basis(conf) 
{
  Lmax      = 4;
  nmax      = 10;
  dof       = 3;
  npca      = 500;
  pca       = false;
  pcadiag   = false;
  pcavtk    = false;
  vtkfreq   = 1;
  tksmooth  = 3.0;
  tkcum     = 0.95;
  tk_type   = Null;
  sampT     = 0;

  string val;

  try {
    if (conf["Lmax"])     Lmax       = conf["Lmax"].as<int>();
    if (conf["nmax"])     nmax       = conf["nmax"].as<int>();
    if (conf["dof"])      dof        = conf["dof"].as<int>();
    if (conf["npca"])     npca       = conf["npca"].as<int>();
    if (conf["selector"]) pca        = conf["selector"].as<bool>();
    if (conf["pca"])      pca        = conf["pca"].as<bool>();
    if (conf["pcadiag"])  pcadiag    = conf["pcadiag"].as<bool>();
    if (conf["pcavtk"])   pcavtk     = conf["pcavtk"].as<bool>();
    if (conf["vtkfreq"])  vtkfreq    = conf["vtkfreq"].as<int>();
    if (conf["tksmooth"]) tksmooth   = conf["tksmooth"].as<double>();
    if (conf["tkcum"])    tkcum      = conf["tkcum"].as<double>();
    if (conf["tk_type"])  tk_type    = setTK(conf["tk_type"].as<std::string>());
  }
  catch (YAML::Exception & error) {
    if (myid==0) std::cout << "Error parsing parameters in AxisymmetricBasis: "
			   << error.what() << std::endl
			   << std::string(60, '-') << std::endl
			   << "Config node"        << std::endl
			   << std::string(60, '-') << std::endl
			   << conf                 << std::endl
			   << std::string(60, '-') << std::endl;

    MPI_Finalize();
    exit(-1);
  }


  sqnorm.setsize(0, Lmax, 1, nmax);
  for (int l=0; l<=Lmax; l++)
    for (int n=1; n<=nmax; n++) sqnorm[l][n] = 1.0;

  if (pca) {

    if (dof==3)
      Ldim = (Lmax + 1)*(Lmax + 2)/2;
    else
      Ldim = Lmax + 1;
    
    weight = new Vector [Ldim];
    b_Hall = new Vector [Ldim];
    evec   = new Matrix [Ldim];
    Tevec  = new Matrix [Ldim];
    
    for (int l=0; l<Ldim; l++) {
      weight[l].setsize(1, nmax);
      b_Hall[l].setsize(1, nmax);
      evec  [l].setsize(1, nmax, 1, nmax);
      Tevec [l].setsize(1, nmax, 1, nmax);
    }

    smth.setsize(1, nmax);
    inv .setsize(1, nmax);
    eval.setsize(1, nmax);
    cuml.setsize(1, nmax);
    
    covar .setsize(1, nmax, 1, nmax);
    sqnorm.setsize(0, Lmax, 1, nmax);
      
    for (int l=0; l<=Lmax; l++)
      for (int n=1; n<=nmax; n++) sqnorm[l][n] = 1.0;

    if (myid==0) {

      const string types[] = {
	"Hall", 
	"VarianceCut", 
	"CumulativeCut",
	"VarianceWeighted", 
	"Null"};

      const string desc[] = {
	"Tapered signal-to-noise power defined by Hall",
	"Cut all coefficients below some S/N level",
	"Cut coefficients below some cumulative fraction",
	"Weight coefficients be S/N for S/N<1",
	"Compute the S/N but do not modify coefficients"};

      cout << "AxisymmetricBasis: using PCA type: " << types[tk_type] 
	   << "====>" << desc[tk_type] << endl;
    }
  }
}

AxisymmetricBasis::~AxisymmetricBasis()
{
  vector<Matrix *>::iterator it;
  for (auto it : expcoefN) delete it;
  for (auto it : expcoefL) delete it;

  if (pca) {
    delete [] weight;
    delete [] b_Hall;
    delete [] evec;
    delete [] Tevec;
  }
}


void AxisymmetricBasis::pca_hall(bool compute)
{
  if (muse <= 0.0) return;

  std::ofstream out;		// PCA diag output
  std::ofstream cof;		// PCA diag output

  if (pcadiag and myid==0 and compute) {

    // Open the diag file
    ostringstream sout1, sout2;
    sout1 << runtag << ".pcadiag." << cC->id << "." << cC->name << ".pcalog";
    sout2 << runtag << ".pcadiag." << cC->id << "." << cC->name << ".pcamat";

    out.open(sout1.str().c_str(), ios::out | ios::app);
    cof.open(sout2.str().c_str(), ios::out | ios::app);

    if (out) {
      out << "#" << endl;
      out << "# Time=" << tnow << endl;
      out << "#" << endl;
      if (dof==3) out << right << "# " << setw(3) << "l";
      out << setw(5)  << "m" << setw(5) << "n";
      out << setw(18) << "coef"
	  << setw(18) << "|coef|^2"
	  << setw(18) << "var(coef)"
	  << setw(18) << "cum var"
	  << setw(18) << "S/N"
	  << setw(18) << "B_Hall"
	  << endl;
    } else {
      cout << "AxisymmetricBasis::pca_hall: could not open output file <"
	   << sout1.str() << ">" << endl
	   << "AxisymmetricBasis::pca_hall: continuing" << endl;
    }

    if (cof.good()) {
      cof << "#" << endl << std::right
	  << "# Time = " << tnow << endl
	  << "#" << endl << setprecision(4);
    } else {
      cout << "AxisymmetricBasis::pca_hall: could not open output file <"
	   << sout2.str() << ">" << endl
	   << "AxisymmetricBasis::pca_hall: continuing" << endl;
    }

  } // END: pcadiag file initialization

  int indx=0, indxC=0;

  if (compute) {

    VtkPCAptr vtkpca;

    static unsigned ocount = 0;

    if (pcavtk and myid==0) {

      if (ocount==0) {		// Look for restart position.  This is
	while (1) {	      // time consuming but is only done once.
	  std::ostringstream fileN;
	  fileN << runtag << "_pca_" << cC->id << "_" << cC->name
		<< "_" << std::setfill('0') << std::setw(5) << ocount;
	  std::ifstream infile(fileN.str());
	  if (not infile.good()) break;
	  ocount++;
	}
	if (ocount)
	  std::cout << "Restart in AxisymmetricBasis::pca_hall: "
		    << "vtk output will begin at "
		    << ocount << std::endl;
      }
      
      if (compute and ocount % vtkfreq==0) {
	vtkpca = VtkPCAptr(new VtkPCA(nmax));
      }
    }

    if (dof==3) {
      L0    = 0;
      fac02 = 16.0*M_PI*M_PI;
    } else {
      L0    = Lmax;
      fac02 = 1.0;
    }

    double fac, var, b;

				// For PCA jack knife
    Vector evalJK, cumlJK, snrval;
    Vector meanJK;
    Matrix covrJK;
    Matrix evecJK;
    double Tmass = 0.0;
    
    covrJK.setsize(1, nmax, 1, nmax);
    meanJK.setsize(1, nmax);
    evecJK.setsize(1, nmax, 1, nmax);

    std::vector<double> meanJK1(nmax), meanJK2(nmax);

    for (auto v : massT) Tmass += v;

    for (int l=L0, loffset=0, loffC=0; l<=Lmax; loffset+=(2*l+1), loffC+=(l+1), l++) {

      for (int m=0, moffset=0; m<=l; m++) {
      
	if (dof==3) {
	  indx  = loffset + moffset;
	  indxC = loffC + m;
	}
	else {
	  indx  = moffset;
	  indxC = m;
	}
	
	covrJK.zero();
	meanJK.zero();
	  
	std::fill(meanJK1.begin(), meanJK1.end(), 0.0);
	std::fill(meanJK2.begin(), meanJK2.end(), 0.0);

	// Compute mean and variance
	//
	for (unsigned T=0; T<sampT; T++) {

	  for (int i=1; i<=nmax; i++) {
	    double modi =
	      (*expcoefT[T])[indx][i] * (*expcoefT[T])[indx][i];
	    if (m)
	      modi += (*expcoefT[T])[indx+1][i] * (*expcoefT[T])[indx+1][i] ;
	    
	    modi = sqrt(modi);
	    
	    meanJK[i] += modi;

	    meanJK1[i-1] += (*expcoefT[T])[indx][i];
	    if (m) meanJK2[i-1] += (*expcoefT[T])[indx+1][i];

	    for (int j=1; j<=nmax; j++) {
	      double modj =
		(*expcoefT[T])[indx][j] * (*expcoefT[T])[indx][j];
	      if (m) 
		modj += (*expcoefT[T])[indx+1][j] * (*expcoefT[T])[indx+1][j] ;

	      modj = sqrt(modj);
	      
	      covrJK[i][j] += modi * modj * sampT;
	    }
	  }
	}
	
	for (int i=1; i<=nmax; i++) {
	  for (int j=1; j<=nmax; j++) {
	    covrJK[i][j] -= meanJK[i]*meanJK[j];
	  }
	}
#ifdef GHQL
	evalJK = covrJK.Symmetric_Eigenvalues_GHQL(evecJK);
#else
	evalJK = covrJK.Symmetric_Eigenvalues(evecJK);
#endif
	evec [indxC] = evecJK;
	Tevec[indxC] = evecJK.Transpose();

	// Transformation output
	//
	if (cof.good()) {
	  cof << "#" << std::endl
	      << "# l=" << l << " m=" << m << std::endl
	      << "#" << std::endl;
	  for (int i=1; i<=nmax; i++) {
	    for (int j=1; j<=nmax; j++) {
	      cof << std::setw(12) << Tevec[indxC][i][j];
	    }
	    cof << std::endl;
	  }
	}

	// Cumulative distribution
	//
	cumlJK = evalJK;
	for (int n=2; n<=nmax; n++) cumlJK[n] += cumlJK[n-1];
	for (int n=1; n<=nmax; n++) cumlJK[n] /= cumlJK[nmax];
	
	// SNR vector
	//
	snrval.setsize(cumlJK.getlow(), cumlJK.gethigh());
	
	if (out) out << endl;

	Vector tt = Tevec[indxC] * meanJK;
	if (myid==0) {
	  for (int n=1; n<=nmax; n++) {
	    std::cout   << std::setw(3)  << l
		        << std::setw(3)  << m
			<< std::setw(3)  << n
			<< std::setw(16) << expcoef[indx][n]
			<< std::setw(16) << meanJK1[n-1];
	    if (m)
	      std::cout << std::setw(16) << expcoef[indx][n]
			<< std::setw(16) << meanJK2[n-1];
	    std::cout   << std::endl;
	  }
	}

	for (int n=1; n<=nmax; n++) {
	  
	  var = evalJK[n]/sampT;
	  //               ^
	  //               |
	  //               +--------- bootstrap variance estimate for
	  //                          population variance
	  
	  b = var/(tt[n]*tt[n]);
	  b = std::max<double>(b, std::numeric_limits<double>::min());
	  b_Hall[indxC][n] = 1.0/(1.0 + b);
	  snrval[n] = sqrt(1.0/b);

	  if (out) {
	    if (dof==3) out << setw(5) << l;
	    out << setw(5)  << m << setw(5) << n;
	  
	    if (var>0.0)
	      out << setw(18) << tt[n]
		  << setw(18) << tt[n]*tt[n]
		  << setw(18) << var
		  << setw(18) << cumlJK[n]
		  << setw(18) << fabs(tt[n])/sqrt(var)
		  << setw(18) << b_Hall[indxC][n];
	    else
	      out << setw(18) << tt[n]
		  << setw(18) << tt[n]*tt[n]
		  << setw(18) << var
		  << setw(18) << cumlJK[n]
		  << setw(18) << "***"
		  << setw(18) << "***";
	    out << endl;
	  }
	  
	  if (tk_type == VarianceCut) {
	  
	    if (tksmooth*var > tt[n]*tt[n])
	      weight[indxC][n] = 0.0;
	    else
	      weight[indxC][n] = 1.0;
	    
	  }
	  else if (tk_type == CumulativeCut) {
	  
	    if (n==1 || cuml[n] <= tkcum)
	      weight[indxC][n] = 1.0;
	    else
	      weight[indxC][n] = 0.0;
	  
	  }
	  else if (tk_type == VarianceWeighted) {
	  
	    weight[indxC][n] = 1.0/(1.0 + var/(tt[n]*tt[n] + 1.0e-14));
	  
	  }
	  else
	    weight[indxC][n] = 1.0;
	  
	}

	if (vtkpca and myid==0) {
	  if (dof==3)
	    vtkpca->Add(meanJK, b_Hall[indxC], snrval, evalJK, evecJK.Transpose(), covrJK, l, m);
	  else
	    vtkpca->Add(meanJK, b_Hall[indxC], snrval, evalJK, evecJK.Transpose(), covrJK, m);
	}

      }

      if (vtkpca) {
	std::ostringstream sout;
	
	sout << runtag << "_pca_" << cC->id << "_" << cC->name
	     << "_" << std::setfill('0') << std::setw(5) << ocount++;
	vtkpca->Write(sout.str());
      }
    }
  }


  for (int l=L0, loffset=0, loffC=0; l<=Lmax; loffset+=(2*l+1), loffC+=(l+1), l++) {
    
    for (int m=0, moffset=0; m<=l; m++) {
      
      if (dof==3) {
	indx  = loffset + moffset;
	indxC = loffC + m;
      } else {
	indx  = moffset;
	indxC = m;
      }

      // Cosine terms
      //
      for (int n=1; n<=nmax; n++) {
	double dd = 0.0;
	for (int nn=1; nn<=nmax; nn++) 
	  dd += Tevec[indxC][n][nn]*expcoef[indx][nn];
	smth[n] = dd * weight[indxC][n];
      }
    
      inv = evec[indxC] * smth;
      for (int n=1; n<=nmax; n++) {
	if (tk_type != Null) expcoef[indx][n]  = inv[n];
	if (tk_type == Hall) expcoef[indx][n] *= b_Hall[indxC][n];
      }
  
      moffset++;

      // Sine terms
      //
      if (m) {
	for (int n=1; n<=nmax; n++) {
	  double dd = 0.0;
	  for (int nn=1; nn<=nmax; nn++) 
	    dd += Tevec[indxC][n][nn]*expcoef[indx+1][nn];
	  smth[n] = dd * weight[indxC][n];
	}
	
	inv = evec[indxC] * smth;
	for (int n=1; n<=nmax; n++) {
	  if (tk_type != Null) expcoef[indx+1][n]  = inv[n];
	  if (tk_type == Hall) expcoef[indx+1][n] *= b_Hall[indxC][n];
	}
	
	moffset++;
      }
    }
  }
}

void AxisymmetricBasis::parallel_gather_coefficients(void)
{

  if (myid == 0) {

    for (int l=L0, loffset=0; l<=Lmax; loffset+=(2*l+1), l++) {

      for (int m=0, moffset=0; m<=l; m++) {

	if (m==0) {
	  for (int n=1; n<=nmax; ++n) {
	    expcoef[loffset+moffset][n] = 0.0;
	  }
	  moffset++;

	} else {
	  for (int n=1; n<=nmax; ++n) {
	    expcoef[loffset+moffset][n] = 0.0;
	    expcoef[loffset+moffset+1][n] = 0.0;
	  }
	  moffset+=2;
	}
      }
    }
  }


  for (int l=L0, loffset=0; l<=Lmax; loffset+=(2*l+1), l++) {

    for (int m=0, moffset=0; m<=l; m++) {

      if (m==0) {
	MPI_Reduce(&expcoef1[loffset+moffset][1], 
		   &expcoef [loffset+moffset][1], nmax, 
		   MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
	moffset++;
      }
      else {
	MPI_Reduce(&expcoef1[loffset+moffset][1], 
		   &expcoef [loffset+moffset][1], nmax, 
		   MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

	MPI_Reduce(&expcoef1[loffset+moffset+1][1],
		   &expcoef [loffset+moffset+1][1], nmax, 
		   MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
	
	moffset+=2;
      }
    }
  }

}

void AxisymmetricBasis::parallel_distribute_coefficients(void)
{

  for (int l=L0, loffset=0; l<=Lmax; loffset+=(2*l+1), l++) {

      for (int m=0, moffset=0; m<=l; m++) {

	if (m==0) {
	  MPI_Bcast(&expcoef[loffset+moffset][1], nmax, MPI_DOUBLE,
		    0, MPI_COMM_WORLD);
	  moffset++;
	}
	else {
	  MPI_Bcast(&expcoef[loffset+moffset][1], nmax, MPI_DOUBLE,
		     0, MPI_COMM_WORLD);
	  MPI_Bcast(&expcoef[loffset+moffset+1][1], nmax, MPI_DOUBLE,
		    0, MPI_COMM_WORLD);
	  moffset+=2;
	}
      }
  }

}


void AxisymmetricBasis::parallel_gather_coef2(void)
{
  MPI_Allreduce(&massT1[0], &massT[0], sampT,
		  MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);

  for (unsigned T=0; T<sampT; T++) {
    for (int l=0; l<=Lmax*(Lmax+2); l++) {
      MPI_Allreduce(&(*expcoefT1[T])[l][1],
		    &(*expcoefT [T])[l][1], nmax,
		    MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
    }
  }
}

AxisymmetricBasis::TKType AxisymmetricBasis::setTK(const std::string& tk)
{
  TKType ret = Null;

  if      (tk == "Hall")             ret = Hall;
  else if (tk == "VarianceCut")      ret = VarianceCut;
  else if (tk == "CumulativeCut")    ret = CumulativeCut;
  else if (tk == "VarianceWeighted") ret = VarianceWeighted;
  else if (tk == "Null")             ret = Null;
  else {
    if (myid==0) {
      cout << "AxisymmetricBasis: no such TK type <" << tk << ">"
	   << " using Null type\n";
    }
  }

  return ret;
}
