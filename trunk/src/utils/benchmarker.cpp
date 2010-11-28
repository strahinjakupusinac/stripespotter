#include "pch.h"
#define GLOBALS			// global variables for image filters are to be declared in this file
 						// (and extern'd elsewhere)
#include "StripeCode.h"
using namespace std;

// global objects
MTRNG RNG;
vector<int> DB;			// database of photoIDs
int QUERY;				// query image

// IR algorithms
ImageFeatures *imgFeatures = NULL;
void (*db_query)(int &, double &);
void db_query_random(int &, double &);
void db_query_generic(int &, double&);

// command line parameters
char ARG_input[256];	// input dataset (output file of dataset compiler)
char ARG_algorithm[32]; // algorithm to use
int ARG_ipa = 0;		// number of images per animal
int ARG_quiet = 0;		// non-interactive mode
int ARG_randseed = 31183; // (filled in with time(NULL) in main()), or specified on cmdline
int ARG_trials = 5;     // random trials
int ARG_sc_avg_cost = 0;    // StripeCode: return average cost over all Stripestrings
double ARG_sc_del_cost = 0.5;
int ARG_sc_use_abslen = 0;  // use normalized absolute lengths, not ratios
int ARG_exploratory = 0;    // exploratory mode (only execute function Exploratory())

CMDLINE_PARAMETERS cmd[] = {
	{ "--input", CMDLINE_STRING, ARG_input, 1, 255, 1,
	    "dataset file generated by the compiler program" },
	{ "--ipa", CMDLINE_INTEGER, &ARG_ipa, 1, 100, 1,
	    "number of images per animal" },
	{ "--method", CMDLINE_STRING, &ARG_algorithm, 1, 31, 1,
	    "algorithm ('random', 'stripecode', 'mrhisto')" },
	{ "--trials", CMDLINE_INTEGER, &ARG_trials, 5, 50000, 0,
	    "number of random trials" },
	{ "--quiet", CMDLINE_BINARY, &ARG_quiet, 0, 1, 0,
		"non-interactive mode" },
	{ "--seed", CMDLINE_INTEGER, &ARG_randseed, 0, 100000, 0,
		"random seed" },
    { "--sc:avg", CMDLINE_BINARY, &ARG_sc_avg_cost, 0, 1, 0,
        "StripeCode: distance = average cost over StripeStrings (as opposed to min cost)" },
    { "--sc:INDELCOST", CMDLINE_DOUBLE, &ARG_sc_del_cost, 0, 10, 0,
        "StripeCode: delete cost (usually a real in [0,1])" },
    { "--explore", CMDLINE_BINARY, &ARG_exploratory, 0, 1, 0,
        "exploratory mode (no experiments)"   },
    {
      "--sc:abslen", CMDLINE_BINARY, &ARG_sc_use_abslen, 0, 1, 0,
         "StripeCode: use normalized absolute lengths, not ratios"
    },
	{ "--help", CMDLINE_HELP, NULL, 0, 0, 0, "" },
	{ NULL, 0, NULL, 0, 0, 0, NULL }
};


// main data structures
map<string, vector<int> > animal_to_photos;	// maps animalID to photoID
map<int, string>		  photo_to_animal;	// maps photoID to animalID
vector<int>				  photos;			// list of all photoIDs
vector<string>			  animals;			// list of all animalIDs
map<int, ImageFeatures*>   photo_to_features;
vector<string> animal;		// unique animals in no particular order -- i.e., keys(animal_to_photos)



// General purpose exploratory mode
// -- animal, photographs, stripes/photograph are all loaded
// -- do whatever you want here
void Exploratory() {
    fprintf(stderr, "Exploratory mode.\n");

    vector<int> &plist = animal_to_photos[animals[0]];
    for(int p = 1; p < plist.size(); p++) {
        printf("Plotting comparison of photo %d and %d for animal '%s'.\n", plist[0], plist[p], animals[0].c_str());
        wxString fname;
        fname.Printf(_("comp-%d-%d.png"), plist[0], plist[p]);
        wxImage *img = ((StripeCode*)photo_to_features[plist[0]])->plotComparison(*(StripeCode*)photo_to_features[plist[p]]);
        if(img)
            img->SaveFile(fname, wxBITMAP_TYPE_PNG);
        else {
            fprintf(stderr, "Cannot write jpeg file.\n");
            return;
        }
        delete img;
    }

    // compare non-animals
    vector<int> &plist2 = animal_to_photos[animals[2]];
    for(int p = 0; p < plist2.size(); p++) {
        printf("Plotting NEGATIVE comparison of photo %d ('%s') and photo %d ('%s').\n", plist[0], animals[0].c_str(), plist2[p], animals[2].c_str());
        wxString fname;
        fname.Printf(_("compNEG-%d-%d.png"), plist[0], plist2[p]);
        wxImage *img = ((StripeCode*)photo_to_features[plist[0]])->plotComparison(*(StripeCode*)photo_to_features[plist2[p]]);
        if(img)
            img->SaveFile(fname, wxBITMAP_TYPE_PNG);
        else {
            fprintf(stderr, "Cannot write jpeg file.\n");
            return;
        }
        delete img;
    }

}

template<class T>
void shuffle (std::vector<T> & deck) {
    int deckSize = (int) deck.size();
    while(deckSize > 1) {
       int k = RNG.genrand_real2() * deckSize;
       deckSize--;
	   swap(deck[deckSize], deck[k]);
    }
}

// reads the input file specified on the command line (in global 'ARG_input')
int read_dataset() {
	// read and parse image features
	FILE *fp = fopen(ARG_input, "r");
	if(!fp)
		return printf("Cannot open '%s'\n", ARG_input);
	char buf[4096]; int line = 0;
	while(fgets(buf, 4096, fp) && ++line)
		if(strncmp(buf, "ANIMAL ", 7)==0) {
		    // get animal name and photo id
			char *p;
			for(p=buf+7; *p && (*p!=' ' || (*p++=0) ); p++);    // fixed-format matching
			if(*(buf+7) && *p) {
				string aname(buf+7);
				int photoid = atoi(p);
				if(photoid < 1) {
					fprintf(stderr, "Error: photoID must be greater than 1 (line %d)\n", line);
					return 0;
				}

                // save photo pointer and read image features
				if(!imgFeatures->read(fp)) {
				    fprintf(stderr, "Error: malformed image feature set (line %d)\n", line);
				    return 0;
				}
				animal_to_photos[aname].push_back(photoid);
				photo_to_features[photoid] = imgFeatures;
				imgFeatures = imgFeatures->clone();
			} else {
			    fprintf(stderr, "Invalid line in file: line %d\n", line);
			    fclose(fp);
			    return 0;
			}
		}
	fclose(fp);

    // delete any animals with less than ipa+1 pictures
    for(map<string,vector<int> >::iterator itr = animal_to_photos.begin(); itr != animal_to_photos.end(); )
        if(itr->second.size() < (unsigned)ARG_ipa+1) {
            fprintf(stderr, "Warning: animal '%s' has less than %d pictures, ignoring it.\n", itr->first.c_str(), ARG_ipa+1);
            animal_to_photos.erase(itr++);
        } else
            itr++;

    // BUILD INDICES
	// list of animals = keys of the set of animal names
	for(map<string,vector<int> >::iterator itr = animal_to_photos.begin(); itr!=animal_to_photos.end(); itr++) {
		animals.push_back(itr->first);
		for(vector<int>::iterator it2 = itr->second.begin(); it2 != itr->second.end(); it2++) {
            photos.push_back(*it2);
            photo_to_animal[*it2] = itr->first;
		}
	}

	return 1;
}

// sample a random database of 'dbsize' animals, retaining one random image as the query image
// and 'ARG_ipa' images per animal as the database
//
// in other words, samples uniformly from the space of databases and query images
// INPUT:
//   dbsize  - number of animals
//   ARG_ipa - images per animal (global)
// OUTPUT:
//   QUERY   - query image (global)
//   DB      - database of photo IDs(global)
void sample_db_query_pair(int dbsize) {
	QUERY = -1;
	DB.clear();

	// create database of 'dbsize' animals, 'ARG_ipa' images per animal
	shuffle(animals);
	vector<int> remainder;
	for(int a = 0; a < dbsize; a++) {
		string animalID = animals[a];
		vector<int> &photolist = animal_to_photos[animalID];
		shuffle(photolist);
		for(int i = 0; i < ARG_ipa; i++)
            DB.push_back(photolist[i]);
		for(int i = ARG_ipa; i < (int)photolist.size(); i++)
			remainder.push_back(photolist[i]);
	}

	// choose query image
	shuffle(remainder);
	QUERY = remainder[RNG.genrand_real2()*remainder.size()];

    // sanity check to make sure DB does not contain query
    for(unsigned i = 0; i < DB.size(); i++)
        assert(DB[i] != QUERY);
}


void db_query_random(int &correctrank, double &querytime) {
	// a real complicated algorithm
	startClocks();
	shuffle(DB);
	querytime = stopClocks();

	// get the ground truth
	set<string> seen;
	string &correct = photo_to_animal[QUERY];

	// compute rank of correct animal (ranks start at 1)
	for(unsigned i = 0; i < DB.size(); i++) {
		string &a = photo_to_animal[DB[i]];
		seen.insert(a);
		if(a == correct) {
			correctrank = (int) seen.size();
			return;
		}
	}
	assert(0);
}

void db_query_generic(int &correctrank, double &querytime) {
    // rank database images by query image
    ImageFeatures *queryImg = photo_to_features[QUERY];
    multimap<double, int> ranking;
    startClocks();
    for(vector<int>::iterator i = DB.begin(); i != DB.end(); i++)
        ranking.insert(pair<double,int>(photo_to_features[*i]->compare(queryImg, NULL), *i));
    querytime = stopClocks();

    // compute correct rank
    set<string> seen;
    string &ground_truth = photo_to_animal[QUERY];
    for(multimap<double,int>::iterator photo = ranking.begin(); photo != ranking.end(); photo++) {
        string &aname = photo_to_animal[photo->second];
        seen.insert(aname);
        if(aname == ground_truth) {
            correctrank = (int)seen.size();
            return;
        }
    }
    assert(0);
}

int main(int argc, char *argv[]) {
	// setup
	ARG_randseed = time(NULL) % getpid();
	if(!ParseCommandLine(argc, argv, cmd))
		return 1;
	if(!ARG_quiet)
		fprintf(stderr, "StripeCode test and benchmarker. Copyright (c) 2010 Mayank Lahiri (mlahiri@gmail.com).\n\n");
	RNG.init_genrand(ARG_randseed);

	// select the algorithm we're using
	if(!strcmp(ARG_algorithm, "stripecode")) {
		db_query  = db_query_generic;
		imgFeatures= new StripeCode();
		if(ARG_sc_avg_cost)
            StripeCode::RETMINCOST = false;
        StripeCode::INDELCOST = ARG_sc_del_cost;
        StripeCode::USERATIOS = !ARG_sc_use_abslen;
	} else {
        if(!strcmp(ARG_algorithm, "random")) {
            db_query = db_query_random;
            imgFeatures= new StripeCode();
        } else {
            if(!strcmp(ARG_algorithm, "mrhisto")) {
                db_query = db_query_generic;
                imgFeatures= new MultiScaleHistogram();
            } else
                return fprintf(stderr, "Unknown algorithm '%s'\n", ARG_algorithm);
        }
    }

	// read and parse dataset file (stored in ARG_input)
	if(!read_dataset())
		return 1;

	// print header
	if(!ARG_quiet)
		printf("# animals %d, photos %d, i.p.a. %d, random %d, algorithm '%s', sc_mincost %d, sc_indelcost %f, sc_useratios %d.\n",
				(int) animal_to_photos.size(), (int) photo_to_features.size(), ARG_ipa, ARG_randseed, ARG_algorithm, StripeCode::RETMINCOST?1:0, StripeCode::INDELCOST, StripeCode::USERATIOS?1:0);

	if(ARG_exploratory) {
	    wxInitAllImageHandlers();
        Exploratory();
        return 0;
	}

	// print header for R
	printf("dbsize correctrank reciprank querytime\n");

	// run experiment
	for(float dbfrac = 0.2; dbfrac <= 1.01; dbfrac += 0.2) {
		// database size (number of animals)
		int dbsize = dbfrac * (float)animal_to_photos.size();

		// random trials (uniformly sample from space of databases and queries)
		for(int trial = 0; trial < ARG_trials; trial++) {
			sample_db_query_pair(dbsize);

			int correctrank   = 0;
			double querytime  = 0;

			db_query(correctrank, querytime);

			printf("%d %d %f %g\n", dbsize, correctrank, (1.0/(double)correctrank), querytime);
		}
	}

	return 0;
}



/*
// currently select ranking algorithm
float (*DBscan)(vector<int> &, int, vector<string> &ranking);

// all ranking algorithms
float DBscan_random(vector<int> &, int, vector<string> &ranking);
// ranking algorithm: random
float DBscan_random(vector<int> &DB, int query_pid, vector<string> &ranking) {
	set<string> animal_names;

	for(int n = (int) DB.size(); --n >= 0; )
		// -1 is "hidden" from view
		if(DB[n] >= 0)
			animal_names.insert(photos_to_animal[DB[n]]);
	assert(animal_names.size() > 1);

	ranking.clear();
	for(set<string>::iterator itr = animal_names.begin(); itr!= animal_names.end(); itr++)
		ranking.push_back(*itr);
	shuffle<string>(ranking);

	int randomrank = RNG.genrand_real2() * animal_names.size();
	return (randomrank)/(float)(animal_names.size()-1);
}

// ranking algorithm: Euclidean distance of StripeCodes
void EXP_FastScan() {
	int nAnimals = (int) animal_to_photos.size();
	int nPhotos  = (int) photo_to_stripecodes.size();
	vector<string> ranking;

	// outer loop over database size parameter
	for(float frac = 0.1; frac <= 0.95; frac+= 0.3) {
		int Na = frac * nAnimals;
		double dbsize_mean_rank = 0;
		double dbsize_stdev_rank = 0;
		double dbsize_mean_time = 0;
		double dbsize_stdev_time = 0;
		double dbsize_num = 0;

		// inner loop over random samples for a given database size N animals
		// till convergence or maximum 30 iterations
		for(int trial1 = 0; trial1 < 50; trial1++) {
			vector<int> Pool;

			// generate a database of size N animals with all their images
			shuffle<string>(animal);
			for(int n = 0; n < Na; n++) {
				vector<int> &pvec = animal_to_photos[animal[n]];
				for(int p = (int) pvec.size(); --p >= 0; )
					Pool.push_back(pvec[p]);
			}

			// leave-one-out cross-validation in random order of images
			// the random order of images is mainly to test for early convergence
			// to a stable mean error.
			shuffle<int>(Pool);
			double loocv_mean_rank  = 0;
			double loocv_mean_time  = 0;
			double loocv_num = 0;
			for(int i = (int) Pool.size(); --i >= 0; ) {
				// remove photo from database temporarily
				int pid = Pool[i];

				// create a database with one random image per animal
				vector<int> DBphotos;
				for(int n = 0; n < Na; n++) {
					vector<int> &pvec = animal_to_photos[animal[n]];
					shuffle<int>(pvec);
					// pick a random image that's not the query image
					for(int p = (int) pvec.size(), q=0; q==0 && --p >= 0; )
						if(pvec[p] != pid) {
							DBphotos.push_back(pvec[p]);
							q = 1;
						}
				}
				assert(DBphotos.size() == Na);

				// run query
				startClocks();
				float correct_rank = DBscan(DBphotos, pid, ranking);
				double elapsed = stopClocks();

				// adjust statistics
				loocv_mean_time += elapsed;
				loocv_mean_rank += correct_rank;
				loocv_num ++;
			}

			// get means
			loocv_mean_rank /= loocv_num;
			loocv_mean_time /= loocv_num;

			// update statistics
			dbsize_mean_rank += loocv_mean_rank;
			dbsize_mean_time += loocv_mean_time;
			dbsize_stdev_rank+= loocv_mean_rank*loocv_mean_rank;
			dbsize_stdev_time+= loocv_mean_time*loocv_mean_time;
			dbsize_num++;

			//printf("# N(frac) %f trial %d loocv_mean_rank %f loocv_mean_time %f\n", frac, trial1, loocv_mean_rank, loocv_mean_time);
		}
		dbsize_mean_rank /= dbsize_num;
		dbsize_mean_time /= dbsize_num;
		dbsize_stdev_rank = sqrt(dbsize_stdev_rank/dbsize_num - pow(dbsize_mean_rank, 2));
		dbsize_stdev_time = sqrt(dbsize_stdev_time/dbsize_num - pow(dbsize_mean_time, 2));

		printf("DBSIZE_FAST: animals %d PERF: mean_rank %f %f mean_time %f %f num_trials %f\n",
				Na,
				dbsize_mean_rank,
				dbsize_stdev_rank,
				dbsize_mean_time,
				dbsize_stdev_time,
				dbsize_num);
	}
}



void EXP_FullScan() {
	int nAnimals = (int) animal_to_photos.size();
	int nPhotos  = (int) photo_to_stripecodes.size();
	vector<string> ranking;

	// outer loop over database size parameter
	for(float frac = 0.1; frac <= 0.95; frac += 0.3) {
		int Na = frac * nAnimals;
		double dbsize_mean_rank = 0;
		double dbsize_stdev_rank = 0;
		double dbsize_mean_time = 0;
		double dbsize_stdev_time = 0;
		double dbsize_mean_photos_per_animal = 0;
		double dbsize_num = 0;

		// inner loop over random samples for a given database size N animals
		// till convergence or maximum 30 iterations
		for(int trial1 = 0; trial1 < 50; trial1++) {
			vector<int> DBphotos;

			// generate a database of size N animals with all their images
			shuffle<string>(animal);
			unsigned total_photos = 0;
			for(int n = 0; n < Na; n++) {
				const vector<int> &pvec = animal_to_photos[animal[n]];
				total_photos += pvec.size();
				for(int p = (int) pvec.size(); --p >= 0; )
					DBphotos.push_back(pvec[p]);
			}
			dbsize_mean_photos_per_animal += (float)total_photos/(float)Na;
			assert(DBphotos.size() == total_photos);

			// leave-one-out cross-validation in random order of images
			// the random order of images is mainly to test for early convergence
			// to a stable mean error.
			shuffle<int>(DBphotos);
			double loocv_mean_rank  = 0;
			double loocv_mean_time  = 0;
			double loocv_num = 0;
			for(int i = DBphotos.size(); --i >= 0; ) {

				// remove photo from database temporarily
				int pid = DBphotos[i];
				DBphotos[i] = -1;

				// run query
				startClocks();
				float correct_rank = DBscan(DBphotos, pid, ranking);
				double elapsed = stopClocks();

				// adjust statistics
				loocv_mean_time += elapsed;
				loocv_mean_rank += correct_rank;
				loocv_num ++;

				// put photo back in database
				DBphotos[i] = pid;
			}

			// get means
			loocv_mean_rank /= loocv_num;
			loocv_mean_time /= loocv_num;

			// update statistics
			dbsize_mean_rank += loocv_mean_rank;
			dbsize_mean_time += loocv_mean_time;
			dbsize_stdev_rank+= loocv_mean_rank*loocv_mean_rank;
			dbsize_stdev_time+= loocv_mean_time*loocv_mean_time;
			dbsize_num++;

			//printf("# N(frac) %f trial %d loocv_mean_rank %f loocv_mean_time %f\n", frac, trial1, loocv_mean_rank, loocv_mean_time);
		}
		dbsize_mean_rank /= dbsize_num;
		dbsize_mean_time /= dbsize_num;
		dbsize_mean_photos_per_animal /= dbsize_num;
		dbsize_stdev_rank = sqrt(dbsize_stdev_rank/dbsize_num - pow(dbsize_mean_rank, 2));
		dbsize_stdev_time = sqrt(dbsize_stdev_time/dbsize_num - pow(dbsize_mean_time, 2));

		printf("DBSIZE_FULL: animals %d mean_photos_per_animal %f PERF: mean_rank %f %f mean_time %f %f num_trials %f\n",
				Na,
				dbsize_mean_photos_per_animal,
				dbsize_mean_rank,
				dbsize_stdev_rank,
				dbsize_mean_time,
				dbsize_stdev_time,
				dbsize_num);
	}
}
*/
