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

#pragma once

#if HAVE_CONFIG_H
#include "config.h"
#endif

#include <vector>
#include "Util.h"
#include "WordsRange.h"
#include "ScoreComponentCollection.h"
#include "Phrase.h"
#include "ChartTranslationOption.h"
#include "ObjectPool.h"

namespace Moses
{

class ChartHypothesis;
class ChartManager;
class RuleCubeItem;

typedef std::vector<ChartHypothesis*> ChartArcList;

class ChartHypothesis
{
  friend std::ostream& operator<<(std::ostream&, const ChartHypothesis&);

protected:
#ifdef USE_HYPO_POOL
  static ObjectPool<ChartHypothesis> s_objectPool;
#endif

  const TargetPhrase &m_targetPhrase;
  const ChartTranslationOption &m_transOpt;

  WordsRange					m_currSourceWordsRange;
	std::vector<const FFState*> m_ffStates; /*! stateful feature function states */
  //ScoreComponentCollection m_scoreBreakdown /*! detailed score break-down by components (for instance language model, word penalty, etc) */
  mutable std::auto_ptr<ScoreComponentCollection> m_scoreBreakdown; /*! detailed score break-down by components (for instance language model, word penalty, etc) */
  ScoreComponentCollection m_currScoreBreakdown /*! scores for this hypothesis */
  ,m_lmNGram
  ,m_lmPrefix;
  float m_totalScore;

  ChartArcList 					*m_arcList; /*! all arcs that end at the same trellis point as this hypothesis */
  const ChartHypothesis 	*m_winningHypo;

  std::vector<const ChartHypothesis*> m_prevHypos;

  ChartManager& m_manager;

  unsigned m_id; /* pkoehn wants to log the order in which hypotheses were generated */

  mutable std::auto_ptr<ScoreComponentCollection> m_lmScoreBreakdown;
  ScoreComponentCollection m_lmCurrScoreBreakdown;

  LMList m_lmList;

  float m_lmScore;

  ChartHypothesis(); // not implemented
  ChartHypothesis(const ChartHypothesis &copy); // not implemented

public:
#ifdef USE_HYPO_POOL
  void *operator new(size_t /* num_bytes */) {
    void *ptr = s_objectPool.getPtr();
    return ptr;
  }

  static void Delete(ChartHypothesis *hypo) {
    s_objectPool.freeObject(hypo);
  }
#else
  static void Delete(ChartHypothesis *hypo) {
    delete hypo;
  }
#endif

  ChartHypothesis(const ChartTranslationOption &, const RuleCubeItem &item,
                  ChartManager &manager);

  ~ChartHypothesis();

  unsigned GetId() const { return m_id; }

  const ChartTranslationOption &GetTranslationOption()const {
    return m_transOpt;
  }
  const TargetPhrase &GetCurrTargetPhrase()const {
    return m_targetPhrase;
  }
  const WordsRange &GetCurrSourceRange()const {
    return m_currSourceWordsRange;
  }
  inline const ChartArcList* GetArcList() const {
    return m_arcList;
  }
	inline const FFState* GetFFState( size_t featureID ) const {
		return m_ffStates[ featureID ];
	}
	inline const ChartManager& GetManager() const { return m_manager; }

  void CreateOutputPhrase(Phrase &outPhrase) const;
  Phrase GetOutputPhrase() const;

	int RecombineCompare(const ChartHypothesis &compare) const;

  void CalcScore();

  void AddArc(ChartHypothesis *loserHypo);
  void CleanupArcList();
  void SetWinningHypo(const ChartHypothesis *hypo);

  const ScoreComponentCollection &GetScoreBreakdown() const {
  	if (!m_scoreBreakdown.get()) {
      m_scoreBreakdown.reset(new ScoreComponentCollection(m_currScoreBreakdown));

      std::vector<const ChartHypothesis*>::const_iterator iter;
      for (iter = m_prevHypos.begin(); iter != m_prevHypos.end(); ++iter) {
        const ChartHypothesis &prevHypo = **iter;

        ScoreComponentCollection prevScoreBreakdown = prevHypo.GetScoreBreakdown();
        // remove LM scores before adding previous score breakdown
        LMList::const_iterator lm_iter;
        for (lm_iter = m_lmList.begin(); lm_iter != m_lmList.end(); ++lm_iter) {
        	LanguageModel *lm = *lm_iter;
        	prevScoreBreakdown.Assign(lm, 0);
        }

        m_scoreBreakdown->PlusEquals(prevScoreBreakdown);
//        m_scoreBreakdown->PlusEquals(prevHypo.GetScoreBreakdown());
      }
    }

    return *m_scoreBreakdown;
  }

  float GetTotalScore() const {
    return m_totalScore;
  }

  float GetLMScore() const {
    return m_lmScore;
  }

  const std::vector<const ChartHypothesis*> &GetPrevHypos() const {
    return m_prevHypos;
  }

	const ChartHypothesis* GetPrevHypo(size_t pos) const {
		return m_prevHypos[pos];
	}

  const Word &GetTargetLHS() const {
    return GetCurrTargetPhrase().GetTargetLHS();
  }

	const ChartHypothesis* GetWinningHypothesis() const {
		return m_winningHypo;
	}

  TO_STRING();

}; // class ChartHypothesis

}

