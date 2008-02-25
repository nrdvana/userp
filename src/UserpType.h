#ifndef USERP_TYPE_H
#define USERP_TYPE_H

#include "Misc.h"

namespace Userp {

class TType;
class TEnumType;
class TUnionType;
class TArrayType;
class TRecordType;

class TReader;
class TWriter;

class TTypeSpace {
	class Implementation;
	Implementation* impl;
public:
	TTypeSpace();
	~TTypeSpace();

	TType* getType(TSymbol *Name);

	TType* DeclareType(TSymbol *Name);
	TType* DeclareType(const char *Name);
	TType* DeclareType(const std::string &Name);

	void DefineEnum(TType *Which, int ElemCount);
	void DefineUnion(TType *Which, TArray<TType*> Members);
	void DefineArray(TType *Which, TType *ElemType, int count= -1);
	void DefineRecord(TType *Which, TArray<TSymbol*>, TArray<TType*>);

	void Prepare(TType *Which);
};

class TType {
public:
	enum TActualType { atUndefined, atEnum, atUnion, atArray, atRecord };
private:
	class Implementation;
	TTypeSpace *owner;
	TSymbol *name;
	Implementation *impl;
	TActualType actualType;
	void CastFailed(TActualType attempted) const;
protected:
	TType();
	~TType(); // Not virtual.  See implementation comments.
public:
	TSymbol* Name() const { return name; }

	TWriter* getAttributeWriter();
	TWriter* getCommentWriter();

	inline void Prepare() { owner->Prepare(this); }

	inline TReader* newAttributeReader();
	TReader* newCommentReader();

	inline TActualType getSubtype() { return actualType; }
	inline bool isEnum() const { return actualType == atEnum; }
	inline bool isUnion() const { return actualType == atUnion; }
	inline bool isArray() const { return actualType == atArray; }
	inline bool isRecord() const { return actualType == atRecord; }
	inline TEnumType* asEnum() { if (!isEnum()) CastFailed(atEnum); return (TEnumType*)this; }
	inline const TEnumType* asEnum() const { if (!isEnum()) CastFailed(atEnum); return (const TEnumType*)this; }
	inline TUnionType* asUnion() { if (!isUnion()) CastFailed(atUnion); return (TUnionType*)this; }
	inline const TUnionType* asUnion() const { if (!isUnion()) CastFailed(atUnion); return (const TUnionType*)this; }
	inline TArrayType* asArray() { if (!isArray()) CastFailed(atArray); return (TArrayType*)this; }
	inline const TArrayType* asArray() const { if (!isArray()) CastFailed(atArray); return (const TArrayType*)this; }
	inline TRecordType* asRecord() { if (!isRecord()) CastFailed(atRecord); return (TRecordType*)this; }
	inline const TRecordType* asRecord() const { if (!isRecord()) CastFailed(atRecord); return (const TRecordType*)this; }

	friend class TTypeSpace;
};

class TEnumType: public TType {
protected:
	~TEnumType() {}
public:
	TWriter* getValueWriter();
	TReader* getValueReader();
	int ValueCount();

	friend class TTypeSpace;
};

class TUnionType: public TType {
protected:
	~TUnionType() {}
public:
	TArray<const TType*> Members();

	friend class TTypeSpace;
};

class TArrayType: public TType {
protected:
	~TArrayType() {}
public:
	TType* ElemType();
	int LengthSpec();
	inline bool isVariableLen() { return LengthSpec() == -1; }

	friend class TTypeSpace;
};

class TRecordType: public TType {
protected:
	~TRecordType() {}
public:
	struct TField { TSymbol* Name; TType* Type; };

	TArray<const TSymbol*> FieldNames();
	TArray<const TType*> FieldTypes();
	TField getField(int idx);

	friend class TTypeSpace;
};

} // namespace

#endif
