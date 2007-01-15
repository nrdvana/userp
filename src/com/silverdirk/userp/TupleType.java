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
public abstract class TupleType extends UserpType.ResolvedType {
	/**
	 * This will be replaced with a Java enum as soon as Java5 is popular
	 * enough for general use.
	 */
	public static class TupleCoding {
		public static final TupleCoding
			INHERIT=    new TupleCoding(),
			INDEFINITE= new TupleCoding(),
			INDEXED=    new TupleCoding(),
			TIGHT=      new TupleCoding(),
			BITPACK=    new TupleCoding();
	}

	protected TupleType(Object[] meta, TypeDef def) {
		super(meta, def);
	}

	public abstract TupleCoding getEncodingMode();

	public boolean isScalar() {
		return false;
	}

	public boolean isTuple() {
		return true;
	}

	public abstract int getElemCount();
	public abstract UserpType getElemType(int idx);
}
