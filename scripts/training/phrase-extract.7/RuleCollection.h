#pragma once
/*
 *  RuleCollection.h
 *  extract
 *
 *  Created by Hieu Hoang on 19/07/2010.
 *  Copyright 2010 __MyCompanyName__. All rights reserved.
 *
 */
#include <set>
#include <iostream>
#include "Rule.h"

class SentenceAlignment;

// helper for sort. Don't compare default non-terminals
struct CompareRule
{
 	bool operator() (const Rule *a, const Rule *b)
  {
		/*
		if (g_debug)
		{
			std::cerr << std::endl << (*a) << std::endl << (*b) << " ";
		}
		 */
		bool ret = (*a) < (*b);
		/*
		if (g_debug)
		{
			std::cerr << ret << std::endl;
		}
		 */
		return ret;
 	}
};


class RuleCollection
{
	friend std::ostream& operator<<(std::ostream &out, const RuleCollection &obj);

protected:
	typedef std::set<const Rule*, CompareRule> CollType;
	CollType m_coll;
	
public:
	~RuleCollection();
	void Add(Rule *rule, const SentenceAlignment &sentence);
	size_t GetSize() const
	{ return m_coll.size(); }
};

