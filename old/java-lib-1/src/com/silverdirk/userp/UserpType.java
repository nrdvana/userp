package com.silverdirk.userp;

import java.util.*;
import java.math.BigInteger;

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
	Symbol name;
	public final ArrayList<TypedData> meta= new ArrayList<TypedData>();

	int hashCache= -1; // -1 == uninitialized, any other value is the actual hash
	public final TypeHandle handle= new TypeHandle(this);

	CodecOverride defaultCodec;

	protected UserpType(Symbol name) {
		this.name= name;
	}

	public Symbol getName() {
		return name;
	}

	public String getDisplayName() {
		return name.data.length == 0? "<unnamed>" : name.toString();
	}

	public List getMeta() {
		return meta;
	}

	public TypeHandle getUniqueKey() {
		return handle;
	}

	public abstract TypeDef getDefinition();

	/** Get the hash code for this type.
	 * <p>Hash codes for types are cached the first time they are requested.
	 * Only the metadata and class for the type is hashed, meaning that two
	 * types of the same class with the same names but different definitions
	 * will have colliding hash codes.  However, this should be fine in practice
	 * since types normally don't have the same name.
	 *
	 * <p>Using a semi-deep (instead of full-deep) hash calculation avoids the
	 * problem of recursion when types have circular references to eachother.
	 * There could still be a problem if there were a circular reference
	 * between a type in the metadata, but the library prevents this.
	 *
	 * @return int the semi-deep hash code
	 */
	public int hashCode() {
		if (hashCache == -1) {
			hashCache= calcHash();
			if (hashCache == -1)
				hashCache= -2;
		}
		return hashCache;
	}

	protected int calcHash() {
		TypeDef def= getDefinition();
		if (def == null)
			throw new UninitializedTypeException(this, "hashCode");
		return name.hashCode() ^ getClass().hashCode();
	}

	public boolean equals(Object other) {
		return (other instanceof UserpType) && equals((UserpType)other);
	}

	static class IntVar {
		int val;
	}

	public static final ThreadLocal<IntVar> recursionCount
		= new ThreadLocal<IntVar>() {
			protected IntVar initialValue() {
				IntVar result= new IntVar();
				result.val= 0;
				return result;
			}
		};
	private static final int RECURSION_TRIGGER= 16;

	public final boolean equals(UserpType other) {
		if (this == other)
			return true;
		if (!name.equals(other.name))
			return false;
		IntVar depth= recursionCount.get();
		if (depth.val > 0) return equals_fastImpl(other);
		if (depth.val < 0) return equals_correctImpl(other);
		return equals_root(other);
	}

	static class SuspectedInfiniteRecursion extends RuntimeException {}

	private final boolean equals_root(UserpType other) {
		boolean result;
		try {
			try {
				recursionCount.get().val= 1;
				result= equals_fastImpl(other);
			}
			catch (SuspectedInfiniteRecursion ex) {
				recursionCount.get().val= -1;
				result= equals_correctImpl(other);
				equalityMap.get().clear();
			}
		}
		finally {
			recursionCount.get().val= 0;
		}
		return result;
	}

	private final boolean equals_fastImpl(UserpType other) {
		if (recursionCount.get().val++ > RECURSION_TRIGGER)
			throw new SuspectedInfiniteRecursion();
		boolean result= definitionEquals(other);
		recursionCount.get().val--;
		return result;
	}

	public static final ThreadLocal<Map<TypeHandle,Map>> equalityMap
		= new ThreadLocal<Map<TypeHandle,Map>>() {
			protected Map<TypeHandle,Map> initialValue() {
				return new WeakHashMap<TypeHandle,Map>();
			}
		};

	private final boolean equals_correctImpl(UserpType other) {
		Map<TypeHandle,Map> equalityMap= this.equalityMap.get();
		Map equalSet= null;
		int hc1= handle.hashCode(), hc2= other.handle.hashCode();
		if (hc1 <= hc2) {
			equalSet= equalityMap.get(handle);
			if (equalSet != null && equalSet.containsKey(other.handle))
				return true;
		}
		if (hc2 <= hc1) {
			equalSet= equalityMap.get(other.handle);
			if (equalSet != null && equalSet.containsKey(handle))
				return true;
		}
		if (equalSet == null)
			equalityMap.put((hc2 <= hc1? other.handle : handle), equalSet= new WeakHashMap());
		boolean result= false;
		try {
			equalSet.put((hc2 <= hc1? handle : other.handle), null);
			result= definitionEquals(other);
		}
		finally {
			if (!result)
				equalSet.remove((hc2 <= hc1? handle : other.handle));
		}
		return result;
	}

	public final boolean definitionEquals(UserpType other) {
		TypeDef idef= getDefinition(), odef= other.getDefinition();
		if (idef == null)
			throw new UninitializedTypeException(this, "equals");
		if (odef == null)
			throw new UninitializedTypeException(other, "equals");
		return idef.equals(odef);
	}

	public UserpType cloneAs(String newName) {
		return cloneAs(new Symbol(newName));
	}

	public UserpType cloneAs(Symbol newName) {
		UserpType result= cloneAs_internal(newName);
		result.setCustomCodec(getCustomCodec());
		return result;
	}

	protected abstract UserpType cloneAs_internal(Symbol newName);

	public String toString() {
		return (name.data.length == 0? "" : getName()+"=")+getDefinition();
	}

	public abstract boolean hasEncoderParamDefaults();

	public CodecOverride getCustomCodec() {
		return defaultCodec;
	}

	public void setCustomCodec(CodecOverride cc) {
		defaultCodec= cc;
	}

	abstract Codec createCodec();

	protected static class InfFlag extends BigInteger {
		InfFlag() {
			super(new byte[] {0});
		}
		public boolean equals(Object other) {
			return other == this;
		}
	}

	public static final InfFlag
		INF= new InfFlag(),
		NEG_INF= new InfFlag();

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

	public abstract static class TypeDef {
		/** Generate a hash code for this type definition.
		 * The generated hash is a "deep hash" incorperating the hash codes of
		 * all referenced UserpTypes, however the hash code of each UserpType
		 * is only a hash of name and class.
		 * @return int The hash code for this type definition
		 */
		public abstract int hashCode();

		public boolean equals(Object other) {
			return getClass() == other.getClass();
		}

		public abstract String toString();
	}
}
