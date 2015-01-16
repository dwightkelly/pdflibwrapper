/*
 *  PDFLWrapper.h
 *
 *  Copyright (c) 2014-2015, Apago, Inc. All rights reserved.
 *
 */

#ifndef APAGO_SPDFWrapper_h__
#define APAGO_SPDFWrapper_h__

#include "PDFLibWrapper.h"

#include "Environ.h"
#include "PICommon.h"
#include "CorCalls.h"
#include "CosCalls.h"


namespace PDFLibWrapper  {

	class CosObjWrapper  {
	public:
		CosObjWrapper(CosObj co) : m_bInitialized(true), m_coObject(co)  { }
		CosObjWrapper() : m_bInitialized(false)  { }

		CosObjWrapper &operator=(CosObj co)  {
			m_bInitialized = true;
			m_coObject = co;
			return *this;
		}

		operator void*() const  { return (void *)m_bInitialized; }
		operator bool() const  { return m_bInitialized; }
		bool operator!() const  { return !m_bInitialized; }

		operator CosObj() const  { assert(m_bInitialized); return m_coObject; }

	private:
		bool m_bInitialized;
		CosObj m_coObject;
	};

	class PDFLDoc;

	class PDFLObject : public Object  {
	public:
		virtual Document *GetDoc() const;

		virtual bool IsIndirect() const;
		virtual ID GetID() const;

		virtual int GetLength() const;

		virtual bool GetKeys(NameSet &setKeys);
		virtual bool HasKey(const Name &nmKey);
		virtual bool HasKey(const char *szKey);

		using Object::Get;
		virtual bool Get(bool &bValue, int nIndex = 0);
		virtual bool Get(int &nValue, int nIndex = 0);
		virtual bool Get(unsigned int &nValue, int nIndex = 0);
		virtual bool Get(float &fValue, int nIndex = 0);
		virtual bool Get(double &dValue, int nIndex = 0);
		virtual bool Get(Name &rValue, int nIndex = 0);
		virtual bool Get(std::string &sValue, int nIndex = 0);
		virtual bool Get(Object::Ptr &pValue, int nIndex = 0);
		virtual bool Get(Buffer &oValue, int nIndex = 0);
		virtual bool Get(Stream &oValue, int nIndex = 0);
		virtual bool Get(bool &bValue, const char *szKey);
		virtual bool Get(int &nValue, const char *szKey);
		virtual bool Get(unsigned int &nValue, const char *szKey);
		virtual bool Get(float &fValue, const char *szKey);
		virtual bool Get(double &dValue, const char *szKey);
		virtual bool Get(Name &rValue, const char *szKey);
		virtual bool Get(std::string &sValue, const char *szKey);
		virtual bool Get(Object::Ptr &pValue, const char *szKey);
		virtual bool Get(Buffer &oValue, const char *szKey);
		virtual bool Get(Stream &oValue, const char *szKey);
		virtual bool Get(bool &bValue, const Name &nmKey);
		virtual bool Get(int &nValue, const Name &nmKey);
		virtual bool Get(unsigned int &nValue, const Name &nmKey);
		virtual bool Get(float &fValue, const Name &nmKey);
		virtual bool Get(double &dValue, const Name &nmKey);
		virtual bool Get(Name &rValue, const Name &nmKey);
		virtual bool Get(std::string &sValue, const Name &nmKey);
		virtual bool Get(Object::Ptr &pValue, const Name &nmKey);
		virtual bool Get(Buffer &oValue, const Name &nmKey);
		virtual bool Get(Stream &oValue, const Name &nmKey);

		struct Impl;
	private:
		PDFLObject(CosObjWrapper coObject, PDFLDoc *pDoc);

		boost::shared_ptr<Impl> m_pImpl;

		friend class PDFLDoc;
	};

	class PDFLDoc : public Document  {
	public:
		PDFLDoc(const std::string &sFileName);
		virtual ~PDFLDoc()  { }

		virtual bool IsValid() const;

		virtual PDFVersion GetVersion() const;
		virtual bool GetCatalog(Object::Ptr &pCatalog) const;

		bool GetObject(Object::ID nID, Object::Ptr &pObject) const;
		void CreateObject(CosObjWrapper coObject, Object::Ptr &pObject) const;

		struct MyImpl;

	protected:
		virtual Document *clone() const;
		virtual Document::Ptr ClonePtr() const;
		virtual bool OpenFile(const std::string &sFileName);


	private:
		boost::shared_ptr<MyImpl> m_pMyImpl;
	};


}

#endif // APAGO_SPDFWrapper_h__
