/*
 *  WrapperTest.cpp
 *
 *  Copyright (c) 2014-2015, Apago, Inc. All rights reserved.
 *
 */

#include <iostream>

#include "PDFLibWrapper.h"

using namespace PDFLibWrapper;

struct InfoState  {
	typedef std::set<Object::ID> ObjectSet;

	int nDepth;
	int nIndent;
	bool bUseIndentInitially;
	ObjectSet *psetVisited;

	InfoState()
		: nDepth(1), nIndent(0), bUseIndentInitially(true), psetVisited(new ObjectSet)
	{ }
	InfoState(const InfoState &rOther)
		: nDepth(rOther.nDepth - 1), nIndent(rOther.nIndent + 3),
		  bUseIndentInitially(rOther.bUseIndentInitially),
		  psetVisited(rOther.psetVisited)
	{ }
};

void
PrintInfo(const Object::Ptr &pObject, InfoState &rState)
{
	std::string sType, sIndent;
	Object::Type eType = pObject->GetTypeName(sType);
	sType.insert(0, "{");
	sType += '}';
	if (rState.nIndent && rState.bUseIndentInitially)  {
		sIndent.assign(rState.nIndent - 1, ' ');
		std::cout << sIndent;
	}

	if (pObject->IsIndirect())  {
		std::cout << "(#" << pObject->GetID() << ") ";
		if (rState.psetVisited->find(pObject->GetID()) != rState.psetVisited->end())  {
			std::cout << "{previously printed}" << std::endl;
			return;
		}
		rState.psetVisited->insert(pObject->GetID());
	}

	if (rState.nIndent && !rState.bUseIndentInitially)  {
		sIndent.assign(rState.nIndent, ' ');
	}

	switch (eType)  {
		case Object::kNull:
			std::cout << "null";
			break;
		case Object::kBoolean:
			{
				bool bVal;
				if (pObject->Get(bVal))  {
					std::cout << std::boolalpha << bVal;
				}
			}
			break;
		case Object::kInteger:
			{
				int nVal;
				if (pObject->Get(nVal))  {
					std::cout << nVal;
				}
			}
			break;
		case Object::kFixed:
			{
				double dVal;
				if (pObject->Get(dVal))  {
					std::cout << dVal;
				}
			}
			break;
		case Object::kName:
			{
				Name nmVal;
				if (pObject->Get(nmVal))  {
					std::cout << '/' << nmVal;
				}
			}
			break;
		case Object::kString:
			{
				std::string sVal;
				if (pObject->Get(sVal))  {
					bool bUnicode = false;
					if (sVal.size() > 3 && (unsigned char)sVal[0] == 0xfe && (unsigned char)sVal[1] == 0xff)  {
						// Unicode string
						std::wstring wsValue;
						std::wstring::value_type cTemp;
						std::string::const_iterator itChar = sVal.begin(),
							itEndChars = sVal.end();
						itChar += 2;  // skip BOM
						wsValue.reserve(sVal.size() / 2 - 1);
						while (itChar != itEndChars)  {
							cTemp = *itChar << 8;
							if (cTemp)  {
								bUnicode = true;
							}
							cTemp += *++itChar;
							wsValue += cTemp;
							++itChar;
						}
						if (bUnicode)  {
							std::flush(std::cout);
							std::wcout << L'(' << wsValue << L')';
							std::flush(std::wcout);
						} else  {
							std::string sNarrow(wsValue.begin(), wsValue.end());
							sVal.swap(sNarrow);
						}
					}
					if (!bUnicode)  {
						std::cout << '(' << sVal << ')';
					}
				}
			}
			break;
		case Object::kStream:
			std::cout << sType << "  ";
			// fall through
		case Object::kDict:
			{
				NameSet setKeys;
				if (pObject->GetKeys(setKeys))  {
					NameSet::const_iterator itKey = setKeys.begin(),
						itEndKeys = setKeys.end();
					if (rState.nDepth > 0)  {
						Object::Ptr pValue;
						std::cout << " << ";
						if (!setKeys.empty())  {
							std::cout << std::endl;
						}
						std::string::size_type nLongest = 0;
						while (itKey != itEndKeys)  {
							nLongest = (std::max)(nLongest, itKey->GetString().size());
							++itKey;
						}
						std::string sKeyName;
						InfoState oSubState(rState);
						itKey = setKeys.begin();
						while (itKey != itEndKeys)  {
							sKeyName = itKey->GetString();
							if (pObject->Get(pValue, *itKey))  {
								bool bShort = false;
								switch (pValue->GetType())  {
									case Object::kArray:
									case Object::kDict:
									case Object::kStream:
										bShort = rState.nDepth > 1 &&
											(!pValue->IsIndirect() ||
											 rState.psetVisited->find(pValue->GetID()) == rState.psetVisited->end());
										break;
									default:
										break;
								}
								if (!bShort)  {
									sKeyName.append(nLongest - sKeyName.size() + 2, ' ');
								} else  {
									sKeyName += "  ";
								}
								std::cout << sIndent << "   /" << sKeyName;
								oSubState.bUseIndentInitially = false;
								PrintInfo(pValue, oSubState);
							}
							++itKey;
						}
						std::cout << sIndent << ">>" << std::endl;
					} else  {
						std::cout << "{dictionary} with " << setKeys.size() << " key(s)" << std::endl;
					}
				}
			}
			break;
		case Object::kArray:
			{
				int nLength = pObject->GetLength();
				if (rState.nDepth > 0)  {
					Object::Ptr pValue;
					std::cout << "[ ";
					if (nLength)  {
						std::cout << std::endl;
					}
					InfoState oSubState(rState);
					for (int i=0; i<nLength; i++)  {
						if (pObject->Get(pValue, i))  {
							oSubState.bUseIndentInitially = true;
							PrintInfo(pValue, oSubState);
						} else  {
							std::cout << sIndent << "{error}" << std::endl;
						}
					}
					std::cout << sIndent << "]" << std::endl;
				} else  {
					std::cout << "{array} with " << nLength << " element(s)" << std::endl;
				}
			}
			break;
	}
	switch (eType)  {
		default:
			std::cout << "   " << sType << std::endl;
			break;
		case Object::kDict:
		case Object::kArray:
		case Object::kStream:
			break;
	}
}

void
PrintInfo(const Object::Ptr &pObject, int nDepth = 1, int nIndent = 0,
	bool bUseIndentInitially = true)
{
	InfoState oState;
	oState.nDepth = nDepth;
	oState.nIndent = nIndent;
	oState.bUseIndentInitially = bUseIndentInitially;
	PrintInfo(pObject, oState);
}


int main(int argc, char **argv)
{
	if (argc < 2 || argc > 3)  {
		std::cerr << "Usage:  " << argv[0] << " filename [depth]" << std::endl;
		return 1;
	}
	int nDepth = 1;
	if (argc == 3)  {
		if (sscanf(argv[2], "%d", &nDepth) != 1 || nDepth < 0)  {
			std::cerr << "Usage:  " << argv[0] << " filename [depth]" << std::endl;
			return 1;
		}
	}

	Document::Ptr pDoc = Document::Open(argv[1]);
	Object::Ptr pCatalog;
	if (!pDoc || !pDoc->GetCatalog(pCatalog))  {
		if (!pDoc)  {
			std::cerr << "Error opening document" << std::endl;
		} else  {
			std::cerr << "Error getting PDF catalog" << std::endl;
		}
		return 1;
	}

	std::cout << "Document catalog:" << std::endl;
	PrintInfo(pCatalog, nDepth);

	return 0;
}
