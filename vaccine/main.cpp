//#include "mpi.h"
#include <unistd.h>
#include "mpi_simulator.h"
#include <cstdlib>
//#include "CCRC32.h"
#include "Utility.h"
#include "ImmunityGenerator.h"

using namespace std;

using dengue::util::to_string;
using dengue::util::mean;
using dengue::util::stdev;
using dengue::util::max_element;

time_t GLOBAL_START_TIME;

///void setup_mpi(MPI_par &m, int &argc, char **argv) {
//    /* MPI variables */
//    m.comm  = MPI_COMM_WORLD;
//    m.info  = MPI_INFO_NULL;
//
//    /* Initialize MPI */
//    MPI_Init(&argc, &argv);
//    MPI_Comm_size(m.comm, &m.mpi_size);
//    MPI_Comm_rank(m.comm, &m.mpi_rank);  
//}

Parameters* define_default_parameters() {
    Parameters* par = new Parameters();
    par->define_defaults();

    double _EF       = 36;
    double _mos_move = 0.5;
    //double _exp_coef = 1;
    double _nmos     = 70;
    
    double _betamp   = 0.25;
    double _betapm   = 0.1;
    string HOME(std::getenv("HOME"));
    string pop_dir = HOME + "/work/dengue/pop-yucatan"; 
//    string WORK(std::getenv("WORK"));
//    string imm_dir = WORK + "/initial_immunity";
//    string imm_dir = pop_dir + "/immunity";

    par->randomseed = 5500;
    par->nRunLength = 20*365 + 100;
    par->annualIntroductionsCoef = 1;
    par->nDailyExposed = {{1.0, 1.0, 1.0, 1.0}};

    par->fPrimaryPathogenicity[0] = 1.0;
    par->fPrimaryPathogenicity[1] = 0.25;
    par->fPrimaryPathogenicity[2] = 1.0;
    par->fPrimaryPathogenicity[3] = 0.25;
    par->fSecondaryScaling[0] = 1.0;
    par->fSecondaryScaling[1] = 1.0;
    par->fSecondaryScaling[2] = 1.0;
    par->fSecondaryScaling[3] = 1.0;
    par->betaPM = _betapm;
    par->betaMP = _betamp;
    par->expansionFactor = _EF;
    par->fMosquitoMove = _mos_move;
    par->szMosquitoMoveModel = "weighted";
    par->fMosquitoTeleport = 0.0;
    par->nDefaultMosquitoCapacity = (int) _nmos;
    par->eMosquitoDistribution = EXPONENTIAL;

    {
        static const int _time_periods[] = {7,7,7,7,7,7,7,7,7,7,
                                          7,7,7,7,7,7,7,7,7,7,
                                          7,7,7,7,7,7,7,7,7,7,
                                          7,7,7,7,7,7,7,7,7,7,
                                          7,7,7,7,7,7,7,7,7,7,
                                          7,8};  
        static const double _multipliers[] = {0.05,0.04,0.05,0.04,0.03,0.04,0.05,0.03,0.02,0.02,
                                            0.03,0.03,0.05,0.04,0.05,0.04,0.06,0.07,0.08,0.09,
                                            0.11,0.15,0.18,0.19,0.24,0.28,0.38,0.46,0.45,0.61,
                                            0.75,0.97,0.91,1.00,0.94,0.85,0.79,0.71,0.65,0.65,
                                            0.42,0.30,0.26,0.27,0.11,0.10,0.11,0.12,0.09,0.08,
                                            0.04,0.07};
        int num_periods = sizeof(_multipliers) / sizeof(_multipliers[0]);
        //assert(_time_periods.size() == _multipliers.size());
        par->nSizeMosquitoMultipliers = num_periods;
        par->nMosquitoMultiplierCumulativeDays[0] = 0;

        for (int j=0; j<num_periods; j++) {
            par->nMosquitoMultiplierDays[j] = _time_periods[j];
            par->nMosquitoMultiplierCumulativeDays[j+1] =
                par->nMosquitoMultiplierCumulativeDays[j]+par->nMosquitoMultiplierDays[j];
            par->fMosquitoMultipliers[j] = _multipliers[j];
        }
    }

    par->nDaysImmune = 365;

    //par->annualSerotypeFile = pop_dir + "/serotypes-yucatan.txt";
    //par->loadAnnualSerotypes(par->annualSerotypeFile);

    par->szPopulationFile = pop_dir + "/population-yucatan.txt";
    par->szImmunityFile   = "";
    par->szLocationFile   = pop_dir + "/locations-yucatan.txt";
    par->szNetworkFile    = pop_dir + "/network-yucatan.txt";
    par->szSwapProbFile   = pop_dir + "/swap_probabilities-yucatan.txt";

    return par;
}

// Take a list of values, return original indices sorted by value
vector<int> ordered(vector<int> const& values) {

    vector<pair<int,int> > pairs(values.size());
    for(unsigned int pos=0; pos<values.size(); pos++) {
        pairs[pos] = make_pair(values[pos],pos);
    }

    //bool comparator ( const mypair& l, const mypair& r) { return l.first < r.first; }
    std::sort( pairs.rbegin(), pairs.rend() ); // sort greatest to least
    vector<int> indices(values.size());
    for(unsigned int i=0; i < pairs.size(); i++) indices[i] = pairs[i].second;

    return indices;
}


void sample_immune_history(Community* community, const Parameters* par) {
    const int last_immunity_year = 2012;
    vector<vector<vector<int>>> full_pop = simulate_immune_dynamics(par->expansionFactor, last_immunity_year);

    int N = community->getNumPerson();
    for(int i=0; i<N; i++) {
        Person* person = community->getPerson(i);

        int age = person->getAge();
        int maxAge = MAX_CENSUS_AGE;

        age = age <= maxAge ? age : maxAge;
        int r = gsl_rng_uniform_int(RNG, full_pop[age].size());
        vector<int> states = full_pop[age][r];
        // we need to go through the states, greatest to least
        // Serotype 0, 1,  2, 3   -->   1, 3, 2, 0
        //        { 0, 12, 1, 2 } --> { 1, 3, 2, 0 }
        vector<int> indices = ordered(states);
        for (int serotype: indices) {
            if (states[serotype]>0) {
                person->setImmunity((Serotype) serotype);
                person->initializeNewInfection();
                person->setRecoveryTime(-365*states[serotype]); // last dengue infection was x years ago
            }
        }
    }
}
/*
unsigned int report_process_id (vector<long double> &args, const time_t start_time) {
//unsigned int report_process_id (vector<long double> &args, const MPI_par* mp, const time_t start_time) {
    // CCRC32 checksum based on string version of argument values
    CCRC32 crc32;
    crc32.Initialize();

    string argstring;
    for (unsigned int i = 0; i < args.size(); i++) argstring += to_string((long double) args[i]) + " ";

    const unsigned char* argchars = reinterpret_cast<const unsigned char*> (argstring.c_str());
    const int len = argstring.length();
    const int process_id = crc32.FullCRC(argchars, len);
    //fprintf(stderr, "%Xbegin\n", process_id);

    double dif = difftime (start_time, GLOBAL_START_TIME);

    stringstream ss;
    ss << " begin " << hex << process_id << " " << dif << " " << argstring << endl;
    //ss << mp->mpi_rank << " begin " << hex << process_id << " " << dif << " " << argstring << endl;
    string output = ss.str();
    fprintf(stderr, output.c_str());

    return process_id;
}*/

void append_if_finite(vector<long double> &vec, double val) {
    if (isfinite(val)) { 
        vec.push_back((long double) val);
    } else {
        vec.push_back(0);
    }
}

vector<long double> simulator(const Parameters* par) {
//vector<long double> simulator(const Parameters* par, const MPI_par* mp) {
    // initialize bookkeeping for run
    // time_t start ,end;
    // time (&start);
    // const unsigned int process_id = report_process_id(args, mp, start);
    int process_id = 0;

    // initialize & run simulator 
    gsl_rng_set(RNG, par->randomseed);
    Community* community = build_community(par);
    sample_immune_history(community, par);
    //vector<int> initial_susceptibles = community->getNumSusceptible();
    seed_epidemic(par, community);
    vector<int> epi_sizes = simulate_epidemic(par, community, process_id);

    //time (&end);
    //double dif = difftime (end,start);

    // calculate linear regression based on estimated reported cases
    const double ef = par->expansionFactor;
//    vector<double> x(epi_sizes.size());
    vector<long double> y(epi_sizes.size());
//    Col y(epi_sizes.size());
    const int pop_size = community->getNumPerson();
    for (unsigned int i = 0; i < epi_sizes.size(); i++) { 
        // convert infections to cases per 1,000
        y[i] = ((long double) 1e3*epi_sizes[i])/(ef*pop_size); 
        //y[i] = ((float_type) 1e3*epi_sizes[i])/(ef*pop_size); 
    }
/*
    stringstream ss;
    ss << mp->mpi_rank << " end " << hex << process_id << " " << dec << dif << " ";
    // parameters
    ss << par->expansionFactor << " " << par->fMosquitoMove << " " << par->annualIntroductionsCoef << " "
       << par->nDefaultMosquitoCapacity << " ";
    // metrics
    float_type _mean             = mean(y);
    float_type _median           = median(y);
    float_type _stdev            = sqrt(variance(y, _mean));
    float_type _max              = max(y);
    float_type _skewness         = skewness(y);
    float_type _median_crossings = median_crossings(y);
    ss << _mean << " " << _median << " " << _stdev << " " << _max << " " << _skewness << " " << _median_crossings << endl;
      
//       << fit->m << " " << fit->b << " " << fit->rsq << endl;

    string output = ss.str();
    fprintf(stderr, output.c_str());
    
    vector<long double> metrics;
    append_if_finite(metrics, _mean);
    append_if_finite(metrics, _median);
    append_if_finite(metrics, _stdev);
    append_if_finite(metrics, _max);
    append_if_finite(metrics, _skewness);
    append_if_finite(metrics, _median_crossings);
*/
    delete par;
    delete community;
    //return metrics;
    return y;
}


int main(int argc, char* argv[]) {
//    MPI_par mp;
//    setup_mpi(mp, argc, argv);
    const gsl_rng* RNG = gsl_rng_alloc (gsl_rng_taus2);
    gsl_rng_set(RNG, time (NULL) * getpid()); // seed the rng using sys time and the process id

    time(&GLOBAL_START_TIME);
    const int years_simulated = 20;
    const int max_catchup_age = 46;

    vector<bool> vaccinate_bool = {false, true};
    vector<bool> retro_bool = {false, true};
    vector<bool> catchup_bool = {false, true};
    vector<int> target_ages = {2,6,10,14};

    cerr << "vaccine retro catchup target ";
    for (int i = 0; i<years_simulated; i++) cerr << "y" << i << " ";
    cerr << endl;


    // Establish baseline
    for (bool vaccinate: vaccinate_bool) {
        if (not vaccinate) {
            bool retro = false;
            bool catchup = false;
            int target = -1;
            Parameters* par = define_default_parameters(); 

            cerr << vaccinate << " " << retro << " " << catchup << " " << target << " ";
            // epi sizes in cases, not infections!
            vector<long double> epi_sizes = simulator(par);
            for (auto v: epi_sizes) cerr << v << " ";
            cerr << endl;
        } else {
            for (bool retro: retro_bool) {
                for (bool catchup: catchup_bool) {
                    for (int target: target_ages) {
                        Parameters* par = define_default_parameters(); 

                        par->bVaccineLeaky = true;

                        par->fVESs_NAIVE.clear();
                        par->fVESs_NAIVE = {0.35, 0.0, 0.35, 0.35};

                        par->fVESs.clear();
                        par->fVESs = {0.7, 0.35, 0.7, 0.7};

                        if (catchup) {
                            for (int catchup_age = target + 1; catchup_age<max_catchup_age; catchup_age++) {
                                par->nVaccinateYear.push_back(1);
                                par->nVaccinateAge.push_back(catchup_age);
                                par->fVaccinateFraction.push_back(0.7);
                                par->nSizeVaccinate++;
                            }
                        } 

                        for (int vacc_year = 1; vacc_year <= years_simulated; vacc_year++) {
                            par->nVaccinateYear.push_back(vacc_year);
                            par->nVaccinateAge.push_back(target);
                            par->fVaccinateFraction.push_back(0.7);
                            par->nSizeVaccinate++;
                        }

                        if (retro) par->bRetroactiveMatureVaccine = true;

                        cerr << vaccinate << " " << retro << " " << catchup << " " << target << " ";
                        // epi sizes in cases, not infections!
                        vector<long double> epi_sizes = simulator(par);
                        for (auto v: epi_sizes) cerr << v << " ";
                        cerr << endl;
                    }
                }
            }        
        }
    }
//    MPI_Finalize();
    return 0;
}
