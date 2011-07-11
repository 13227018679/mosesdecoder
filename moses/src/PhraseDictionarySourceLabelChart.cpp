
#include "PhraseDictionarySourceLabel.h"
#include "FactorCollection.h"
#include "InputType.h"
#include "ChartRuleCollection.h"
#include "CellCollection.h"
#include "DotChart.h"
#include "StaticData.h"
#include "ChartInput.h"

using namespace std;
using namespace Moses;

Word PhraseDictionarySourceLabel::CreateCoveredWord(const Word &origSourceLabel, const InputType &src, const WordsRange &range) const
{
	string coveredWordsString = origSourceLabel.GetFactor(0)->GetString();

	for (size_t pos = range.GetStartPos(); pos <= range.GetEndPos(); ++pos)
	{
		const Word &word = src.GetWord(pos);
		coveredWordsString += "_" + word.GetFactor(0)->GetString();
	}
	
	FactorCollection &factorCollection = FactorCollection::Instance();
	
	Word ret;
	
	const Factor *factor = factorCollection.AddFactor(Input, 0, coveredWordsString, true);
	ret.SetFactor(0, factor);
	
	return ret;
}

const ChartRuleCollection *PhraseDictionarySourceLabel::GetChartRuleCollection(InputType const& src
																																							 , WordsRange const& range
																																							 , bool adhereTableLimit
																																							 , const CellCollection &cellColl
																																							 , size_t maxDefaultSpan
																																							 , size_t maxSourceSyntaxSpan
																																							 , size_t maxTargetSyntaxSpan) const
{
	ChartRuleCollection *ret = new ChartRuleCollection();
	m_chartTargetPhraseColl.push_back(ret);

	size_t relEndPos = range.GetEndPos() - range.GetStartPos();
	size_t absEndPos = range.GetEndPos();

	// MAIN LOOP. create list of nodes of target phrases
	ProcessedRuleStack &runningNodes = *m_runningNodesVec[range.GetStartPos()];

	const ProcessedRuleStack::SavedNodeColl &savedNodeColl = runningNodes.GetSavedNodeColl();
	for (size_t ind = 0; ind < savedNodeColl.size(); ++ind)
	{
		const SavedNode &savedNode = *savedNodeColl[ind];
		const ProcessedRule &prevProcessedRule = savedNode.GetProcessedRule();
		const PhraseDictionaryNodeSourceLabel &prevNode = static_cast<const PhraseDictionaryNodeSourceLabel &>(prevProcessedRule.GetLastNode());
		const WordConsumed *prevWordConsumed = prevProcessedRule.GetLastWordConsumed();
		size_t startPos = (prevWordConsumed == NULL) ? range.GetStartPos() : prevWordConsumed->GetWordsRange().GetEndPos() + 1;

		// search for terminal symbol
		if (startPos == absEndPos)
		{
			const Word &sourceWord = src.GetWord(absEndPos);
			const PhraseDictionaryNodeSourceLabel *node = prevNode.GetChild(sourceWord, sourceWord);
			if (node != NULL)
			{
				const Word &sourceWord = node->GetSourceWord();
				WordConsumed *newWordConsumed = new WordConsumed(absEndPos, absEndPos
																													, sourceWord, NULL
																													, prevWordConsumed);
				ProcessedRule *processedRule = new ProcessedRule(*node, newWordConsumed);
				runningNodes.Add(relEndPos+1, processedRule);
			}
		}

		// search for non-terminals
		size_t endPos, stackInd;
		if (startPos > absEndPos)
			continue;
		else if (startPos == range.GetStartPos() && range.GetEndPos() > range.GetStartPos())
		{ // start.
			endPos = absEndPos - 1;
			stackInd = relEndPos;
		}
		else
		{
			endPos = absEndPos;
			stackInd = relEndPos + 1;
		}

		// get headwords in this span from chart
		const vector<const Word*> headWords = cellColl.GetTargetLHSList(WordsRange(startPos, endPos));
				
		// go thru each source span
		const ChartInput &chartInput = static_cast<const ChartInput&>(src);
		const LabelList &labelList = chartInput.GetLabelList(startPos, endPos);
		
		LabelList::const_iterator iterLabelList;
		for (iterLabelList = labelList.begin(); iterLabelList != labelList.end(); ++iterLabelList)
		{
			const Word &sourceLabel = *iterLabelList;
			
			// go thru each headword & see if in phrase table
			vector<const Word*>::const_iterator iterHeadWords;
			for (iterHeadWords = headWords.begin(); iterHeadWords != headWords.end(); ++iterHeadWords)
			{
				const Word &targetLabel = **iterHeadWords;
				
				const PhraseDictionaryNodeSourceLabel *node = prevNode.GetChild(targetLabel, sourceLabel);
				if (node != NULL)
				{
					//const Word &sourceWord = node->GetSourceWord();
					WordConsumed *newWordConsumed = new WordConsumed(startPos, endPos
																														, targetLabel, &sourceLabel
																														, prevWordConsumed);

					ProcessedRule *processedRule = new ProcessedRule(*node, newWordConsumed);
					runningNodes.Add(stackInd, processedRule);
				}
			} // for (iterHeadWords

			// backoff 
			Word backoffLabel = CreateCoveredWord(sourceLabel, src, WordsRange(startPos, endPos));			
			
			for (iterHeadWords = headWords.begin(); iterHeadWords != headWords.end(); ++iterHeadWords)
			{
				const Word &headWord = **iterHeadWords;
				
				const PhraseDictionaryNodeSourceLabel *node = prevNode.GetChild(headWord, backoffLabel);
				if (node != NULL)
				{
					//const Word &sourceWord = node->GetSourceWord();
					WordConsumed *newWordConsumed = new WordConsumed(startPos, endPos
																													 , headWord, NULL
																													 , prevWordConsumed);
					
					ProcessedRule *processedRule = new ProcessedRule(*node, newWordConsumed);
					runningNodes.Add(stackInd, processedRule);
				}
			} // for (iterHeadWords
			
		} // for (iterLabelList 
	}

	// return list of target phrases
	ProcessedRuleColl &nodes = runningNodes.Get(relEndPos + 1);
	DeleteDuplicates(nodes);
		
	size_t rulesLimit = StaticData::Instance().GetRuleLimit();
	ProcessedRuleColl::const_iterator iterNode;
	for (iterNode = nodes.begin(); iterNode != nodes.end(); ++iterNode)
	{
		const ProcessedRule &processedRule = **iterNode;
		const PhraseDictionaryNodeSourceLabel &node = static_cast<const PhraseDictionaryNodeSourceLabel &>(processedRule.GetLastNode());
		const WordConsumed *wordConsumed = processedRule.GetLastWordConsumed();
		assert(wordConsumed);

		const TargetPhraseCollection *targetPhraseCollection = node.GetTargetPhraseCollection();

		if (targetPhraseCollection != NULL)
		{
			ret->Add(*targetPhraseCollection, *wordConsumed, adhereTableLimit, rulesLimit);
		}
	}
	ret->CreateChartRules(rulesLimit);
	
	return ret;
}

void PhraseDictionarySourceLabel::DeleteDuplicates(ProcessedRuleColl &nodes) const
{
	map<size_t, float> minEntropy;
	map<size_t, float>::iterator iterEntropy;
	
	// find out min entropy for each node id
	ProcessedRuleColl::iterator iter;
	for (iter = nodes.begin(); iter != nodes.end(); ++iter)
	{
		const ProcessedRule *processedRule = *iter;
		const PhraseDictionaryNodeSourceLabel &node = static_cast<const PhraseDictionaryNodeSourceLabel&> (processedRule->GetLastNode());
		size_t nodeId = node.GetId();
		float entropy = node.GetEntropy();
		
		iterEntropy = minEntropy.find(nodeId);
		if (iterEntropy == minEntropy.end())
		{
			minEntropy[nodeId] = entropy;
		}
		else
		{
			float origEntropy = minEntropy[nodeId];
			if (entropy < origEntropy)
			{
				minEntropy[nodeId] = entropy;
			}
		}
	}
		
	// delete nodes which are over min entropy
	size_t ind = 0;
	while (ind < nodes.GetSize())
	{
		const ProcessedRule &processedRule = nodes.Get(ind);
		const PhraseDictionaryNodeSourceLabel &node = static_cast<const PhraseDictionaryNodeSourceLabel&> (processedRule.GetLastNode());
		size_t nodeId = node.GetId();
		float entropy = node.GetEntropy();
		float minEntropy1 = minEntropy[nodeId];
		
		if (entropy > minEntropy1)
		{
			nodes.Delete(ind);
		}
		else
		{
			ind++;
		}
	}
}


