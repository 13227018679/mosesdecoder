/*
 *  Data.cpp
 *  met - Minimum Error Training
 *
 *  Created by Nicola Bertoldi on 13/05/08.
 *
 */

#include <algorithm>
#include "util/check.hh"
#include <cmath>
#include <fstream>

#include "Data.h"
#include "FileStream.h"
#include "Scorer.h"
#include "ScorerFactory.h"
#include "Util.h"

Data::Data(Scorer& ptr, const std::string& sparseweightsfile)
    : theScorer(&ptr),
      score_type(theScorer->getName()),
      number_of_scores(0),
      scoredata(new ScoreData(*theScorer)),
      featdata(new FeatureData)
{
//  TRACE_ERR("Data::score_type " << score_type << std::endl);
//  TRACE_ERR("Data::Scorer type from Scorer: " << theScorer->getName() << endl);
  if (sparseweightsfile.size()) {
    sparse_weights.load(sparseweightsfile);
    ostringstream msg;
    msg << "Data::sparse_weights {";
    sparse_weights.write(msg,"=");
    msg << "}";
    TRACE_ERR(msg.str() << std::endl);
  }
}

Data::~Data() {
  if (featdata) {
    delete featdata;
    featdata = NULL;
  }
  if (scoredata) {
    delete scoredata;
    scoredata = NULL;
  }
}

void Data::loadnbest(const std::string &file)
{
  //TRACE_ERR("loading nbest from " << file << std::endl);

  FeatureStats featentry;
  ScoreStats scoreentry;
  std::string sentence_index;

  inputfilestream inp(file); // matches a stream with a file. Opens the file

  if (!inp.good())
    throw runtime_error("Unable to open: " + file);

  std::string substring, subsubstring, stringBuf;
  std::string theSentence;
  std::string::size_type loc;

  while (getline(inp,stringBuf,'\n')) {
    if (stringBuf.empty()) continue;

//              TRACE_ERR("stringBuf: " << stringBuf << std::endl);

    getNextPound(stringBuf, substring, "|||"); //first field
    sentence_index = substring;

    getNextPound(stringBuf, substring, "|||"); //second field
    theSentence = substring;

    // adding statistics for error measures
    featentry.reset();
    scoreentry.clear();

    theScorer->prepareStats(sentence_index, theSentence, scoreentry);

    scoredata->add(scoreentry, sentence_index);

    getNextPound(stringBuf, substring, "|||"); //third field

    // examine first line for name of features
    if (!existsFeatureNames()) {
      std::string stringsupport=substring;
      std::string features="";
      std::string tmpname="";

      size_t tmpidx=0;
      while (!stringsupport.empty()) {
        //                      TRACE_ERR("Decompounding: " << substring << std::endl);
        getNextPound(stringsupport, subsubstring);

        // string ending with ":" are skipped, because they are the names of the features
        if ((loc = subsubstring.find_last_of(":")) != subsubstring.length()-1) {
          features+=tmpname+"_"+stringify(tmpidx)+" ";
          tmpidx++;
        }
        // ignore sparse feature name
        else if (subsubstring.find("_") != string::npos) {
          // also ignore its value
          getNextPound(stringsupport, subsubstring);
        }
        // update current feature name
        else {
          tmpidx=0;
          tmpname=subsubstring.substr(0,subsubstring.size() - 1);
        }
      }

      featdata->setFeatureMap(features);
    }

    // adding features
    while (!substring.empty()) {
//                      TRACE_ERR("Decompounding: " << substring << std::endl);
      getNextPound(substring, subsubstring);

      // no ':' -> feature value that needs to be stored
      if ((loc = subsubstring.find_last_of(":")) != subsubstring.length()-1) {
        featentry.add(ConvertStringToFeatureStatsType(subsubstring));
      }
      // sparse feature name? store as well
      else if (subsubstring.find("_") != string::npos) {
        std::string name = subsubstring;
        getNextPound(substring, subsubstring);
        featentry.addSparse( name, atof(subsubstring.c_str()) );
      }
    }
    //cerr << "number of sparse features: " << featentry.getSparse().size() << endl;
    featdata->add(featentry,sentence_index);
  }

  inp.close();
}


void Data::createShards(size_t shard_count, float shard_size, const string& scorerconfig,
                        std::vector<Data>& shards)
{
  CHECK(shard_count);
  CHECK(shard_size >= 0);
  CHECK(shard_size <= 1);

  size_t data_size = scoredata->size();
  CHECK(data_size == featdata->size());

  shard_size *= data_size;

  for (size_t shard_id = 0; shard_id < shard_count; ++shard_id) {
    vector<size_t> shard_contents;
    if (shard_size == 0) {
      //split into roughly equal size shards
      size_t shard_start = floor(0.5 + shard_id * (float)data_size / shard_count);
      size_t shard_end = floor(0.5 + (shard_id+1) * (float)data_size / shard_count);
      for (size_t i = shard_start; i < shard_end; ++i) {
        shard_contents.push_back(i);
      }
    } else {
      //create shards by randomly sampling
      for (size_t i = 0; i < floor(shard_size+0.5); ++i) {
        shard_contents.push_back(rand() % data_size);
      }
    }

    Scorer* scorer = ScorerFactory::getScorer(score_type, scorerconfig);

    shards.push_back(Data(*scorer));
    shards.back().score_type = score_type;
    shards.back().number_of_scores = number_of_scores;
    for (size_t i = 0; i < shard_contents.size(); ++i) {
      shards.back().featdata->add(featdata->get(shard_contents[i]));
      shards.back().scoredata->add(scoredata->get(shard_contents[i]));
    }
    //cerr << endl;
  }
}
