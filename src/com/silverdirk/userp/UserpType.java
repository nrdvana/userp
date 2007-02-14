package com.silverdirk.userp;

import java.math.BigInteger;
import java.util.WeakHashMap;
import java.io.IOException;

/**
 * <p>Project: Universal Serialization Protocol</p>
 * <p>Title: Type</p>
 * <p>Description: This class represents an encodable type.</p>
 * <p>Copyright Copyright (c) 2006-2007</p>
 *
 * @author Michael Conrad
 * @version $Revision$
 */
public abstract class UserpType {
	String name;
	Object[] meta;
	UserpType synOf;
	TypeDef def;
	ReaderWriterImpl impl;
	Class preferredNativeStorage;

	int hashCache= -1; // -1 == uninitialized, any other value is the actual hash
	int staticProtocolTypeCode= -1;
	public final TypeHandle handle= new TypeHandle(this);

	ThreadLocal equalitySetRef= new ThreadLocal() {
		protected Object initialValue() {
			return new WeakHashMap();
		}
	};

	protected UserpType(Object[] meta) {
		initMeta(meta);
	}
	protected void finishInit() {
	}

	public String getName() {
		return name;
	}
	public Object[] getMeta() {
		return meta;
	}
	protected void initMeta(Object[] meta) {
		byte[] nameMetaEntry= null;
		if (meta != null)
			for (int i=0; i<meta.length; i++)
				if (meta[i] instanceof TypedData) {
					TypedData td= (TypedData) meta[i];
					if (td.dataType == TypeNameSnafuTypes.TTypeName) {
						if (nameMetaEntry != null)
							throw new RuntimeException("two names specified in metadata for this type: "+nameMetaEntry+", "+((TypeName)meta[i]).name);
						nameMetaEntry= (byte[]) td.data;
					}
				}
		this.meta= meta;
		this.name= nameMetaEntry != null? Util.bytesToReadableString(nameMetaEntry) : "";
	}
	void lateInitMeta(Object[] meta) {
		initMeta(meta);
		hashCache= -1;
	}
	UserpType overrideImpl(ReaderWriterImpl impl) {
		this.impl= impl;
		return this;
	}

	public static Object nameToMetaElement(String name) {
		try {
			return new TypedData(TypeNameSnafuTypes.TTypeName, name.getBytes("UTF-8"));
		}
		catch (java.io.UnsupportedEncodingException ex) {
			throw new RuntimeException();
		}
	}

	public static Object[] nameToMeta(String name) {
		return new Object[] { nameToMetaElement(name) };
	}

	public TypeHandle getUniqueKey() {
		return handle;
	}
	public UserpType getSynonymOrigin() {
		return synOf;
	}

	public Class getNativeStoragePref() {
		return preferredNativeStorage;
	}

	public void setNativeStoragePref(Class c) {
		preferredNativeStorage= c;
	}

	public UserpType withStoPref(Class c) {
		setNativeStoragePref(c);
		return this;
	}

	protected abstract void typeSwap(UserpType from, UserpType to);

	public int hashCode() {
		if (hashCache == -1) {
			hashCache= calcHash();
			if (hashCache == -1)
				hashCache= -2;
		}
		return hashCache;
	}

	protected abstract int calcHash();

	public boolean equals(Object other) {
		return (other instanceof UserpType) && equals((UserpType)other);
	}

	public abstract boolean equals(UserpType other);

	public abstract boolean isScalar();
	public abstract boolean hasScalarComponent();
	public abstract BigInteger getScalarRange();
	public abstract int getScalarBitLen();

	public abstract UserpType resolve();

	final ReaderImpl getReaderImpl() {
		return impl;
	}

	public UserpType makeSynonym(String newName) {
		return makeSynonym(nameToMeta(newName));
	}
	public abstract UserpType makeSynonym(Object[] newMeta);

	public abstract String toString();

	public abstract int getTypeRefCount();
	public abstract UserpType getTypeRef(int idx);

	// convenience methods
	public boolean isEnum()   { return false; }
	public boolean isRange()  { return false; }
	public boolean isUnion()  { return false; }
	public boolean isTuple()  { return false; }
	public boolean isArray()  { return false; }
	public boolean isRecord() { return false; }

	public final boolean metaEquals(UserpType other) {
		return Util.arrayEquals(true, meta, other.meta);
	}
	public abstract boolean definitionEquals(UserpType other);

	public static class TypeName {
		String name;
		TypeName(String name) {
			this.name= name;
		}
		public String getName() {
			return name;
		}
	}

	/**
	 * This class simply changes the "equals" concept for a type object.
	 * I use it as the key in maps to hash on the object reference, rather than
	 * on the object value.
	 */
	public static class TypeHandle {
		public final UserpType type;

		public TypeHandle(UserpType type) {
			this.type= type;
		}
		public boolean equals(Object other) {
			// Could say "other.type == type", but the handles are 1-to-1 and this is faster.
			return other == this;
		}
	}

	abstract static class ResolvedType extends UserpType {
		public ResolvedType(Object[] meta, TypeDef def) {
			super(meta);
			this.def= def;
		}

		public String toString() {
			return name+"="+def;
		}

		protected int calcHash() {
			int metaHash= meta == null? 0 : Util.arrayHash(meta);
			int typeHash= getClass().hashCode();
			for (int i=0, stop=getTypeRefCount(); i<stop; i++)
				typeHash= ((typeHash << 3) | (typeHash >>> 29)) ^ getTypeRef(i).getClass().hashCode();
			return metaHash ^ typeHash;
		}

		public boolean equals(UserpType other) {
			if (this == other)
				return true;
			if (hashCode() != other.hashCode())
				return false;
			WeakHashMap equalitySet= (WeakHashMap) equalitySetRef.get();
			if (equalitySet.containsKey(other.handle))
				return true;
			equalitySet.put(other.handle, null);
			boolean result= false;
			try {
				result= metaEquals(other) && definitionEquals(other);
			}
			finally {
				if (!result) equalitySet.remove(other.handle);
			}
			return result;
		}

		public boolean definitionEquals(UserpType other) {
			return other instanceof ResolvedType
				&& def.equals(other.def);
		}

		public UserpType resolve() {
			return this;
		}

		public boolean isScalar() {
			return def.isScalar();
		}
		public boolean hasScalarComponent() {
			return def.hasScalarComponent();
		}
		public BigInteger getScalarRange() {
			return def.getScalarRange();
		}
		public int getScalarBitLen() {
			return def.getScalarBitLen();
		}
		public int getTypeRefCount() {
			return def.getTypeRefCount();
		}
		public UserpType getTypeRef(int idx) {
			return def.getTypeRef(idx);
		}

		protected void typeSwap(UserpType from, UserpType to){
			def.typeSwap(from, to);
		}
	}
}
