#include <Elastic.H>

#include <boost/assign/list_of.hpp>

// Atomic radii in picometers from Clementi, E.; Raimond, D. L.;
// Reinhardt, W. P. (1967). "Atomic Screening Constants from SCF
// Functions. II. Atoms with 37 to 86 Electrons". Journal of Chemical
// Physics 47 (4): 1300-1307.  See also Paper 1, ref. therein.
//
// Z     radius
const Geometric::radData atomic_radii = boost::assign::map_list_of
  (1     , 53    )
  (2     , 31    )
  (3     , 167   )
  (4     , 112   )
  (5     , 87    )
  (6     , 67    )
  (7     , 56    )
  (8     , 48    )
  (9     , 42    )
  (10    , 38    )
  (11    , 190   )
  (12    , 145   )
  (13    , 118   )
  (14    , 111   )
  (15    , 98    )
  (16    , 180   )
  (17    , 79    )
  (18    , 188   )
  (19    , 243   )
  (20    , 194   )
  (21    , 184   )
  (22    , 176   )
  (23    , 171   )
  (24    , 166   )
  (25    , 161   )
  (26    , 156   )
  (27    , 152   )
  (28    , 149   )
  (29    , 145   )
  (30    , 152   )
  (31    , 136   )
  (32    , 125   )
  (33    , 114   )
  (34    , 103   )
  (35    , 94    )
  (36    , 88    )
  (37    , 265   )
  (38    , 219   )
  (39    , 212   )
  (40    , 206   )
  (41    , 198   )
  (42    , 190   )
  (43    , 183   )
  (44    , 178   )
  (45    , 173   )
  (46    , 169   )
  (47    , 172   )
  (48    , 161   )
  (49    , 193   )
  (50    , 217   )
  (51    , 133   )
  (52    , 123   )
  (53    , 198   )
  (54    , 108   )
  (55    , 298   )
  (56    , 268   )
  (59    , 247   )
  (60    , 206   )
  (61    , 205   )
  (62    , 238   )
  (63    , 231   )
  (64    , 233   )
  (65    , 225   )
  (66    , 228   )
  (68    , 226   )
  (69    , 222   )
  (70    , 222   )
  (71    , 217   )
  (72    , 208   )
  (73    , 200   )
  (74    , 193   )
  (75    , 188   )
  (76    , 185   )
  (77    , 180   )
  (78    , 177   )
  (79    , 166   )
  (80    , 171   )
  (81    , 156   )
  (82    , 202   )
  (83    , 143   )
  (84    , 135   )
  (86    , 120   );


// Total cross section from Malik & Trefftz, 1960, Zeitschrift fur Astrophysik, 50, 96-109

// Column 1 in eV, Column2 in Bohr cross section (pi*a_0^2) units

const Elastic::InterpPair Hydrogen = boost::assign::map_list_of
  (0.66356077923727  , 28.864    )
  (0.66576762346358  , 29.5088   )
  (0.71282701193426  , 28.2574   )
  (0.73590227585419  , 27.4989   )
  (0.75936666273882  , 26.8542   )
  (0.80889140369906  , 26.3234   )
  (0.83209592175062  , 25.6028   )
  (0.90677351672548  , 24.9204   )
  (0.95590913472103  , 24.2757   )
  (1.031106467362    , 23.745    )
  (1.05457085424663  , 23.1003   )
  (1.10409559520687  , 22.5694   )
  (1.17903305901478  , 21.9628   )
  (1.22881766880808  , 21.5079   )
  (1.2782131556367   , 20.9391   )
  (1.35379961124236  , 20.5221   )
  (1.45506137966837  , 20.1052   )
  (1.58185287994542  , 19.6504   )
  (1.75999228472356  , 19.1958   )
  (1.91233528596856  , 18.7032   )
  (2.06519530374007  , 18.3623   )
  (2.24360682247953  , 17.9835   )
  (2.42186867854026  , 17.5668   )
  (2.60026659158166  , 17.188    )
  (2.77893661858437  , 16.8851   )
  (2.9830084838763   , 16.5064   )
  (3.21287675270137  , 16.1657   )
  (3.39141072272342  , 15.8249   )
  (3.64683049251644  , 15.4463   )
  (3.87695726960477  , 15.1815   )
  (4.0810291348967   , 14.8028   )
  (4.31091100941984  , 14.4621   )
  (4.54091533522557  , 14.1593   )
  (4.71957175653021  , 13.8564   )
  (4.97525003458648  , 13.5537   )
  (5.28200410318252  , 13.1753   )
  (5.53768238123879  , 12.8726   )
  (5.74227126305723  , 12.6456   )
  (5.97267015410688  , 12.4566   )
  (6.15132657541152  , 12.1537   )
  (6.40726336173105  , 11.9268   )
  (6.61198830053015  , 11.7378   )
  (6.81683569061185  , 11.5866   )
  (6.99562816889715  , 11.3216   )
  (7.20035310769626  , 11.1326   )
  (7.43061594176524  , 10.9057   )
  (7.71209062335465  , 10.641    )
  (7.96789135269351  , 10.3762   )
  (8.27517604351412  , 10.1495   )
  (8.50530282060245  , 9.88464   )
  (8.76123960692198  , 9.6578    )
  (9.06852429774258  , 9.43109   )
  (9.35012143061459  , 9.20432   )
  (9.55484636941369  , 9.01526   )
  (9.78536771174593  , 8.86419   )
  (10.0157666027956  , 8.6752    )
  (10.2717033891151  , 8.44835   )
  (10.5533005219871  , 8.22158   )
  (10.8349112605572  , 7.9948    )
  (11.1421823456797  , 7.7681    )
  (11.4237930842498  , 7.54133   )
  (11.6798523218519  , 7.35241   )
  (11.9615991174026  , 7.16356   )
  (12.2176583550047  , 6.97464   )
  (12.4223832938038  , 6.78558   )
  (12.7041164836565  , 6.59673   )
  (12.9858496735092  , 6.40788   )
  (13.2163710158414  , 6.25682   )
  (13.4212320116212  , 6.10568   )
  (13.600541506433   , 5.9924    );


// Total cross section from LaBahn & Callaway, 1966, Phys. Rev., 147, 50, 96-109

const Elastic::InterpPair Helium = boost::assign::map_list_of
  (0.0972135 , 62.2773619684373)
  (0.212908  , 65.3193661349083)
  (0.251768  , 66.6419766420696)
  (0.440878  , 67.8332685763108)
  (0.704798  , 68.6287198361997)
  (1.11846   , 68.7641224795695)
  (1.5694    , 68.5696578943123)
  (1.86971   , 68.109100411296 )
  (2.20762   , 67.6491712468105)
  (2.50774   , 66.9903792673527)
  (2.77027   , 66.3312731286295)
  (3.07045   , 65.7387687541625)
  (3.25779   , 65.0790342969086)
  (3.44532   , 64.6178484953617)
  (3.78297   , 63.8930830701785)
  (4.0079    , 63.2339769314554)
  (4.2329    , 62.6405300791922)
  (4.57055   , 61.9160788132744)
  (4.79555   , 61.3229461202767)
  (5.02048   , 60.6635258222882)
  (5.20782   , 60.0041055242997)
  (5.50788   , 59.2790259398512)
  (5.883     , 58.4226277824826)
  (6.14552   , 57.7635216437595)
  (6.48318   , 57.0390703778416)
  (6.89576   , 56.0507253290223)
  (7.27088   , 55.1943271716537)
  (7.68347   , 54.2059821228344)
  (7.98353   , 53.4809025383858)
  (8.32118   , 52.756451272468)
  (8.6963    , 51.9000531150995)
  (8.99617   , 50.9767390342094)
  (9.33408   , 50.5168098697239)
  (9.67173   , 49.7920444445407)
  (9.97191   , 49.1995400700737)
  (10.2346   , 48.6726949820667)
  (10.4596   , 48.0795622890689)
  (10.7222   , 47.5527172010619)
  (10.9849   , 47.0921597180456)
  (11.2852   , 46.565628789304 )
  (11.5478   , 46.038783701297 )
  (11.773    , 45.57759789975  )
  (12.0731   , 44.9191200795576)
  (12.3734   , 44.4585625965413)
  (12.7488   , 43.866686540605 )
  (13.0489   , 43.2738680068726)
  (13.3867   , 42.6153901866802)
  (13.7996   , 41.9578548442838)
  (14.1375   , 41.4976115205329)
  (14.4752   , 40.9054213053313)
  (14.8132   , 40.5114655865711)
  (15.1509   , 39.8529877663787)
  (15.4889   , 39.4590320476185)
  (15.7893   , 39.064762169593 )
  (16.1271   , 38.5385454001167)
  (16.465    , 38.0123286306404)
  (16.8404   , 37.4864260204295)
  (17.1407   , 37.0258685374132)
  (17.5162   , 36.566253532193 )
  (17.8917   , 36.1063243677075)
  (18.2672   , 35.6463952032219)
  (18.6426   , 35.120492593011 )
  (19.0181   , 34.6608775877908)
  (19.3561   , 34.2669218690307)
  (19.6941   , 33.8729661502705)
  (20.0321   , 33.478696272245)
  (20.3324   , 33.0844263942195)
  (20.708    , 32.6907848347247)
  (21.046    , 32.2968291159645)
  (21.3839   , 31.836899951479 )
  (21.7594   , 31.37700220292  )
  (22.1725   , 30.9174814454794)
  (22.548    , 30.523808470058 )
  (22.9612   , 30.1304182379755)
  (23.3369   , 29.8689434814172)
  (23.5995   , 29.3421612252633)
  (23.9752   , 29.080686468705 )
  (24.3506   , 28.4886847490626)
  (24.8389   , 28.0958914195842)
  (25.2144   , 27.6360879188048)
  (25.8906   , 27.1125729190106)
  (26.3788   , 26.5875499547427)
  (26.7543   , 26.1277464539633)
  (27.2427   , 25.734953124485 )
  (27.7686   , 25.342473954272 )
  (28.2944   , 24.8177337333429)
  (28.8205   , 24.5574841979195)
  (29.2337   , 24.164093965837 )
  (29.8723   , 23.706363916209 )
  (30.3607   , 23.3135705867306)
  (30.8868   , 23.1194201607388)
  (31.4127   , 22.7269409905258)
  (31.9388   , 22.4666600391759)
  (32.615    , 21.9431450393817)
  (33.3665   , 21.5524251610547)
  (34.0429   , 21.2272389054817)
  (34.6064   , 20.8350424786075)
  (35.0948   , 20.5083796744872)
  (35.5457   , 20.2475018205331)
  (36.0342   , 20.0530372352759)
  (36.4851   , 19.7921907972484)
  (37.0112   , 19.59800895533  )
  (37.5372   , 19.2716288945485)
  (38.0258   , 19.0771957252179)
  (38.5519   , 18.8830138832995)
  (38.9277   , 18.6876696520993)
  (39.4537   , 18.4273887007494)
  (39.9423   , 18.2329555314187)
  (40.506    , 18.0390878487657)
  (40.9569   , 17.7782099948116)
  (41.483    , 17.5840595688197)
  (41.9715   , 17.3895949835625)
  (42.3472   , 17.1281516429308)
  (42.7606   , 16.9991892645009)
  (43.174    , 16.8702583019976)
  (43.5498   , 16.6749140707974)
  (43.9631   , 16.479852582936)
  (44.339    , 16.3506074611673)
  (44.6772   , 16.2210795960598)
  (45.2033   , 16.0268977541414)
  (45.504    , 15.9631862551266)
  (45.8798   , 15.8339411333579)
  (46.2556   , 15.7046960115892)
  (46.5939   , 15.5751681464817)
  (46.857    , 15.5111424882016)
  (47.1952   , 15.3816146230941)
  (47.571    , 15.2523695013254)
  (47.8717   , 15.188626586384 )
  (48.1348   , 15.1246009281039)
  (48.4354   , 14.9947903196575)
  (48.7736   , 14.8652310386235)
  (49.0368   , 14.8673359057014)
  (49.2622   , 14.7368969787244)
  (49.5254   , 14.7389704298757)
  (49.7885   , 14.6749447715956)
  (49.9763   , 14.4781239918482);

// Interpolated from Figure 1 of "Elastic scattering and charge
// transfer in slow collisions: isotopes of H and H + colliding with
// isotopes of H and with He" by Predrag S Krstić and David R Schultz,
// 1999 J. Phys. B: At. Mol. Opt. Phys. 32 3485
//
const Elastic::InterpPair ProtonHydrogen = boost::assign::map_list_of
  (-0.994302,   2.86205)
  (-0.897482,   2.90929)
  (-0.801179,   2.86016)
  (-0.691555,   2.89417)
  (-0.588753,   2.85638)
  (-0.49242,    2.81291)
  (-0.395965,   2.79213)
  (-0.292839,   2.8148)
  (-0.19019,    2.74866)
  (-0.0872765,  2.73165)
  (0.00935082,  2.74299)
  (0.112152,    2.7052)
  (0.208688,    2.69953)
  (0.311612,    2.68441)
  (0.401578,    2.65417)
  (0.517468,    2.65606)
  (0.613862,    2.62394)
  (0.716846,    2.62016)
  (0.819688,    2.58992)
  (0.909797,    2.58614)
  (1.01906,     2.55213)
  (1.1092,      2.55402)
  (1.21203,     2.52189)
  (1.3085,      2.50488)
  (1.41149,     2.5011)
  (1.52077,     2.47087)
  (1.61715,     2.43685)
  (1.71368,     2.42929)
  (1.81666,     2.42551)
  (1.9131,      2.40094)
  (2.0159,      2.36315);

// Interpolated from the top panel of Figure 4, op. cit.
//
const Elastic::InterpPair ProtonHelium = boost::assign::map_list_of
  (-0.984127,   2.68889)
  (-0.904762,   2.68889)
  (-0.825397,   2.68889)
  (-0.753968,   2.64444)
  (-0.674603,   2.6)
  (-0.595238,   2.57778)
  (-0.515873,   2.57778)
  (-0.444444,   2.55556)
  (-0.373016,   2.48889)
  (-0.293651,   2.44444)
  (-0.214286,   2.46667)
  (-0.142857,   2.44444)
  (-0.0634921,  2.4)
  (0.015873,    2.37778)
  (0.0952381,   2.37778)
  (0.166667,    2.33333)
  (0.246032,    2.28889)
  (0.325397,    2.28889)
  (0.404762,    2.28889)
  (0.47619,     2.24444)
  (0.555556,    2.2)
  (0.634921,    2.17778)
  (0.706349,    2.2)
  (0.785714,    2.17778)
  (0.865079,    2.13333)
  (0.936508,    2.08889)
  (1.01587,     2.06667)
  (1.09524,     2.08889)
  (1.16667,     2.06667)
  (1.24603,     2.04444)
  (1.3254,      2.02222)
  (1.40476,     1.97778)
  (1.47619,     1.93333)
  (1.55556,     1.91111)
  (1.63492,     1.91111)
  (1.71429,     1.91111)
  (1.79365,     1.91111)
  (1.87302,     1.91111)
  (1.95238,     1.91111);


// *** Add addtional elemental data sets here

Geometric::Geometric()
{
  radii = atomic_radii;
}

bool Elastic::extrapolate = true;

Elastic::Elastic()
{
  atomicdata[1] = Hydrogen;
  atomicdata[2] = Helium;
  iondata[1]    = ProtonHydrogen;
  iondata[2]    = ProtonHelium;
}

double Elastic::interpolate(const std::map<double, double> &data,
			    double x)
{
  typedef std::map<double, double>::const_iterator i_t;
  
  i_t i=data.upper_bound(x);
  
  if (i==data.begin())
    {
      return i->second;
    }
  
  if (i==data.end()) 
    {
      if (extrapolate) {
	i_t l = data.find(data.rbegin()->first);
	i = l; i--;

	double xa = log(i->first);
	double xb = log(l->first);
	double ya = log(i->second);
	double yb = log(l->second);

	double delta = (log(x) - xa)/(xb - xa);
	return l->second * exp(delta*(yb - ya));

      } else return (--i)->second;
    } 
  
  i_t l=i; l--;

  double delta = (x - l->first)/(i->first - l->first);

  return delta*i->second + (1-delta)*l->second;
}


