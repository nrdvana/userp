#ifndef USERP_SYMBOL_H
#define USERP_SYMBOL_H

#include <stdint.h>

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
#endif

#if 0
#include <string>

namespace Userp {

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

class TSymbolTable;

class TSymbol {
	TSymbolTable *owner;
	const char* asText_Cache;
	int len;
	uint8_t data[];
	
	void RenderAsText();
	TSymbol() {}
	~TSymbol() {}
public:
	inline TConstBytes Data() const { return TConstBytes(data, len); }
	const char* asText();

	inline bool Equals(const TSymbol &Peer) const { return (this == &Peer) || (owner != Peer.owner && Data() == Peer.Data()); }
	bool Equals(const char* CStr) const { return Data() == TConstBytes((const uint8_t*)CStr, strlen(CStr)); }
	bool Equals(const std::string &Str) const { return Data() == TConstBytes((const uint8_t*)Str.data(), Str.size()); }

	inline bool operator == (const TSymbol &Peer) const { return Equals(Peer); }
	inline bool operator < (const TSymbol &Peer) const { return Data() < Peer.Data(); }

	friend class TSymbolTable;
};

class TSymbolTable {
	class Implementation;
	Implementation *impl;
public:
	TSymbolTable();
	~TSymbolTable();
	TSymbol* GetSymbol(TConstBytes Data, bool AutoCreate= true);
	TSymbol* GetSymbol(const char* Text, bool AutoCreate= true);
	TSymbol* GetSymbol(std::string Text, bool AutoCreate= true);
	bool Owns(TSymbol *s) { return s->owner == this; }
};

} // namespace

#endif
#endif