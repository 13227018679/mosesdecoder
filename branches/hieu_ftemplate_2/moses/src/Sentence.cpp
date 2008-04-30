// $Id$
// vim:tabstop=2

/***********************************************************************
Moses - factored phrase-based language decoder
Copyright (C) 2006 University of Edinburgh

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

#include "Sentence.h"
#include "PhraseDictionaryMemory.h"
#include "TranslationOptionCollectionText.h"
#include "StaticData.h"
#include "Util.h"

int Sentence::Read(std::istream& in,const std::vector<FactorType>& factorOrder) 
{
	const std::string& factorDelimiter = StaticData::Instance().GetFactorDelimiter();
	std::string line;
	std::map<std::string, std::string> meta;

	if (getline(in, line, '\n').eof())	
			return 0;
	line = Trim(line);
  meta = ProcessAndStripSGML(line);

	if (meta.find("id") != meta.end()) { this->SetTranslationId(atol(meta["id"].c_str())); }
	
	//parse XML markup in translation line
	const StaticData &staticData = StaticData::Instance();
	if (staticData.GetXmlInputType() != XmlPassThrough)
		m_xmlOptionsList = ProcessAndStripXMLTags(line);
	Phrase::CreateFromString(factorOrder, line, factorDelimiter);
	
	//only fill the vector if we are parsing XML
	if (staticData.GetXmlInputType() != XmlPassThrough ) {
		for (size_t i=0; i<GetSize();i++) {
			m_xmlCoverageMap.push_back(false);
		}
		for (size_t i=0; i< m_xmlOptionsList.size();i++) {
			//m_xmlOptionsList will be empty for XmlIgnore
			for(size_t j=m_xmlOptionsList[i].startPos;j<=m_xmlOptionsList[i].endPos;j++) {
				m_xmlCoverageMap[j]=true;
				
			}
		}
	}
		
	return 1;
}

TranslationOptionCollection* 
Sentence::CreateTranslationOptionCollection() const 
{
	size_t maxNoTransOptPerCoverage = StaticData::Instance().GetMaxNoTransOptPerCoverage();
	TranslationOptionCollection *rv= new TranslationOptionCollectionText(*this, maxNoTransOptPerCoverage);
	assert(rv);
	return rv;
}
void Sentence::Print(std::ostream& out) const
{
	out<<*static_cast<Phrase const*>(this)<<"\n";
}


bool Sentence::XmlOverlap(size_t startPos, size_t endPos) const {
	for (size_t pos = startPos; pos <=  endPos ; pos++)
		{
			if (pos < m_xmlCoverageMap.size() && m_xmlCoverageMap[pos]) {
				return true;
				}
		}
		return false;
}

void Sentence::GetXmlTranslationOptions(std::vector <TranslationOption*> &list, size_t startPos, size_t endPos) const {
	//iterate over XmlOptions list, find exact source/target matches
	//we don't worry about creating the objects ahead of time because this should only be called once for each unique start/end when a given sentence is processed
	const std::vector<FactorType> &outputFactorOrder = StaticData::Instance().GetOutputFactorOrder();
	
	for(size_t i=0;i<m_xmlOptionsList.size();i++) {
		if (startPos == m_xmlOptionsList[i].startPos && endPos == m_xmlOptionsList[i].endPos) {
			//create TranslationOptions
			
			for (size_t j=0;j<m_xmlOptionsList[i].targetPhrases.size();j++) {
				TargetPhrase targetPhrase(Output);
				targetPhrase.CreateFromString(outputFactorOrder,m_xmlOptionsList[i].targetPhrases[j],StaticData::Instance().GetFactorDelimiter());
				targetPhrase.SetScore(m_xmlOptionsList[i].targetScores[j]);
				WordsRange range(m_xmlOptionsList[i].startPos,m_xmlOptionsList[i].endPos);
				
				TranslationOption *option = new TranslationOption(range,targetPhrase,*this, 0); // not sure what decode step it should be
				assert(option);
				list.push_back(option);

			}
		}
	}
}


std::string Sentence::ParseXmlTagAttribute(const std::string& tag,const std::string& attributeName){
	/*TODO deal with unescaping \"*/
	string tagOpen = attributeName + "=\"";
	size_t contentsStart = tag.find(tagOpen);
	if (contentsStart == std::string::npos) return "";
	contentsStart += tagOpen.size();
	size_t contentsEnd = tag.find_first_of('"',contentsStart+1);
	if (contentsEnd == std::string::npos) {
		TRACE_ERR("Malformed XML attribute: "<< tag);
		return "";	
	}
	size_t possibleEnd;
	while (tag.at(contentsEnd-1) == '\\' && (possibleEnd = tag.find_first_of('"',contentsEnd+1)) != std::string::npos) {
		contentsEnd = possibleEnd;
	}
	return tag.substr(contentsStart,contentsEnd-contentsStart);
}
