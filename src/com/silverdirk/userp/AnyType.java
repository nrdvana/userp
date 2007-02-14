package com.silverdirk.userp;

import java.math.BigInteger;

/**
 * <p>Project: Universal Serialization Protocol</p>
 * <p>Title: Type Any</p>
 * <p>Description: Type Any is the theoretical union of all definable types.</p>
 * <p>Copyright Copyright (c) 2004-2007</p>
 *
 * @author Michael Conrad
 * @version $Revision$
 */
public class AnyType extends UserpType.ResolvedType {
	private static class AnyDef extends TypeDef {
		public boolean equals(Object other) {
			return other instanceof AnyDef;
		}

		public boolean isScalar() {
			return false;
		}

		public boolean hasScalarComponent() {
			// It does, but we don't have a way to calculate it before
			//   seeing the output stream, so pretend we don't
			// Also, we don't want the reader to get our scalar for us, since
			//   readType() takes care of it.
			return false;
		}

		public BigInteger getScalarRange() {
			throw new UnsupportedOperationException();
		}

		public int getScalarBitLen() {
			return TypeDef.VARIABLE_LEN;
		}

		public UserpType resolve(PartialType pt) {
			throw new UnsupportedOperationException();
		}

		static final AnyDef INSTANCE= new AnyDef();
	}

	private AnyType(String name) {
		super(nameToMeta(name), AnyDef.INSTANCE);
		impl= AnyImpl.INSTANCE;
		preferredNativeStorage= TypedData.class;
	}

	public UserpType makeSynonym(Object[] newName) {
		throw new UnsupportedOperationException("Cannot make synonyms of TAny");
	}

	public String toString() {
		return name;
	}

	public boolean equals(Object other) {
		return other == this;
	}

	public static final AnyType INSTANCE_TANY= new AnyType("TAny");
}
