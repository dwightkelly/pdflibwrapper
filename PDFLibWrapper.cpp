/*
 *  PDFLibWrapper.cpp
 *
 *  Copyright (c) 2014-2015, Apago, Inc. All rights reserved.
 *
 */

#include <fstream>

#include <boost/thread.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/ptr_container/ptr_vector.hpp>
#include <boost/assign/list_of.hpp>

#ifdef PDFLIB_USE_CONTENTS_PARSER
#include "apago/ContentsParser.h"
#endif // PDFLIB_USE_CONTENTS_PARSER

#include "PDFLibWrapper.h"


namespace  {

	using namespace PDFLibWrapper;

	typedef boost::shared_mutex Mutex;
	typedef boost::upgrade_lock<Mutex> ROLock;
	typedef boost::upgrade_to_unique_lock<Mutex> RWLock;

	class StringPointerWrapper  {
	public:
		StringPointerWrapper(const std::string &sValue)
			: m_pString(new std::string(sValue))
		{ }
		bool operator<(const StringPointerWrapper &rOther) const
		{ return *m_pString < *rOther.m_pString; }
		bool operator==(const std::string &sOther) const
		{ return *m_pString == sOther; }
		bool operator!=(const std::string &sOther) const
		{ return *m_pString != sOther; }
		std::string *m_pString;
	};
	typedef std::map<StringPointerWrapper, std::pair<unsigned int, std::string *> > NameStringMap;
	typedef boost::ptr_vector<std::string> NameStringVector;
	typedef std::map<std::string, Object::Type> TypeMap;

	int g_nUnassignedToken = boost::integer_traits<unsigned int>::const_max;
	Mutex g_mtxNameStrings;
	NameStringMap g_mNameStrings;
	NameStringVector g_vNameStrings;
	TypeMap g_mTypeMap = boost::assign::map_list_of
		("CosNull", Object::kNull)
		("CosArray", Object::kArray)
		("CosBoolean", Object::kBoolean)
		("CosBool", Object::kBoolean)  // used in DVA
		("CosDict", Object::kDict)
		("CosFixed", Object::kFixed)
		("CosInteger", Object::kInteger)
		("CosName", Object::kName)
		("CosNull", Object::kNull)
		("CosStream", Object::kStream)
		("CosString", Object::kString);

	Document::Ptr g_pMasterDoc;
	std::string g_sMasterName;
	bool g_bAutoRegistered = PDFLibWrapper::Document::AutoRegister();
}

namespace PDFLibWrapper  {

struct Document::Impl  {
	Impl(const std::string &sFileName) : m_sFileName(sFileName)  { }
	std::string m_sFileName;
	Object::Ptr m_pTrailer;
};

Name::Name(const char *szName /*= NULL*/)
: m_nToken(g_nUnassignedToken), m_pString(NULL)
{
	if (szName)  {
		Set(szName);
	}
}

Name::Name(const std::string &sName)
: m_nToken(g_nUnassignedToken), m_pString(NULL)
{
	Set(sName);
}

void
Name::Set(const char *szName)
{
	Set(std::string(szName));
}

void
Name::Set(const std::string &sName)
{
	ROLock lck(g_mtxNameStrings);
	NameStringMap::iterator itFind = g_mNameStrings.lower_bound(sName);
	if (itFind == g_mNameStrings.end() || itFind->first != sName)  {
		RWLock lckWriting(lck);
		StringPointerWrapper oNewString(sName);
		m_pString = oNewString.m_pString;
		m_nToken = g_vNameStrings.size();
		g_vNameStrings.push_back(oNewString.m_pString);
		NameStringMap::mapped_type oValue(m_nToken, m_pString);
		itFind = g_mNameStrings.insert(itFind,
			NameStringMap::value_type(oNewString, oValue));
	} else  {
		m_nToken = itFind->second.first;
		m_pString = itFind->second.second;
	}
}


Document::Document(const std::string &sFileName)
: m_pImpl(new Impl(sFileName))
{
}

Document::Ptr
Document::Open(const std::string &sFileName)
{
	Document::Ptr pNew(g_pMasterDoc->ClonePtr());

	if (!pNew->OpenFile(sFileName))  {
		pNew.reset();
	}

	return pNew;
}

bool
Document::GetTrailer(Object::Ptr &pTrailer) const
{
#ifdef PDFLIB_USE_CONTENTS_PARSER
	if (!m_pImpl->m_pTrailer && IsValid())  {
		std::ifstream is(m_pImpl->m_sFileName.c_str());
		if (is)  {
			const int nBufferIncrease = 16;
			is.seekg(0, std::ios_base::end);
			std::ifstream::pos_type nSize = is.tellg(), nPos;
			long long nXRefPos;
			std::vector<char> vBuffer;
			int nBufSize = nBufferIncrease;
			bool bFound = false, bFoundEOF = false, bFoundXRef = false;
			char *pFirst, *pCurrent, *pEnd;
			while (!bFoundXRef && !is.fail())  {
				vBuffer.resize((std::min<std::ifstream::pos_type>)(nSize, nBufSize));
				nPos = nSize;
				nPos -= vBuffer.size();
				is.seekg(nPos);
				pFirst = &vBuffer[0];
				is.read(pFirst, vBuffer.size());
				is.clear();
				pEnd = pFirst + vBuffer.size() - 1;
				pCurrent = pEnd - 1;
				if (!bFoundEOF)  {
					while (pCurrent != pFirst && *pCurrent != '%')  {
						--pCurrent;
					}
					if (pCurrent != pFirst)  {
						bFoundEOF = strncmp(pCurrent - 1, "%%EOF", 5) == 0;
					} else  {
						nBufSize += nBufferIncrease;
					}
				}
				if (bFoundEOF)  {
					while (pCurrent != pFirst && *pCurrent != 's')  {
						--pCurrent;
					}
					if (pCurrent != pFirst && !strncmp(pCurrent, "startxref", 9))  {
						pCurrent += 9;
						while (!isdigit(*pCurrent))  {
							++pCurrent;
						}
						nXRefPos = 0;
						while (isdigit(*pCurrent))  {
							nXRefPos *= 10;
							nXRefPos += (*pCurrent - '0');
							++pCurrent;
						}
						bFoundXRef = true;
						if (nXRefPos < nPos)  {
							nXRefPos -= nBufSize;
							if (nXRefPos < 0)  {
								nXRefPos = 0;
							}
							is.seekg(nXRefPos);
							pFirst = &vBuffer[0];
							is.read(pFirst, vBuffer.size());
							is.clear();
							pEnd = pFirst + vBuffer.size() - 1;
						}
					} else  {
						nBufSize += nBufferIncrease;
					}
				}
			}
			while (!bFound && !is.fail())  {
				pCurrent = strstr(pFirst, "trailer");
				if (pCurrent != NULL)  {
					if (pCurrent - pFirst > 0)  {
						vBuffer.erase(vBuffer.begin(), vBuffer.begin() + (pCurrent - pFirst));
					}
					bFound = strstr(pCurrent, "%%EOF") != NULL;
				}
				if (!bFound)  {
					nBufSize += nBufferIncrease;
					vBuffer.resize(nBufSize);
				}
			}
			if (bFound)  {
				Apago::PDF::DisplayList oDL;
				if (oDL.Parse((const unsigned char *)&vBuffer[0], vBuffer.size()))  {
					return true;
				}
			}
		}
	}
#endif // PDFLIB_USE_CONTENTS_PARSER

	return false;
}

bool
Document::Register(const std::string &sName, const Ptr &pDoc)
{
	if (!g_pMasterDoc)  {
		g_sMasterName = sName;
		g_pMasterDoc = pDoc;
		return true;
	}

	return false;
}


Object::Type
Object::GetType(const std::string &sType)
{
	TypeMap::const_iterator itType = g_mTypeMap.find(sType);
	if (itType != g_mTypeMap.end())  {
		return itType->second;
	}
	return kUnknown;
}

Object::Type
Object::GetTypeName(std::string &sName) const
{
	GetTypeName(m_eType, sName);
	return m_eType;
}

void
Object::GetTypeName(const Type &eType, std::string &sName)
{
	switch (eType)  {
		case kNull:  sName = "null";  break;
		case kBoolean:  sName = "boolean";  break;
		case kInteger:  sName = "integer";  break;
		case kFixed:  sName = "fixed-point";  break;
		case kName:  sName = "name";  break;
		case kString:  sName = "string";  break;
		case kStream:  sName = "stream";  break;
		case kArray:  sName = "array";  break;
		case kDict:  sName = "dictionary";  break;
		default:  sName = "unknown";  break;
	}
}


void
Object::GetObjectDescription(std::string &sDesc, Path &vPath, bool bNeedParent,
	bool bFullPath /*= false*/)

{
	sDesc.clear();
	Path::const_iterator itSelect = vPath.begin();
	if (!bFullPath)  {
		bool bFoundIndirect = false;
		itSelect = vPath.end();
		--itSelect;
		while (bNeedParent && itSelect->m_nObjectID == kInvalidID)  {
			if (bFoundIndirect)  {
				bNeedParent = false;
			}
			--itSelect;
			if (itSelect->m_nObjectID == kInvalidID)  {
				bFoundIndirect = true;
			}
		}
	}

	char szTemp[100];
	const PDFLibWrapper::Name *pName;
	Object *pLastObj = NULL, *pObj;
	bool bFirst = true;
	while (itSelect != vPath.end())  {
		pObj = itSelect->m_pObject;
		if (pLastObj != pObj)  {
			pLastObj = pObj;
			if (!bFirst)  {
				pName = itSelect->GetName();
				if (pName)  {
					if (pName->IsValid())  {
						sDesc += " /";
						sDesc += pName->GetString();
					}
				} else  {
					long nIndex = *itSelect->GetIndex();
					if (nIndex >= 0)  {
						sprintf(szTemp, "[%ld]", nIndex);
						sDesc += szTemp;
					}
				}
			} else  {
				bFirst = false;
			}
			if (!sDesc.empty())  {
				sDesc += ' ';
			}
			if (!pObj)  {
				sDesc += " (object not present)";
			} else if (pObj->IsIndirect())  {
				sprintf(szTemp, "(Object %ld)", pObj->GetID());
				sDesc += szTemp;
			} else  {
				sDesc += "(Object)";
			}
		}
		++itSelect;
	}
}

std::string
Object::GetString()
{
	std::stringstream ss;
	switch (m_eType)  {
		default:
			ss << "{unknown}";
			break;
		case kNull:
			ss << "null";
			break;
		case kBoolean:
			{
				bool bValue;
				if (Get(bValue))  {
					ss << std::boolalpha << bValue;
				}
			}
			break;
		case kInteger:
			{
				int nValue;
				if (Get(nValue))  {
					ss << nValue;
				}
			}
			break;
		case kFixed:
			{
				double dValue;
				if (Get(dValue))  {
					ss << dValue;
				}
			}
			break;
		case kName:
			{
				Name nmValue;
				if (Get(nmValue))  {
					ss << '/' << nmValue.GetString();
				}
			}
			break;
		case kString:
			{
				std::string sValue;
				if (Get(sValue))  {
					ss << '(' << sValue << ')';
				}
			}
			break;
		case kArray:
			ss << "[...]";
			break;
		case kDict:
			ss << "<<...>>";
			break;
		case kStream:
			ss << "{stream}";
			break;
	}

	return ss.str();
}


std::string
PDFVersion::AsString() const
{
	char szTemp[20];
	sprintf(szTemp, "%d.%d", m_nMajor, m_nMinor);
	return szTemp;
}

}
