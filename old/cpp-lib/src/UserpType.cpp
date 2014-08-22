#include "UserpType.h"
#include "DataSource.h"
#include "Misc.h"

#include <map>
#include <list>
#include <vector>
#include <set>

using namespace std;
using std::list;
using std::map;

namespace Userp {

class TTypeSpace::Implementation {
public:
	TSymbolTable symbols;
	list<TType*> types;
	map<TSymbol&, TType*> typeLookup;
};

TTypeSpace::TTypeSpace() {
	impl= new Implementation();
}

TTypeSpace::~TTypeSpace() {
	delete impl;
}

TType* TypeSpace::getType(TSymbol *Name) {
	map<TSymbol&, TType*>::iterator found= impl->typeLookup.find(*Name);
	if (found == typeLookup.end())
		return NULL;
	else
		return found->second;
}

TType* TTypeSpace::DeclareType(TSymbol *Name) {
	// map to a local symbol
	if (!impl->symbols->Owns(Name))
		Name= impl->symbols->GetSymbol(Name->Data(), true);
	TType *result= new TType();
	result->impl= NULL;
	result->owner= this;
	result->actualType= atUndefined;
	result->name= Name;
	impl->types.push_back(result);
	return result;
}

TType* TTypeSpace::DeclareType(const char *Name) {
	return DeclareType(impl->symbols->GetSymbol(Name));
}

TType* TTypeSpace::DeclareType(const std::string &Name) {
	return DeclareType(impl->symbols->GetSymbol(Name));
}

void CheckDefinePrereqs(TTypeSpace *ths, TType *Which) {
	if (Which->owner != ths)
		throw "Cannot alter types in other spaces";
	if (Which->actualType != atUndefined)
		throw "Type has already been defined";
}

void TTypeSpace::DefineEnum(TType *Which, int ElemCount) {
	CheckDefinePrereqs(this, Which);
	Which->actualType= atEnum;
	Which->impl= new EnumImplementation(ElemCount);
}

void TTypeSpace::DefineUnion(TType *Which, TArray<TType*> Members) {
	CheckDefinePrereqs(this, Which);
	Which->actualType= atUnion;
	Which->impl= new UnionImplementation(Members);
}

void TTypeSpace::DefineArray(TType *Which, TType *ElemType, int count= -1) {
	CheckDefinePrereqs(this, Which);
	Which->actualType= atArray;
	Which->impl= new ArrayImplementation(Members);
}

void TTypeSpace::DefineRecord(TType *Which, TArray<TSymbol*> fieldNames, TArray<TType*> fieldTypes) {
	CheckDefinePrereqs(this, Which);
	Which->actualType= atRecord;
	Which->impl= new RecordImplementation(Members);
}

} // namespace
