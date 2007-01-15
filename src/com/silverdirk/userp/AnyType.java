package com.silverdirk.userp;

import java.math.BigInteger;

/**
 * <p>Project: </p>
 * <p>Title: </p>
 * <p>Description: </p>
 * <p>Copyright Copyright (c) 2004</p>
 *
 * @author Michael Conrad / TheSilverDirk
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
			// it does, but we don't have a way to calculate it before
			// seeing the output stream, so pretend we don't
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
