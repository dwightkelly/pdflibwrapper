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

#include "PDFInit.h"
#include "CosCalls.h"
#include "PDCalls.h"

namespace  {

	typedef boost::shared_mutex Mutex;
	typedef boost::upgrade_lock<Mutex> ROLock;
	typedef boost::upgrade_to_unique_lock<Mutex> RWLock;

	Mutex g_mtxPDFLInit;
	bool g_bInitted = false;

	bool
	InitPDFL()
	{
		ASInt32 nRet = 0;
		ROLock lck(g_mtxPDFLInit);
		if (!g_bInitted)  {
			RWLock lckWrite(lck);
			PDFLDataRec thePDFLDataRec;
			DURING
				memset(&thePDFLDataRec, 0, sizeof(PDFLDataRec));
				thePDFLDataRec.size = sizeof(PDFLDataRec);
				nRet = ::PDFLInit(&thePDFLDataRec);
			HANDLER
				nRet = ERRORCODE;
			END_HANDLER
			g_bInitted = nRet == 0;
		}

		return nRet == 0;
	}

	using namespace PDFLibWrapper;

	typedef std::map<CosType, Object::Type> TypeMap;

	// work-around for compiling on OS X
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

	Impl(CosObj co, PDFLDoc *pDoc);
	int GetLength();
	bool GetKeys(NameSet &setKeys);
	CosObj GetElement(int nIndex);
	bool GetValue(const char *szName, Object::Ptr &pObj);
	bool GetValue(const Name &nmKey, Object::Ptr &pObj);
	const Name &GetName(const char *szName);
	const Name &GetName(ASAtom atmName);
	ASAtom GetAtom(const Name &nmValue);

	static ASBool DictEnum(CosObj inCosObj, CosObj inValue, void *inClientData);

	PDFLDoc *m_pDoc;
	CosObj m_CosObj;
	A2NMap m_mA2NMap;
	N2AMap m_mN2AMap;
	boost::optional<Dict> m_pDict;
	boost::optional<NameSet > m_psetNames;
};

PDFLObject::Impl::Impl(CosObj co, PDFLDoc *pDoc)
: m_pDoc(pDoc), m_CosObj(co)
{
}

int
PDFLObject::Impl::GetLength()
{
	if (CosObjGetType(m_CosObj) == CosArray)  {
		return CosArrayLength(m_CosObj);
	} else  {
		return 1;
	}
}

CosObj
PDFLObject::Impl::GetElement(int nIndex)
{
	if (!m_CosObj)  { return NULL; }

	CosObj coRet = NULL;
	CosType eType = CosObjGetType(m_CosObj);
	int nLength = 1;
	if (eType == CosArray)  {
		nLength = CosArrayLength(m_CosObj);
	}
	if (nIndex >= nLength)  {
		return NULL;
	}
	switch (eType)  {
		case CosArray:
			coRet = CosArrayGet(m_CosObj, nIndex);
			break;
		default:
			coRet = m_CosObj;
	}

	return coRet;
}


bool
PDFLObject::Impl::GetValue(const Name &nmKey, Object::Ptr &pObj)
{
	if (m_pDict)  {
		Dict::const_iterator itFind = m_pDict->find(nmKey);
		if (itFind != m_pDict->end())  {
			pObj = itFind->second;
			return true;
		}
	}

	CosObj coValue = NULL;
	CosObj coDict = NULL;
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
	CosType eType = CosObjGetType(m_CosObj);
	if (eType == CosDict || eType == CosStream)  {
		if (!m_psetNames)  {
			m_psetNames.reset(NameSet());
		}
		CosObj coDict = (eType == CosStream ? CosStreamDict(m_CosObj) : m_CosObj);
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


PDFLObject::PDFLObject(CosObj coObject, PDFLDoc *pDoc)
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
	CosObj coValue = m_pImpl->GetElement(nIndex);
	if (CosObjGetType(coValue) == CosBoolean)  {
		bValue = CosBooleanValue(coValue) != 0;
		return true;
	}
	return false;
}

bool
PDFLObject::Get(int &nValue, int nIndex /*= 0 */)
{
	CosObj coValue = m_pImpl->GetElement(nIndex);
	if (CosObjGetType(coValue) == CosInteger)  {
		nValue = CosIntegerValue(coValue);
		return true;
	}
	return false;
}

bool
PDFLObject::Get(unsigned int &nValue, int nIndex /*= 0 */)
{
	CosObj coValue = m_pImpl->GetElement(nIndex);
	if (CosObjGetType(coValue) == CosInteger)  {
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
	CosObj coValue = m_pImpl->GetElement(nIndex);
	if (CosObjGetType(coValue) == CosFixed)  {
		fValue = ASFixedToFloat(CosFixedValue(coValue));
		return true;
	}
	return false;
}

bool
PDFLObject::Get(double &dValue, int nIndex /*= 0 */)
{
	CosObj coValue = m_pImpl->GetElement(nIndex);
	if (CosObjGetType(coValue) == CosFixed)  {
		dValue = ASFixedToFloat(CosFixedValue(coValue));
		return true;
	}
	return false;
}

bool
PDFLObject::Get(Name &rValue, int nIndex /*= 0 */)
{
	CosObj coValue = m_pImpl->GetElement(nIndex);
	if (CosObjGetType(coValue) == CosName)  {
		rValue = m_pImpl->GetName(CosNameValue(coValue));
		return rValue.IsValid();
	}
	return false;
}

bool
PDFLObject::Get(std::string &sValue, int nIndex /*= 0 */)
{
	CosObj coValue = m_pImpl->GetElement(nIndex);
	if (CosObjGetType(coValue) == CosString)  {
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
	CosObj coValue = m_pImpl->GetElement(nIndex);
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
	throw std::logic_error("The method or operation is not implemented.");
}

bool
PDFLObject::Get(unsigned int &nValue, const char *szKey)
{
	throw std::logic_error("The method or operation is not implemented.");
}

bool
PDFLObject::Get(float &fValue, const char *szKey)
{
	throw std::logic_error("The method or operation is not implemented.");
}

bool
PDFLObject::Get(double &dValue, const char *szKey)
{
	throw std::logic_error("The method or operation is not implemented.");
}

bool
PDFLObject::Get(Name &rValue, const char *szKey)
{
	throw std::logic_error("The method or operation is not implemented.");
}

bool
PDFLObject::Get(std::string &sValue, const char *szKey)
{
	throw std::logic_error("The method or operation is not implemented.");
}

bool
PDFLObject::Get(Object::Ptr &pValue, const char *szKey)
{
	throw std::logic_error("The method or operation is not implemented.");
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
	throw std::logic_error("The method or operation is not implemented.");
}

bool
PDFLObject::Get(int &nValue, const Name &nmKey)
{
	throw std::logic_error("The method or operation is not implemented.");
}

bool
PDFLObject::Get(unsigned int &nValue, const Name &nmKey)
{
	throw std::logic_error("The method or operation is not implemented.");
}

bool
PDFLObject::Get(float &fValue, const Name &nmKey)
{
	throw std::logic_error("The method or operation is not implemented.");
}

bool
PDFLObject::Get(double &dValue, const Name &nmKey)
{
	throw std::logic_error("The method or operation is not implemented.");
}

bool
PDFLObject::Get(Name &rValue, const Name &nmKey)
{
	throw std::logic_error("The method or operation is not implemented.");
}

bool
PDFLObject::Get(std::string &sValue, const Name &nmKey)
{
	throw std::logic_error("The method or operation is not implemented.");
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
	Open(sFileName);
}

bool
PDFLDoc::MyImpl::Open(const std::string &sFileName)
{
	if (!sFileName.empty())  {
		InitPDFL();

		DURING
			boost::shared_ptr<void> aspFile(
				ASPathFromPlatformPath((void *)sFileName.c_str()),
				ASPathName_deleter()
				);

			m_pdDoc = PDDocOpen(aspFile.get(), NULL, NULL, false);
			if (m_pdDoc)  {
				m_cdDoc = PDDocGetCosDoc(m_pdDoc);
				return true;
			}
		HANDLER
		END_HANDLER
	}
	return false;
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
		CosObj coFind = CosDocGetObjByID(m_cdDoc, nID);
		if (coFind)  {
			m_mObjects[nID].reset(new PDFLObject(coFind, m_pOwner));
			pObject = m_mObjects[nID];
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
	CosObj coRoot = CosDocGetRoot(m_pMyImpl->m_cdDoc);
	if (coRoot)  {
		CreateObject(coRoot, pCatalog);
		return true;
	}
	return false;
}

PDFVersion
PDFLDoc::GetVersion() const
{
	ASInt16 nMajor, nMinor;
	PDDocGetVersion(m_pMyImpl->m_pdDoc, &nMajor, &nMinor);
	return PDFVersion(nMajor, nMinor);
}

bool
PDFLDoc::GetObject(Object::ID nID, Object::Ptr &pObject) const
{
	return m_pMyImpl->GetObject(nID, pObject);
}

void
PDFLDoc::CreateObject(CosObj coObject, Object::Ptr &pObject) const
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

}
