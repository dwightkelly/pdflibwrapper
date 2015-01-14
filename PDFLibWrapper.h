/*
 *  PDFLibWrapper.h
 *
 *  Copyright (c) 2014-2015, Apago, Inc. All rights reserved.
 *
 */

#ifndef APAGO_PDFLibWrapper_h__
#define APAGO_PDFLibWrapper_h__

#include <istream>
#include <set>
#include <vector>
#include <string>

#include <boost/integer_traits.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/shared_array.hpp>
#include <boost/variant.hpp>

namespace PDFLibWrapper  {

	class PDFVersion  {
	public:
		PDFVersion(unsigned char nMajor = 0, unsigned char nMinor = 0)
			: m_nMajor(nMajor), m_nMinor(nMinor)
		{ }

		bool IsSet() const  { return m_nMajor > 0 && m_nMinor > 0; }

		bool operator<(const PDFVersion &rOther) const  {
			return m_nMajor < rOther.m_nMajor ||
				(m_nMajor == rOther.m_nMajor && m_nMinor < rOther.m_nMinor);
		}
		bool operator<=(const PDFVersion &rOther) const  {
			return m_nMajor < rOther.m_nMajor ||
				(m_nMajor == rOther.m_nMajor && m_nMinor <= rOther.m_nMinor);
		}
		bool operator>(const PDFVersion &rOther) const  { return rOther < *this;  }
		bool operator>=(const PDFVersion &rOther) const  { return rOther <= *this;  }
		bool operator==(const PDFVersion &rOther) const  {
			return m_nMajor == rOther.m_nMajor && m_nMinor == rOther.m_nMinor;
		}
		bool operator!=(const PDFVersion &rOther) const  { return !(*this == rOther); }

		unsigned short m_nMajor;
		unsigned short m_nMinor;
	};

	class Name  {
	public:
		Name(const char *szName = NULL);
		Name(const std::string &sName);

		void Set(const char *szName);
		void Set(const std::string &sName);

		bool IsValid() const  { return m_pString != NULL; }
		const std::string &GetString() const  { return *m_pString; }

		bool operator==(const Name &nmOther) const
		{ return m_nToken == nmOther.m_nToken; }
		bool operator!=(const Name &nmOther) const
		{ return m_nToken != nmOther.m_nToken; }
		bool operator<(const Name &nmOther) const
		{ return m_nToken < nmOther.m_nToken; }

		friend std::istream &operator>>(std::istream &is, Name &nmDest)  {
			std::string sTemp;
			sTemp.assign((std::istreambuf_iterator<char>(is)),
				std::istreambuf_iterator<char>());
			nmDest.Set(sTemp);
			return is;
		}
		friend std::ostream &operator<<(std::ostream &os, const Name &nmSrc)  {
			if (nmSrc.IsValid())  {
				os << nmSrc.GetString();
			} else  {
				os << "{{invalid name}}";
			}
			return os;
		}

	private:
		unsigned int m_nToken;
		std::string *m_pString;
	};
	typedef std::set<Name> NameSet;

	class Object
	{
	public:
		enum Type { kUnknown, kNull, kBoolean, kInteger, kFixed, kName, kString,
			kStream, kArray, kDict };
		typedef boost::shared_ptr<Object> Ptr;
		typedef boost::shared_array<const unsigned char> Buffer;
		typedef boost::shared_ptr<std::istream> Stream;
		typedef long ID;
		typedef boost::variant<long, Name> Selector;
		struct PathElement  {
// 			PathElement(const Selector &oSelector = Name(), 
// 				ID nID = Object::kInvalidID)
// 			: m_pObject(NULL), m_oWayToThis(oSelector), m_nObjectID(nID)
// 			{ }
			PathElement(const Selector &oSelector = Name(),
				Object *pObject = NULL, ID nID = Object::kInvalidID)
				: m_pObject(pObject), m_oWayToThis(oSelector),
				m_nObjectID(pObject ? pObject->GetID() : nID)
			{
			}
			const Name *GetName() const  { return boost::get<Name>(&m_oWayToThis); }
			const long *GetIndex() const  { return boost::get<long>(&m_oWayToThis); }

			Object *m_pObject; // TEMPORARY
			Selector m_oWayToThis;
			ID m_nObjectID;
		};
		typedef std::vector<PathElement> Path;

		enum  { kInvalidID = boost::integer_traits<ID>::const_max };

		Type GetType() const  { return m_eType; }
		Type GetTypeName(std::string &sName) const;
		static Type GetType(const std::string &sType);

		virtual bool IsIndirect() const = 0;
		virtual ID GetID() const = 0;

		virtual int GetLength() const = 0;

		virtual bool GetKeys(NameSet &setKeys) = 0;

		virtual bool Get(bool &bValue, int nIndex = 0) = 0;
		virtual bool Get(int &nValue, int nIndex = 0) = 0;
		virtual bool Get(unsigned int &nValue, int nIndex = 0) = 0;
		virtual bool Get(float &fValue, int nIndex = 0) = 0;
		virtual bool Get(double &dValue, int nIndex = 0) = 0;
		virtual bool Get(Name &rValue, int nIndex = 0) = 0;
		virtual bool Get(std::string &sValue, int nIndex = 0) = 0;
		virtual bool Get(Object::Ptr &pValue, int nIndex = 0) = 0;
		virtual bool Get(Buffer &oValue, int nIndex = 0) = 0;
		virtual bool Get(Stream &oValue, int nIndex = 0) = 0;

		virtual bool Get(bool &bValue, const char *szKey) = 0;
		virtual bool Get(int &nValue, const char *szKey) = 0;
		virtual bool Get(unsigned int &nValue, const char *szKey) = 0;
		virtual bool Get(float &fValue, const char *szKey) = 0;
		virtual bool Get(double &dValue, const char *szKey) = 0;
		virtual bool Get(Name &rValue, const char *szKey) = 0;
		virtual bool Get(std::string &sValue, const char *szKey) = 0;
		virtual bool Get(Object::Ptr &pValue, const char *szKey) = 0;
		virtual bool Get(Buffer &oValue, const char *szKey) = 0;
		virtual bool Get(Stream &oValue, const char *szKey) = 0;

		virtual bool Get(bool &bValue, const std::string &sKey)  { return Get(bValue, sKey.c_str()); }
		virtual bool Get(int &nValue, const std::string &sKey)  { return Get(nValue, sKey.c_str()); }
		virtual bool Get(unsigned int &nValue, const std::string &sKey)  { return Get(nValue, sKey.c_str()); }
		virtual bool Get(float &fValue, const std::string &sKey)  { return Get(fValue, sKey.c_str()); }
		virtual bool Get(double &dValue, const std::string &sKey)  { return Get(dValue, sKey.c_str()); }
		virtual bool Get(Name &rValue, const std::string &sKey)  { return Get(rValue, sKey.c_str()); }
		virtual bool Get(std::string &sValue, const std::string &sKey)  { return Get(sValue, sKey.c_str()); }
		virtual bool Get(Object::Ptr &pValue, const std::string &sKey)  { return Get(pValue, sKey.c_str()); }
		virtual bool Get(Buffer &oValue, const std::string &sKey)  { return Get(oValue, sKey.c_str()); }
		virtual bool Get(Stream &oValue, const std::string &sKey)  { return Get(oValue, sKey.c_str()); }

		virtual bool Get(bool &bValue, const Name &nmKey) = 0;
		virtual bool Get(int &nValue, const Name &nmKey) = 0;
		virtual bool Get(unsigned int &nValue, const Name &nmKey) = 0;
		virtual bool Get(float &fValue, const Name &nmKey) = 0;
		virtual bool Get(double &dValue, const Name &nmKey) = 0;
		virtual bool Get(Name &rValue, const Name &nmKey) = 0;
		virtual bool Get(std::string &sValue, const Name &nmKey) = 0;
		virtual bool Get(Object::Ptr &pValue, const Name &nmKey) = 0;
		virtual bool Get(Buffer &oValue, const Name &nmKey) = 0;
		virtual bool Get(Stream &oValue, const Name &nmKey) = 0;

		std::string GetString();

		static void GetObjectDescription(std::string &sDesc, Path &vPath,
			bool bNeedParent, bool bFullPath = false);

	protected:
		Object() : m_eType(kUnknown)  { }

		Type m_eType;
		//ID m_nID;
	};

	class Document
	{
	public:
		typedef boost::shared_ptr<Document> Ptr;

		static Ptr Open(const std::string &sFileName);

		virtual bool IsValid() const = 0;

		virtual PDFVersion GetVersion() const = 0;
		virtual bool GetTrailer(Object::Ptr &pTrailer) const;
		virtual bool GetCatalog(Object::Ptr &pCatalog) const = 0;

		static bool Register(const std::string &sName, const Ptr &pDoc);
		static bool AutoRegister();

		struct Impl;

	protected:
		Document(const std::string &sFileName);
		Document(const Document &rOther);
		Document &operator=(const Document &rOther);

		virtual Document *clone() const = 0;

		virtual bool OpenFile(const std::string &sFileName) = 0;

		boost::shared_ptr<Impl> m_pImpl;
	};

}

#endif // APAGO_PDFLibWrapper_h__
