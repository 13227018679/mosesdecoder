// $Id$

#include "InputTreeRep.h"
#include "Util.h"
#include "XmlTree.h"
#include "FactorCollection.h"

using namespace std;

namespace Moses
{

/**
 * Process parsed sentence, build data structure
 * to query for getting label for different spans
*/

SyntaxLabel::SyntaxLabel(const std::string &label, bool isNonTerm)
{
	m_label = label;
	m_nonTerm = isNonTerm;
}

InputTreeRep::InputTreeRep(size_t sourceSize)
{
  const string noLabel = "X";
  SyntaxLabel syntNoLabel(noLabel,true);
  for (size_t startPos = 0; startPos < sourceSize; ++startPos) {
      //cerr << "startPos" << startPos << endl;
      vector<vector<SyntaxLabel> > internal;
      for (size_t endPos = startPos; endPos < sourceSize; ++endPos) {
        //cerr << "endPos" << endPos << endl;
        vector<SyntaxLabel> oneCell;
        oneCell.push_back(syntNoLabel);
        internal.push_back(oneCell);
    }
    m_sourceChart.push_back(internal);
  }
}

vector<SyntaxLabel> InputTreeRep::GetParent(size_t startPos, size_t endPos)
{
    if(startPos == 0)
    {
        if(endPos == (m_sourceChart.front().size() -1) )
        {
            return GetLabels(startPos,endPos);
        }
        else
        {
            return GetLabels(startPos,endPos+1);
        }
    }
    else
    {
        return GetLabels(startPos-1,endPos+1);
    }
}

//A copy of the same method in InputTree
bool InputTreeRep::ProcessAndStripXMLTags(string &line, std::vector<XMLParseOutputForTrain> &sourceLabels)
{
  //parse XML markup in translation line
  // no xml tag? we're done.
  if (line.find_first_of('<') == string::npos) {
    return true;
  }

  // break up input into a vector of xml tags and text
  // example: (this), (<b>), (is a), (</b>), (test .)
  vector<string> xmlTokens = MosesTraining::TokenizeXml(line);

  // we need to store opened tags, until they are closed
  // tags are stored as tripled (tagname, startpos, contents)
  typedef pair< string, pair< size_t, string > > OpenedTag;
  vector< OpenedTag > tagStack; // stack that contains active opened tags

  string cleanLine; // return string (text without xml)
  size_t wordPos = 0; // position in sentence (in terms of number of words)

  // loop through the tokens
  for (size_t xmlTokenPos = 0 ; xmlTokenPos < xmlTokens.size() ; xmlTokenPos++) {
    // not a xml tag, but regular text (may contain many words)
    if(!MosesTraining::isXmlTag(xmlTokens[xmlTokenPos])) {
      // add a space at boundary, if necessary
      if (cleanLine.size()>0 &&
          cleanLine[cleanLine.size() - 1] != ' ' &&
          xmlTokens[xmlTokenPos][0] != ' ') {
        cleanLine += " ";
      }
      cleanLine += xmlTokens[xmlTokenPos]; // add to output
      wordPos = Tokenize(cleanLine).size(); // count all the words
    }

    // process xml tag
    else {
      // *** get essential information about tag ***

      // strip extra boundary spaces and "<" and ">"
      string tag =  MosesTraining::Trim(MosesTraining::TrimXml(xmlTokens[xmlTokenPos]));
      //VERBOSE(3,"XML TAG IS: " << tag << std::endl);

      if (tag.size() == 0) {
        //TRACE_ERR("ERROR: empty tag name: " << line << endl);
        return false;
      }

      // check if unary (e.g., "<wall/>")
      bool isUnary = ( tag[tag.size() - 1] == '/' );

      // check if opening tag (e.g. "<a>", not "</a>")g
      bool isClosed = ( tag[0] == '/' );
      bool isOpen = !isClosed;

      if (isClosed && isUnary) {
        //TRACE_ERR("ERROR: can't have both closed and unary tag <" << tag << ">: " << line << endl);
        return false;
      }

      if (isClosed)
        tag = tag.substr(1); // remove "/" at the beginning
      if (isUnary)
        tag = tag.substr(0,tag.size()-1); // remove "/" at the end

      // find the tag name and contents
      string::size_type endOfName = tag.find_first_of(' ');
      string tagName = tag;
      string tagContent = "";
      if (endOfName != string::npos) {
        tagName = tag.substr(0,endOfName);
        tagContent = tag.substr(endOfName+1);
      }

      // *** process new tag ***

      if (isOpen || isUnary) {
        // put the tag on the tag stack
        OpenedTag openedTag = make_pair( tagName, make_pair( wordPos, tagContent ) );
        tagStack.push_back( openedTag );
        //VERBOSE(3,"XML TAG " << tagName << " (" << tagContent << ") added to stack, now size " << tagStack.size() << endl);
      }

      // *** process completed tag ***

      if (isClosed || isUnary) {
        // pop last opened tag from stack;
        if (tagStack.size() == 0) {
          //TRACE_ERR("ERROR: tag " << tagName << " closed, but not opened" << ":" << line << endl);
          return false;
        }
        OpenedTag openedTag = tagStack.back();
        tagStack.pop_back();

        // tag names have to match
        if (openedTag.first != tagName) {
          //TRACE_ERR("ERROR: tag " << openedTag.first << " closed by tag " << tagName << ": " << line << endl );
          return false;
        }

        // assemble remaining information about tag
        size_t startPos = openedTag.second.first;
        string tagContent = openedTag.second.second;
        size_t endPos = wordPos;

        // span attribute overwrites position
        string span = MosesTraining::ParseXmlTagAttribute(tagContent,"span");
        if (! span.empty()) {
          vector<string> ij = Tokenize(span, "-");
          if (ij.size() != 1 && ij.size() != 2) {
            //TRACE_ERR("ERROR: span attribute must be of the form \"i-j\" or \"i\": " << line << endl);
            return false;
          }
          startPos = atoi(ij[0].c_str());
          if (ij.size() == 1) endPos = startPos + 1;
          else endPos = atoi(ij[1].c_str()) + 1;
        }

        //VERBOSE(3,"XML TAG " << tagName << " (" << tagContent << ") spanning " << startPos << " to " << (endPos-1) << " complete, commence processing" << endl);

        if (startPos >= endPos) {
          //TRACE_ERR("ERROR: tag " << tagName << " must span at least one word: " << line << endl);
          return false;
        }

        // may be either a input span label ("label"), or a specified output translation "translation"
        string label = MosesTraining::ParseXmlTagAttribute(tagContent,"label");
        //std::cerr << "Obtained Label : " << label << std::endl;

        //no need to do something with translation here
        //string translation = ParseXmlTagAttribute(tagContent,"translation");

        // specified label
        if (label.length() > 0) {
          WordsRange range(startPos,endPos-1); // really?
          XMLParseOutputForTrain item(label, range);
          sourceLabels.push_back(item);
        }
      }
    }
  }
  // we are done. check if there are tags that are still open
  if (tagStack.size() > 0) {
    //TRACE_ERR("ERROR: some opened tags were never closed: " << line << endl);
    return false;
  }

  // return de-xml'ed sentence in line
  line = cleanLine;
  return true;
}

int InputTreeRep::Read(std::string &line)
{
  //std::cerr << "Reading in parse tree..." << line << std::endl;

  std::vector<XMLParseOutputForTrain> sourceLabels;
  ProcessAndStripXMLTags(line, sourceLabels);

  //tokenize line
  //std::cerr << "Sentence string : " << line << std::cerr;
  m_sourceSentence = Tokenize(" ",line);

  // size input chart
  size_t sourceSize = m_sourceSentence.size();

  //std::cerr << "Size of source sentence : " << sourceSize << std::endl;

  // default label

  // do source labels
  vector<XMLParseOutputForTrain>::const_iterator iterLabel;
  for (iterLabel = sourceLabels.begin(); iterLabel != sourceLabels.end(); ++iterLabel) {
    const XMLParseOutputForTrain &labelItem = *iterLabel;
    const WordsRange &range = labelItem.m_range;
    const string &label = labelItem.m_label;
    SyntaxLabel syntLabel(label,true);

    //std::cerr << "Adding chart label : " << range.GetStartPos() << " : "<< range.GetEndPos() << std::endl;

    AddChartLabel(range.GetStartPos(), range.GetEndPos(), syntLabel);
  }

  return 1;
}

void InputTreeRep::AddChartLabel(size_t startPos, size_t endPos, SyntaxLabel &label)
{
  CHECK(label.IsNonTerm());

  //cerr << "Add at positions : " << startPos << " : "<< endPos << endl;
  m_sourceChart[startPos][endPos-startPos].push_back(label);
}

void InputTreeRep::Print(std::ostream &out)
{
  size_t size = m_sourceSentence.size();
  for (size_t startPos = 0; startPos < size; ++startPos) {
    for (size_t endPos = startPos; endPos < size; ++endPos) {
      vector<SyntaxLabel> &labelSet = m_sourceChart[startPos][endPos - startPos];
      vector<SyntaxLabel>::iterator iter;
      for (iter = labelSet.begin(); iter != labelSet.end(); ++iter) {
        SyntaxLabel sLabel = *iter;
        out << "[" << startPos <<"," << endPos << "]="
            << sLabel.GetString() << "(" << sLabel.IsNonTerm() << ") ";
        CHECK(sLabel.IsNonTerm());
      }
    }
  }
}


} // namespace

