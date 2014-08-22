#ifndef USERP_SYMBOL_H
#define USERP_SYMBOL_H

#include <stdint.h>
#include <assert.h>

#ifdef USERP_CXX_IFACE
#include <string>
namespace Userp {
#endif

#ifdef USERP_CXX_IFACE
template <class ElemT>
struct TArray {
	ElemT* Start;
	unsigned Len;

	TArray(): Start(0), Len(0) {}
	TArray(ElemT *Start, unsigned Len): Start(Start), Len(Len) {}
	TArray(ElemT *Start, ElemT *Limit): Start((ElemT*)Start), Len(((ElemT*)Limit)-((ElemT*)Start)) {}

	bool operator == (const TArray<ElemT> &Peer) const {
		if (Len != Peer.Len) return false;
		for (int i=0; i<Len; i++)
			if (Start[i] != Peer.Start[i])
				return false;
		return true;
	}

	bool operator < (const TArray<ElemT> &Peer) const {
		for (int i=0, limit= Len<Peer.Len? Len : Peer.Len; i<limit; i++)
			if (Start[i] < Peer.Start[i])
				return true;
		if (Len < Peer.Len)
			return true;
		return false;
	}
};

typedef TArray<const uint8_t> TConstBytes;
typedef TConstBytes const_byte_array_t;
#else
typedef struct const_byte_array {
	const uint8_t* Start;
	int Len;
} const_byte_array_t;
#endif

struct userp_symbol_table;
typedef struct userp_symbol_table userp_symbol_table_t;

struct userp_symbol;
typedef struct userp_symbol userp_symbol_t;

USERP_EXTERN int userp_symbol_compare(const userp_symbol_t *a, const userp_symbol_t *b);
USERP_EXTERN const char* userp_symbol_asText(const userp_symbol_t*);
USERP_EXTERN const_byte_array_t userp_symbol_getBytes(const userp_symbol_t*);
USERP_EXTERN userp_symbol_table_t* userp_symbol_getOwner(const userp_symbol_t*);
USERP_EXTERN void userp_symbol_GCMark(userp_symbol_t*);

USERP_EXTERN userp_symbol_table_t* userp_symbol_table_create();
USERP_EXTERN void userp_symbol_table_destroy(userp_symbol_table_t*);
USERP_EXTERN userp_symbol_t* userp_symbol_table_SymbolFromBytes(const_byte_array_t Bytes, bool Create);
USERP_EXTERN userp_symbol_t* userp_symbol_table_SymbolFromStr(const char *Str, bool Create);
USERP_EXTERN void userp_symbol_table_GSSweep(userp_symbol_table*);

#ifdef USERP_CXX_IFACE
class TSymbol {
	userp_symbol_t* symbolObj;
public:
	inline TConstBytes Bytes() const { return userp_symbol_getBytes(symbolObj); }
	inline const char* asText() const { return userp_symbol_asText(symbolObj); }

	inline bool operator == (const TSymbol &Peer) const { return symbolObj == Peer.symbolObj; }
	inline bool operator < (const TSymbol &Peer) const { return userp_symbol_compare(symbolObj, Peer.symbolObj) < 0; }

	inline void GCMark() { userp_symbol_GCMark(symbolObj); }
};

class TSymbolTable {
	userp_symbol_table_t *symtbl;
	TSymbolTable(const TSymbolTable &); // deny copying
	TSymbolTable& operator = (const TSymbolTable& Peer); // deny copying
public:
	TSymbolTable() {
		symtbl= userp_symbol_table_create();
	}
	~TSymbolTable() {
		assert(symtbl != NULL);
		userp_symbol_table_destroy(symtbl);
		symtbl= NULL;
	}
	// would rather extend the symboltable struct, but its definition is hidden
	operator userp_symbol_table_t * () { return symtbl; }

	inline TSymbol GetSymbol(TConstBytes Data, bool AutoCreate= true);
	inline TSymbol GetSymbol(const char* Text, bool AutoCreate= true);
	inline TSymbol GetSymbol(std::string Text, bool AutoCreate= true);
	bool Owns(const TSymbol &s);
	void GCSweep();
};
#endif

#ifdef USERP_CXX_IFACE
} // namespace
#endif

#endif // include-graud
