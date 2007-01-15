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
abstract class TypeDef {
	UserpType[] typeRefs= EMPTY_REF_LIST;
	protected int
		scalarBitLen= UNRESOLVED;
	BigInteger scalarRange= NONSCALAR;

	public String toString() {
		return Util.getReadableClassName(this.getClass());
	}

	public abstract boolean equals(Object other);

	public abstract boolean isScalar();

	public abstract boolean hasScalarComponent();

	public BigInteger getScalarRange() {
		throw new UnsupportedOperationException();
	}

	public int getScalarBitLen() {
		throw new UnsupportedOperationException();
	}

	public int getTypeRefCount() {
		return typeRefs.length;
	}

	public UserpType getTypeRef(int idx) {
		return typeRefs[idx];
	}

	public int getSubtypeIdx(UserpType t) {
		for (int i=0; i<typeRefs.length; i++)
			if (typeRefs[i] == t)
				return i;
		return -1;
	}

	void typeSwap(UserpType from, UserpType to) {
		for (int i=0; i<typeRefs.length; i++)
			if (typeRefs[i] == from)
				typeRefs[i]= to;
	}

	abstract UserpType resolve(PartialType pt);

	protected void setScalarRange(BigInteger range) {
		scalarRange= range;
		if (range != INFINITE)
			scalarBitLen= range.subtract(BigInteger.ONE).bitLength();
		else
			scalarBitLen= VARIABLE_LEN;
	}

	public static final int
		UNRESOLVED= -2,   // value unknown because type is not yet resolved
		VARIABLE_LEN= -1; // length of data varies
	public static final BigInteger
		INFINITE= BigInteger.valueOf(-1),   // scalar value is infinite
		PARTSCALAR= BigInteger.valueOf(-2), // type begins with a scalar component
		NONSCALAR= BigInteger.valueOf(-3);  // type is not a scalar type
	static final UserpType[] EMPTY_REF_LIST= new UserpType[0];
}
