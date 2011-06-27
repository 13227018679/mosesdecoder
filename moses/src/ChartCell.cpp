// $Id$
// vim:tabstop=2
/***********************************************************************
 Moses - factored phrase-based language decoder
 Copyright (C) 2010 Hieu Hoang

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.

 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 ***********************************************************************/

#include <algorithm>
#include "ChartCell.h"
#include "ChartTranslationOptionCollection.h"
#include "ChartCellCollection.h"
#include "RuleCubeQueue.h"
#include "RuleCube.h"
#include "WordsRange.h"
#include "Util.h"
#include "StaticData.h"
#include "ChartTranslationOption.h"
#include "ChartTranslationOptionList.h"

using namespace std;
using namespace Moses;

namespace Moses
{
extern bool g_debug;

ChartCell::ChartCell(size_t startPos, size_t endPos, ChartManager &manager)
  :m_coverage(startPos, endPos)
  ,m_manager(manager)
{
  const StaticData &staticData = StaticData::Instance();
  m_nBestIsEnabled = staticData.IsNBestEnabled();
}

/** Get all hypotheses in the cell that have the specified constituent label */
const HypoList &ChartCell::GetSortedHypotheses(const Word &constituentLabel) const
{
  std::map<Word, ChartHypothesisCollection>::const_iterator
  iter = m_hypoColl.find(constituentLabel);
  assert(iter != m_hypoColl.end());
  return iter->second.GetSortedHypotheses();
}

/** Add the given hypothesis to the cell */
bool ChartCell::AddHypothesis(ChartHypothesis *hypo)
{
  const Word &targetLHS = hypo->GetTargetLHS();
  return m_hypoColl[targetLHS].AddHypothesis(hypo, m_manager);
}

/** Pruning */
void ChartCell::PruneToSize()
{
  std::map<Word, ChartHypothesisCollection>::iterator iter;
  for (iter = m_hypoColl.begin(); iter != m_hypoColl.end(); ++iter) {
    ChartHypothesisCollection &coll = iter->second;
    coll.PruneToSize(m_manager);
  }
}

/** Decoding at span level: fill chart cell with hypotheses
 *  (implementation of cube pruning)
 * \param transOptList list of applicable rules to create hypotheses for the cell
 * \param allChartCells entire chart - needed to look up underlying hypotheses
 */
void ChartCell::ProcessSentence(const ChartTranslationOptionList &transOptList
                                , const ChartCellCollection &allChartCells)
{
  const StaticData &staticData = StaticData::Instance();

  // priority queue for applicable rules with selected hypotheses
  RuleCubeQueue queue;

  // add all trans opt into queue. using only 1st child node.
  ChartTranslationOptionList::const_iterator iterList;
  for (iterList = transOptList.begin(); iterList != transOptList.end(); ++iterList) 
  {
    const ChartTranslationOption &transOpt = **iterList;
    RuleCube *ruleCube = new RuleCube(transOpt, allChartCells);
    queue.Add(ruleCube);
  }

  // pluck things out of queue and add to hypo collection
  const size_t popLimit = staticData.GetCubePruningPopLimit();
  for (size_t numPops = 0; numPops < popLimit && !queue.IsEmpty(); ++numPops) 
  {
    RuleCube *ruleCube = queue.Pop();

    // create hypothesis from RuleCube
    ChartHypothesis *hypo = new ChartHypothesis(*ruleCube, m_manager);
    assert(hypo);
    hypo->CalcScore();
    AddHypothesis(hypo);

    // add neighbors to the queue
    ruleCube->CreateNeighbors(queue);
  }
}

void ChartCell::SortHypotheses()
{
  // sort each mini cells & fill up target lhs list
  assert(m_constituentLabelSet.empty());
  std::map<Word, ChartHypothesisCollection>::iterator iter;
  for (iter = m_hypoColl.begin(); iter != m_hypoColl.end(); ++iter) {
    m_constituentLabelSet.insert(iter->first);

    ChartHypothesisCollection &coll = iter->second;
    coll.SortHypotheses();
  }
}

/** Return the highest scoring hypothesis in the cell */
const ChartHypothesis *ChartCell::GetBestHypothesis() const
{
  const ChartHypothesis *ret = NULL;
  float bestScore = -std::numeric_limits<float>::infinity();

  std::map<Word, ChartHypothesisCollection>::const_iterator iter;
  for (iter = m_hypoColl.begin(); iter != m_hypoColl.end(); ++iter) {
    const HypoList &sortedList = iter->second.GetSortedHypotheses();
    assert(sortedList.size() > 0);

    const ChartHypothesis *hypo = sortedList[0];
    if (hypo->GetTotalScore() > bestScore) {
      bestScore = hypo->GetTotalScore();
      ret = hypo;
    };
  }

  return ret;
}

/** Is there a hypothesis in the cell that has the specified constituent label? */
bool ChartCell::ConstituentLabelExists(const Word &constituentLabel) const
{
  std::map<Word, ChartHypothesisCollection>::const_iterator iter;
  iter = m_hypoColl.find(constituentLabel);
  return (iter != m_hypoColl.end());
}

void ChartCell::CleanupArcList()
{
  // only necessary if n-best calculations are enabled
  if (!m_nBestIsEnabled) return;

  std::map<Word, ChartHypothesisCollection>::iterator iter;
  for (iter = m_hypoColl.begin(); iter != m_hypoColl.end(); ++iter) {
    ChartHypothesisCollection &coll = iter->second;
    coll.CleanupArcList();
  }
}

void ChartCell::OutputSizes(std::ostream &out) const
{
  std::map<Word, ChartHypothesisCollection>::const_iterator iter;
  for (iter = m_hypoColl.begin(); iter != m_hypoColl.end(); ++iter) {
    const Word &targetLHS = iter->first;
    const ChartHypothesisCollection &coll = iter->second;

    out << targetLHS << "=" << coll.GetSize() << " ";
  }
}

size_t ChartCell::GetSize() const
{
  size_t ret = 0;
  std::map<Word, ChartHypothesisCollection>::const_iterator iter;
  for (iter = m_hypoColl.begin(); iter != m_hypoColl.end(); ++iter) {
    const ChartHypothesisCollection &coll = iter->second;

    ret += coll.GetSize();
  }

  return ret;
}

void ChartCell::GetSearchGraph(long translationId, std::ostream &outputSearchGraphStream, const std::map<int,bool> &reachable) const
{
  std::map<Word, ChartHypothesisCollection>::const_iterator iterOutside;
  for (iterOutside = m_hypoColl.begin(); iterOutside != m_hypoColl.end(); ++iterOutside) {
    const ChartHypothesisCollection &coll = iterOutside->second;
    coll.GetSearchGraph(translationId, outputSearchGraphStream, reachable);
  }

}

std::ostream& operator<<(std::ostream &out, const ChartCell &cell)
{
  std::map<Word, ChartHypothesisCollection>::const_iterator iterOutside;
  for (iterOutside = cell.m_hypoColl.begin(); iterOutside != cell.m_hypoColl.end(); ++iterOutside) {
    const Word &targetLHS = iterOutside->first;
    cerr << targetLHS << ":" << endl;

    const ChartHypothesisCollection &coll = iterOutside->second;
    cerr << coll;
  }

  /*
  ChartCell::HCType::const_iterator iter;
  for (iter = cell.m_hypos.begin(); iter != cell.m_hypos.end(); ++iter)
  {
  	const ChartHypothesis &hypo = **iter;
  	out << hypo << endl;
  }
   */

  return out;
}

} // namespace


