/*
 *  PDFLWrapper.cpp
 *
 *  Copyright (c) 2014-2015, Apago, Inc. All rights reserved.
 *
 */

#include "PDFLWrapper.h"

#include <boost/optional.hpp>
#include <boost/thread.hpp>
#include <boost/thread/shared_mutex.hpp>
#include <boost/assign/list_of.hpp>

#ifdef ADOBE_PDFL
#include "PDFLInitCommon.h"
#else
#include "PDFInit.h"
#endif
#include "PDCalls.h"
#include "ASCalls.h"

namespace  {

	class PDFLInitter  {
	public:
		typedef boost::shared_mutex Mutex;

		static PDFLInitter *Get()  {
			static Mutex g_mtxPDFLInitter;
			static boost::scoped_ptr<PDFLInitter> g_pInstance;
			{
				boost::unique_lock<Mutex> lck(g_mtxPDFLInitter);
				if (!g_pInstance)  {
					g_pInstance.reset(new PDFLInitter);
				}
			}
			return g_pInstance.get();
		}

		bool IsValid() const  { return m_bInitted; }

		~PDFLInitter()  {
			if (m_bInitted)  {
				::PDFLTerm();
			}
		}

	private:
		PDFLInitter()
			: m_bInitted(false)
		{
			ASInt32 nRet = 0;
			PDFLDataRec thePDFLDataRec;
			DURING
				memset(&thePDFLDataRec, 0, sizeof(PDFLDataRec));
				thePDFLDataRec.size = sizeof(PDFLDataRec);
				nRet = ::PDFLInit(&thePDFLDataRec);
			HANDLER
				nRet = ERRORCODE;
			END_HANDLER

			m_bInitted = (nRet == 0);
		}

		bool m_bInitted;
	};


	using namespace PDFLibWrapper;

	typedef std::map<CosType, Object::Type> TypeMap;

	// work-around for compiling on OS X, since boost::assign::map_list_of
	// was having problems compiling
	TypeMap CreateTypeMap()  {
		TypeMap mTemp;
		mTemp[CosNull] = Object::kNull;
		mTemp[CosArray] = Object::kArray;
		mTemp[CosBoolean] = Object::kBoolean;
		mTemp[CosDict] = Object::kDict;
		mTemp[CosFixed] = Object::kFixed;
		mTemp[CosInteger] = Object::kInteger;
		mTemp[CosName] = Object::kName;
		mTemp[CosNull] = Object::kNull;
		mTemp[CosStream] = Object::kStream;
		mTemp[CosString] = Object::kString;
		return mTemp;
	}

	TypeMap g_mTypeMap = CreateTypeMap();

}


namespace PDFLibWrapper  {

bool
Document::AutoRegister()
{
	return PDFLibWrapper::Document::Register(
		"PDFL",
		PDFLibWrapper::Document::Ptr(new PDFLibWrapper::PDFLDoc(""))
		);
}

struct PDFLObject::Impl  {
	typedef std::map<ASAtom, Name> A2NMap;
	typedef std::map<Name, ASAtom> N2AMap;
	typedef std::map<Name, Object::Ptr> Dict;

	Impl(CosObjWrapper co, PDFLDoc *pDoc);
	int GetLength();
	bool GetKeys(NameSet &setKeys);
	CosObjWrapper GetElement(int nIndex);
	bool GetValue(const char *szName, Object::Ptr &pObj);
	bool GetValue(const Name &nmKey, Object::Ptr &pObj);
	const Name &GetName(const char *szName);
	const Name &GetName(ASAtom atmName);
	ASAtom GetAtom(const Name &nmValue);

	static ASBool DictEnum(CosObj inCosObj, CosObj inValue, void *inClientData);

	bool HasKey(const Name &nmKey);


	PDFLDoc *m_pDoc;
	CosObjWrapper m_CosObj;
	A2NMap m_mA2NMap;
	N2AMap m_mN2AMap;
	boost::optional<Dict> m_pDict;
	boost::optional<NameSet > m_psetNames;
};

PDFLObject::Impl::Impl(CosObjWrapper co, PDFLDoc *pDoc)
: m_pDoc(pDoc), m_CosObj(co)
{
}

int
PDFLObject::Impl::GetLength()
{
	if (m_CosObj)  {
		if (CosObjGetType(m_CosObj) == CosArray)  {
			return CosArrayLength(m_CosObj);
		} else  {
			return 1;
		}
	}

	return 0;
}

CosObjWrapper
PDFLObject::Impl::GetElement(int nIndex)
{
	CosObjWrapper coRet;

	if (m_CosObj)  {
		CosType eType = CosObjGetType(m_CosObj);

		int nLength = 1;
		if (eType == CosArray)  {
			nLength = CosArrayLength(m_CosObj);
		}
		if (nIndex < nLength)  {
			switch (eType)  {
				case CosArray:
					coRet = CosArrayGet(m_CosObj, nIndex);
					break;
				default:
					coRet = m_CosObj;
			}
		}
	}

	return coRet;
}

bool
PDFLObject::Impl::HasKey(const Name &nmKey)
{
	if (!m_CosObj)  { return false; }

	if (m_pDict)  {
		Dict::const_iterator itFind = m_pDict->find(nmKey);
		if (itFind != m_pDict->end())  {
			return true;
		}
	}

	CosObjWrapper coValue;
	CosObjWrapper coDict;
	switch (CosObjGetType(m_CosObj))  {
		case CosStream:
			coDict = CosStreamDict(m_CosObj);
			break;
		case CosDict:
			coDict = m_CosObj;
			break;
	}
	if (coDict)  {
		ASAtom atmKey = GetAtom(nmKey);
		if (CosDictKnown(coDict, atmKey))  {
			return true;
		}
	}
	return false;
}

bool
PDFLObject::Impl::GetValue(const Name &nmKey, Object::Ptr &pObj)
{
	if (!m_CosObj)  { return false; }

	if (m_pDict)  {
		Dict::const_iterator itFind = m_pDict->find(nmKey);
		if (itFind != m_pDict->end())  {
			pObj = itFind->second;
			return true;
		}
	}

	CosObjWrapper coValue;
	CosObjWrapper coDict;
	switch (CosObjGetType(m_CosObj))  {
		case CosStream:
			coDict = CosStreamDict(m_CosObj);
			break;
		case CosDict:
			coDict = m_CosObj;
			break;
	}
	if (coDict)  {
		ASAtom atmKey = GetAtom(nmKey);
		if (CosDictKnown(coDict, atmKey))  {
			coValue = CosDictGet(coDict, atmKey);
		}
		if (coValue)  {
			if (!m_pDict)  {
				m_pDict.reset(Dict());
			}
			m_pDoc->CreateObject(coValue, pObj);
			(*m_pDict)[nmKey] = pObj;
			return true;
		}
	}
	pObj.reset();
	return false;
}

bool
PDFLObject::Impl::GetValue(const char *szName, Object::Ptr &pObj)
{
	return GetValue(GetName(szName), pObj);
}

const Name &
PDFLObject::Impl::GetName(ASAtom atmName)
{
	A2NMap::const_iterator itFind = m_mA2NMap.find(atmName);
	if (itFind == m_mA2NMap.end())  {
		Name &nmNewName = m_mA2NMap[atmName];
		nmNewName.Set(ASAtomGetString(atmName));
		m_mN2AMap[nmNewName] = atmName;
		return nmNewName;
	}
	return itFind->second;
}

const Name &
PDFLObject::Impl::GetName(const char *szName)
{
	return GetName(ASAtomFromString(szName));
}

ASAtom
PDFLObject::Impl::GetAtom(const Name &nmValue)
{
	ASAtom atmRet = ASAtomNull;
	if (nmValue.IsValid())  {
		N2AMap::const_iterator itFind = m_mN2AMap.find(nmValue);
		if (itFind == m_mN2AMap.end())  {
			atmRet = ASAtomFromString(nmValue.GetString().c_str());
			m_mA2NMap[atmRet] = nmValue;
			m_mN2AMap[nmValue] = atmRet;
		} else  {
			atmRet = itFind->second;
		}
	}
	return atmRet;
}

bool
PDFLObject::Impl::GetKeys(NameSet &setKeys)
{
	if (!m_CosObj)  { return false; }

	CosType eType = CosObjGetType(m_CosObj);
	if (eType == CosDict || eType == CosStream)  {
		if (!m_psetNames)  {
			m_psetNames.reset(NameSet());
		}
		CosObjWrapper coDict;
		if (eType == CosStream)  {
			coDict = CosStreamDict(m_CosObj);
		} else  {
			coDict =  m_CosObj;
		}
		if (CosObjEnum(coDict, DictEnum, this))  {
			setKeys = *m_psetNames;
			return true;
		}
	}
	return false;
}

ASBool
PDFLObject::Impl::DictEnum(CosObj inCosObj, CosObj , void *inClientData)
{
	Impl *pThis = (Impl *)inClientData;
	if (CosObjGetType(inCosObj) == CosName)  {
		pThis->m_psetNames->insert(pThis->GetName(CosNameValue(inCosObj)));
	}
	return true;
}


PDFLObject::PDFLObject(CosObjWrapper coObject, PDFLDoc *pDoc)
: m_pImpl(new Impl(coObject, pDoc))
{
	if (coObject)  {
		TypeMap::const_iterator itType = g_mTypeMap.find(CosObjGetType(coObject));
		if (itType != g_mTypeMap.end())  {
			m_eType = itType->second;
		}
	}
}

bool
PDFLObject::IsIndirect() const
{
	return m_pImpl->m_CosObj && CosObjIsIndirect(m_pImpl->m_CosObj);
}

Object::ID
PDFLObject::GetID() const
{
	ID nRet = kInvalidID;
	if (IsIndirect())  {
		nRet = CosObjGetID(m_pImpl->m_CosObj);
	}
	return nRet;
}

bool
PDFLObject::Get(bool &bValue, int nIndex /*= 0 */)
{
	CosObjWrapper coValue = m_pImpl->GetElement(nIndex);
	if (coValue && CosObjGetType(coValue) == CosBoolean)  {
		bValue = CosBooleanValue(coValue) != 0;
		return true;
	}
	return false;
}

bool
PDFLObject::Get(int &nValue, int nIndex /*= 0 */)
{
	CosObjWrapper coValue = m_pImpl->GetElement(nIndex);
	if (coValue && CosObjGetType(coValue) == CosInteger)  {
		nValue = CosIntegerValue(coValue);
		return true;
	}
	return false;
}

bool
PDFLObject::Get(unsigned int &nValue, int nIndex /*= 0 */)
{
	CosObjWrapper coValue = m_pImpl->GetElement(nIndex);
	if (coValue && CosObjGetType(coValue) == CosInteger)  {
		int nTemp = CosIntegerValue(coValue);
		if (nTemp >= 0)  {
			nValue = nTemp;
			return true;
		}
	}
	return false;
}

bool
PDFLObject::Get(float &fValue, int nIndex /*= 0 */)
{
	CosObjWrapper coValue = m_pImpl->GetElement(nIndex);
	if (coValue && CosObjGetType(coValue) == CosFixed)  {
		fValue = ASFixedToFloat(CosFixedValue(coValue));
		return true;
	}
	return false;
}

bool
PDFLObject::Get(double &dValue, int nIndex /*= 0 */)
{
	CosObjWrapper coValue = m_pImpl->GetElement(nIndex);
	if (coValue && CosObjGetType(coValue) == CosFixed)  {
		dValue = ASFixedToFloat(CosFixedValue(coValue));
		return true;
	}
	return false;
}

bool
PDFLObject::Get(Name &rValue, int nIndex /*= 0 */)
{
	CosObjWrapper coValue = m_pImpl->GetElement(nIndex);
	if (coValue && CosObjGetType(coValue) == CosName)  {
		rValue = m_pImpl->GetName(CosNameValue(coValue));
		return rValue.IsValid();
	}
	return false;
}

bool
PDFLObject::Get(std::string &sValue, int nIndex /*= 0 */)
{
	CosObjWrapper coValue = m_pImpl->GetElement(nIndex);
	if (coValue && CosObjGetType(coValue) == CosString)  {
		ASInt32 nLength;
		const char *szValue = CosStringValue(coValue, &nLength);
		if (szValue)  {
			sValue.assign(szValue, nLength);
			return true;
		}
	}
	return false;
}

bool
PDFLObject::Get(Object::Ptr &pValue, int nIndex /*= 0 */)
{
	CosObjWrapper coValue = m_pImpl->GetElement(nIndex);
	if (coValue)  {
		m_pImpl->m_pDoc->CreateObject(coValue, pValue);
		return true;
	}
	return false;
}

bool
PDFLObject::Get(Buffer &oValue, int nIndex /*= 0 */)
{
	throw std::logic_error("The method or operation is not implemented.");
}

bool
PDFLObject::Get(Stream &oValue, int nIndex /*= 0 */)
{
	throw std::logic_error("The method or operation is not implemented.");
}

bool
PDFLObject::Get(bool &bValue, const char *szKey)
{
	Object::Ptr pObject;
	if (m_pImpl->GetValue(szKey, pObject))  {
		return pObject->Get(bValue);
	}
	return false;
}

bool
PDFLObject::Get(int &nValue, const char *szKey)
{
	Object::Ptr pObject;
	if (m_pImpl->GetValue(szKey, pObject))  {
		return pObject->Get(nValue);
	}
	return false;
}

bool
PDFLObject::Get(unsigned int &nValue, const char *szKey)
{
	Object::Ptr pObject;
	if (m_pImpl->GetValue(szKey, pObject))  {
		return pObject->Get(nValue);
	}
	return false;
}

bool
PDFLObject::Get(float &fValue, const char *szKey)
{
	Object::Ptr pObject;
	if (m_pImpl->GetValue(szKey, pObject))  {
		return pObject->Get(fValue);
	}
	return false;
}

bool
PDFLObject::Get(double &dValue, const char *szKey)
{
	Object::Ptr pObject;
	if (m_pImpl->GetValue(szKey, pObject))  {
		return pObject->Get(dValue);
	}
	return false;
}

bool
PDFLObject::Get(Name &rValue, const char *szKey)
{
	Object::Ptr pObject;
	if (m_pImpl->GetValue(szKey, pObject))  {
		return pObject->Get(rValue);
	}
	return false;
}

bool
PDFLObject::Get(std::string &sValue, const char *szKey)
{
	Object::Ptr pObject;
	if (m_pImpl->GetValue(szKey, pObject))  {
		return pObject->Get(sValue);
	}
	return false;
}

bool
PDFLObject::Get(Object::Ptr &pValue, const char *szKey)
{
	return m_pImpl->GetValue(szKey, pValue);
}

bool
PDFLObject::Get(Buffer &oValue, const char *szKey)
{
	throw std::logic_error("The method or operation is not implemented.");
}

bool
PDFLObject::Get(Stream &oValue, const char *szKey)
{
	throw std::logic_error("The method or operation is not implemented.");
}

bool
PDFLObject::Get(bool &bValue, const Name &nmKey)
{
	Object::Ptr pObject;
	if (m_pImpl->GetValue(nmKey, pObject))  {
		return pObject->Get(bValue);
	}
	return false;
}

bool
PDFLObject::Get(int &nValue, const Name &nmKey)
{
	Object::Ptr pObject;
	if (m_pImpl->GetValue(nmKey, pObject))  {
		return pObject->Get(nValue);
	}
	return false;
}

bool
PDFLObject::Get(unsigned int &nValue, const Name &nmKey)
{
	Object::Ptr pObject;
	if (m_pImpl->GetValue(nmKey, pObject))  {
		return pObject->Get(nValue);
	}
	return false;
}

bool
PDFLObject::Get(float &fValue, const Name &nmKey)
{
	Object::Ptr pObject;
	if (m_pImpl->GetValue(nmKey, pObject))  {
		return pObject->Get(fValue);
	}
	return false;
}

bool
PDFLObject::Get(double &dValue, const Name &nmKey)
{
	Object::Ptr pObject;
	if (m_pImpl->GetValue(nmKey, pObject))  {
		return pObject->Get(dValue);
	}
	return false;
}

bool
PDFLObject::Get(Name &rValue, const Name &nmKey)
{
	Object::Ptr pObject;
	if (m_pImpl->GetValue(nmKey, pObject))  {
		return pObject->Get(rValue);
	}
	return false;
}

bool
PDFLObject::Get(std::string &sValue, const Name &nmKey)
{
	Object::Ptr pObject;
	if (m_pImpl->GetValue(nmKey, pObject))  {
		return pObject->Get(sValue);
	}
	return false;
}

bool
PDFLObject::Get(Object::Ptr &pValue, const Name &nmKey)
{
	return m_pImpl->GetValue(nmKey, pValue);
}

bool
PDFLObject::Get(Buffer &oValue, const Name &nmKey)
{
	throw std::logic_error("The method or operation is not implemented.");
}

bool
PDFLObject::Get(Stream &oValue, const Name &nmKey)
{
	throw std::logic_error("The method or operation is not implemented.");
}

int
PDFLObject::GetLength() const
{
	return m_pImpl->GetLength();
}

bool
PDFLObject::GetKeys(NameSet &setKeys)
{
	return m_pImpl->GetKeys(setKeys);
}

Document *
PDFLObject::GetDoc() const
{
	return m_pImpl->m_pDoc;
}

bool
PDFLObject::HasKey(const Name &nmKey)
{
	return m_pImpl->HasKey(nmKey);
}

bool
PDFLObject::HasKey(const char *szKey)
{
	return HasKey(m_pImpl->GetName(szKey));
}


struct PDFLDoc::MyImpl  {
	typedef std::map<Object::ID, boost::shared_ptr<PDFLObject> > ObjectMap;

	MyImpl(PDFLDoc *pOwner, const std::string &sFileName);

	bool Open(const std::string &sFileName);

	bool GetObject(Object::ID nID, Object::Ptr &pObject);

	PDFLDoc *m_pOwner;
	PDDoc m_pdDoc;
	CosDoc m_cdDoc;
	ObjectMap m_mObjects;
};

struct ASPathName_deleter  {
	void operator()(ASPathName p)  { ASFileSysReleasePath(NULL, p); }
};

PDFLDoc::MyImpl::MyImpl(PDFLDoc *pOwner, const std::string &sFileName)
: m_pOwner(pOwner), m_pdDoc(NULL), m_cdDoc(NULL)
{
	if (!sFileName.empty())  {
		Open(sFileName);
	}
}

bool
PDFLDoc::MyImpl::Open(const std::string &sFileName)
{
	bool bSuccess = false;

	if (!sFileName.empty())  {
		if (!PDFLInitter::Get()->IsValid())  {
			return false;
		}

		DURING
			boost::shared_ptr<void> aspFile(
				ASPathFromPlatformPath((void *)sFileName.c_str()),
				ASPathName_deleter()
				);

			m_pdDoc = PDDocOpen((ASPathName)aspFile.get(), NULL, NULL, false);
			if (m_pdDoc)  {
				m_cdDoc = PDDocGetCosDoc(m_pdDoc);
				bSuccess = true;
			}
		HANDLER
		END_HANDLER
	}
	return bSuccess;
}

bool
PDFLDoc::MyImpl::GetObject(Object::ID nID, Object::Ptr &pObject)
{
	if (nID != Object::kInvalidID)  {
		MyImpl::ObjectMap::const_iterator itFind = m_mObjects.find(nID);
		if (itFind != m_mObjects.end())  {
			pObject = itFind->second;
			return true;
		}
		CosObjWrapper coFind = CosDocGetObjByID(m_cdDoc, nID);
		if (coFind)  {
			m_mObjects[nID].reset(new PDFLObject(coFind, m_pOwner));
			pObject = m_mObjects[nID];
			return true;
		}
	}
	return false;
}


PDFLDoc::PDFLDoc(const std::string &sFileName)
: Document(sFileName)
{
	m_pMyImpl.reset(new MyImpl(this, sFileName));
}

bool
PDFLDoc::IsValid() const
{
	return m_pMyImpl->m_cdDoc != NULL;
}

bool
PDFLDoc::GetCatalog(Object::Ptr &pCatalog) const
{
	CosObjWrapper coRoot = CosDocGetRoot(m_pMyImpl->m_cdDoc);
	if (coRoot)  {
		CreateObject(coRoot, pCatalog);
		return true;
	}
	return false;
}

PDFVersion
PDFLDoc::GetVersion() const
{
	ASInt16 nMajor = 0, nMinor = 0;
	PDDocGetVersion(m_pMyImpl->m_pdDoc, &nMajor, &nMinor);
	return PDFVersion(nMajor, nMinor);
}

bool
PDFLDoc::GetObject(Object::ID nID, Object::Ptr &pObject) const
{
	return m_pMyImpl->GetObject(nID, pObject);
}

void
PDFLDoc::CreateObject(CosObjWrapper coObject, Object::Ptr &pObject) const
{
	if (CosObjIsIndirect(coObject))  {
		GetObject(CosObjGetID(coObject), pObject);
	} else  {
		pObject.reset(new PDFLObject(coObject, const_cast<PDFLDoc *>(this)));
	}
}

Document *
PDFLDoc::clone() const
{
	return new PDFLDoc("");
}

bool
PDFLDoc::OpenFile(const std::string &sFileName)
{
	return m_pMyImpl->Open(sFileName);
}

Document::Ptr
PDFLDoc::ClonePtr() const
{
	Document::Ptr pRet(clone());
	return pRet;
}

}
