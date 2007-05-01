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
	public final List meta= new LinkedList();

	int hashCache= -1; // -1 == uninitialized, any other value is the actual hash
	public final TypeHandle handle= new TypeHandle(this);

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

	public final boolean equals(UserpType other) {
		return equals(other, new HashMap<TypeHandle,TypeHandle>());
	}

	public final boolean definitionEquals(UserpType other) {
		HashMap equalityMap= new HashMap<TypeHandle,TypeHandle>();
		equalityMap.put(handle, other.handle);
		return definitionEquals(other, equalityMap);
	}

	protected final boolean equals(UserpType other, Map<TypeHandle,TypeHandle> equalityMap) {
		equalityMap.put(handle, other.handle);
		return name.equals(other.name) && definitionEquals(other, equalityMap);
	}

	protected final boolean definitionEquals(UserpType other, Map<TypeHandle,TypeHandle> equalityMap) {
		TypeDef idef= getDefinition(), odef= other.getDefinition();
		if (idef == null)
			throw new UninitializedTypeException(this, "equals");
		if (odef == null)
			throw new UninitializedTypeException(other, "equals");
		return idef.equals(odef, equalityMap);
	}

	public UserpType cloneAs(String newName) {
		return cloneAs(new Symbol(newName));
	}

	public abstract UserpType cloneAs(Symbol newName);

	public String toString() {
		return getName()+"="+getDefinition();
	}

	public abstract boolean hasEncoderParamDefaults();

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

	interface RecursiveAware {
		public boolean equals(Object other, Map<TypeHandle,TypeHandle> equalityMap);
	}

	public abstract static class TypeDef implements RecursiveAware {
		/** Generate a hash code for this type definition.
		 * The generated hash is a "deep hash" incorperating the hash codes of
		 * all referenced types, however the hash code is a type only a hash
		 * of the metadata.
		 * @return int The hash code for this type definition
		 */
		public abstract int hashCode();

		public boolean equals(Object other) {
			return equals((TypeDef)other, new HashMap<TypeHandle,TypeHandle>());
		}

		public boolean equals(Object other, Map<TypeHandle,TypeHandle> equalityMap) {
			return (other instanceof TypeDef) && equals(other, equalityMap);
		}

		protected abstract boolean equals(TypeDef other, Map<TypeHandle,TypeHandle> equalityMap);

		public abstract String toString();
	}
}
