// Community.h
// Manages relationship between people, locations, and mosquitoes
#ifndef __COMMUNITY_H
#define __COMMUNITY_H
#include <string>
#include <vector>
#include <map>
#include <set>
#include <utility>
#include <numeric>
#include <cmath>
#include <algorithm>

class Person;
class Mosquito;
class Location;

// We use this to make sure that locations are iterated through in a well-defined order (by ID), rather than by mem address
struct LocPtrComp { bool operator()(const Location* A, const Location* B) const { return A->getID() < B->getID(); } };

// We use this to created a vector of people, sorted by decreasing age.  Used for aging/immunity swapping.
struct PerPtrComp { bool operator()(const Person* A, const Person* B) const { return A->getAge() > B->getAge(); } };

class Community {
    public:
        Community(const Parameters* parameters);
        virtual ~Community();
        bool loadPopulation(std::string szPop,std::string szImm, std::string szSwap);
        bool loadLocations(std::string szLocs,std::string szNet);
        bool loadMosquitoes(std::string moslocFilename, std::string mosFilename);
        int getNumPeople() const { return _people.size(); }
        std::vector<Person*> getPeople() const { return _people; }
        int getNumInfected(int day);
        int getNumSymptomatic(int day);
        std::vector<int> getNumSusceptible();
        void populate(Person **parray, int targetpop);
        Person* getPersonByID(int id);
        bool infect(int id, Serotype serotype, int day);
        void attemptToAddMosquito(Location *p, Serotype serotype, int nInfectedByID, double prob_infecting_bite);
        int getDay() { return _nDay; }                                // what day is it?
        void swapImmuneStates();
        void updateDiseaseStatus();
        void mosquitoToHumanTransmission();
        void humanToMosquitoTransmission();
        void tick(int day);                                           // simulate one day
        void setNoSecondaryTransmission() { _bNoSecondaryTransmission = true; }
        void setMosquitoMultiplier(double f) { _fMosquitoCapacityMultiplier = f; }  // seasonality multiplier for number of mosquitoes
        void applyMosquitoMultiplier(double f);                    // sets multiplier and kills off infectious mosquitoes as necessary
        void applyVectorControl();
        double getMosquitoMultiplier() const { return _fMosquitoCapacityMultiplier; }

        void setExpectedExtrinsicIncubation(double n) { _expectedEIP = n; _EIP_emu = exp(log(_expectedEIP) - (_EIP_sigma*_EIP_sigma)/2.0); }
        double getExpectedExtrinsicIncubation() const { return _expectedEIP; }
        double getEIP() const { return (_par->simpleEIP ? _expectedEIP : _EIP_emu * exp(gsl_ran_gaussian(RNG, _EIP_sigma))); }

        int getNumInfectiousMosquitoes();
        int getNumExposedMosquitoes();
        void vaccinate(CatchupVaccinationEvent cve);
        void targetVaccination(Person* p); // routine vaccination on target birthday
        void updateVaccination();          // for boosting and multi-does vaccines
        void setVES(double f);
        void setVESs(std::vector<double> f);
        Mosquito *getInfectiousMosquito(int n);
        Mosquito *getExposedMosquito(int n);
        std::vector< std::vector<int> > getNumNewlyInfected() { return _nNumNewlyInfected; }
        std::vector< std::vector<int> > getNumNewlySymptomatic() { return _nNumNewlySymptomatic; }
        std::vector< std::vector<int> > getNumVaccinatedCases() { return _nNumVaccinatedCases; }
        std::vector< std::vector<int> > getNumSevereCases() { return _nNumSevereCases; }
        static void flagInfectedLocation(Location* _pLoc, int day);

        int ageIntervalSize(int ageMin, int ageMax) { return std::accumulate(_nPersonAgeCohortSizes+ageMin, _nPersonAgeCohortSizes+ageMax,0); }

        void reset();                                                 // reset the state of the community
        const std::vector<Location*> getLocations() const { return _location; }
        const std::vector< std::vector<Mosquito*> > getInfectiousMosquitoes() const { return _infectiousMosquitoQueue; }
        const std::vector< std::vector<Mosquito*> > getExposedMosquitoes() const { return _exposedMosquitoQueue; }
        const std::vector<Person*> getAgeCohort(unsigned int age) const { assert(age<_personAgeCohort.size()); return _personAgeCohort[age]; }

        std::vector< std::vector<int> > tallyInfectionsByLocType(bool tally_tirs);

    protected:
        static const Parameters* _par;
        std::vector<Person*> _people;                                 // the array index is equal to the ID
        std::vector< std::vector<Person*> > _personAgeCohort;         // array of pointers to people of the same age
        int _nPersonAgeCohortSizes[NUM_AGE_CLASSES];                  // size of each age cohort
        double *_fMortality;                                          // mortality by year, starting from 0
        std::vector<Location*> _location;                             // the array index is equal to the ID
        std::vector< std::vector<Person*> > _exposedQueue;            // queue of people with n days of latency left
        std::vector< std::vector<Mosquito*> > _infectiousMosquitoQueue;  // queue of infectious mosquitoes with n days
                                                                         // left to live
        std::vector< std::vector<Mosquito*> > _exposedMosquitoQueue;  // queue of exposed mosquitoes with n days of latency left
        int _nDay;                                                    // current day
        int _nMaxInfectionParity;                                     // maximum number of infections (serotypes) per person
        bool _bNoSecondaryTransmission;
        double _fMosquitoCapacityMultiplier;                          // seasonality multiplier for mosquito capacity
        double _expectedEIP;                                          // extrinsic incubation period in days
        double _EIP_emu;                                              // e^mu for log-normal sampling of EIP, (Chan & Johanson 2012)
        static constexpr double _EIP_sigma = pow((double) 4.9, -0.5); // SD for log-normal sampling of EIP, (Chan & Johanson 2012)
        //static const double _EIP_sigma = 0.4517539514526256;            // same as above, but to accommodate the intel compiler
        std::vector< std::vector<int> > _nNumNewlyInfected;
        std::vector< std::vector<int> > _nNumNewlySymptomatic;
        std::vector< std::vector<int> > _nNumVaccinatedCases;
        std::vector< std::vector<int> > _nNumSevereCases;
        static std::vector<std::set<Location*, LocPtrComp> > _isHot;
        static std::vector<Person*> _peopleByAge;
        static std::map<int, std::set<std::pair<Person*, Person*> > > _delayedBirthdays;
        static std::set<Person*> _revaccinate_set;          // not automatically re-vaccinated, just checked for boosting, multiple doses

        static std::vector<std::set<Location*, LocPtrComp> > _vectorControlStartDates;
        static std::set<Location*, LocPtrComp> _vectorControlLocations; // Locations that currently have vector control measures in place
        bool _uniformSwap;                                            // use original swapping (==true); or parse swap file (==false)

        void expandExposedQueues();
        void expandMosquitoQueues();
        void moveMosquito(Mosquito *m);
        void mosquitoFilter(std::vector<Mosquito*>& mosquitoes, const double survival_prob);
        void _advanceTimers();
        void _modelMosquitoMovement();
        void _processBirthday(Person* p);
        void _processDelayedBirthdays();
        void _swapIfNeitherInfected(Person* p, Person* donor);
};
#endif
