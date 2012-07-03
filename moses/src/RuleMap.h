/*******************************************
DAMT Hiero : Features passed to classifier
********************************************/

#ifndef moses_RuleMap_h
#define moses_RuleMap_h

#include <vector>
#include <iterator>
#include <string>
#include "Word.h"
#include "AlignmentInfo.h"

#include <boost/unordered_map.hpp>

namespace Moses
{
    class RuleMap
    {
        protected :

        //strings to be changed into Moses::Word when working with factors
        boost::unordered_map< std::string,std::vector<std::string>* > m_targetsForSource;

        public :

        //iterators
        typedef boost::unordered_map< std::string,std::vector<std::string>* >::iterator iterator;
        typedef boost::unordered_map< std::string,std::vector<std::string>* >::const_iterator const_iterator;

         iterator begin() {
            return m_targetsForSource.begin();
         }
          iterator end() {
            return m_targetsForSource.end();
          }
          const_iterator begin() const {
            return m_targetsForSource.begin();
          }
          const_iterator end() const {
            return m_targetsForSource.end();
          }

        RuleMap(){};
        ~RuleMap();


        void AddRule(std::string&,std::string&);


    };
}
#endif
