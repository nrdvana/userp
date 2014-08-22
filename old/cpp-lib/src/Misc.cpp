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
