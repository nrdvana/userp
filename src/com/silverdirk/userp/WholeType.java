package com.silverdirk.userp;

import java.math.BigInteger;

/**
 * <p>Project: Universal Serialization Protocol</p>
 * <p>Title: Type Whole</p>
 * <p>Description: This class represents the set of either positive or negative whole integers.</p>
 * <p>Copyright Copyright (c) 2006-2007</p>
 *
 * @author Michael Conrad
 * @version $Revision$
 */
public class WholeType extends UserpType.ResolvedType {
	boolean positive;

	private static class WholeDef extends TypeDef {
		boolean positive;

		WholeDef(boolean positive) {
			this.positive= positive;
		}

		public String toString() {
			return "Whole("+(positive?"positive":"negative")+")";
		}

		public boolean equals(Object other) {
			return other == this;
		}

		public boolean isScalar() {
			return true;
		}
		public boolean hasScalarComponent() {
			return true;
		}
		public BigInteger getScalarRange() {
			return TypeDef.INFINITE;
		}
		public int getScalarBitLen() {
			return TypeDef.VARIABLE_LEN;
		}
		protected UserpType resolve(PartialType pt) {
			throw new UnsupportedOperationException();
		}

		static final WholeDef
			POS_INSTANCE= new WholeDef(true),
			NEG_INSTANCE= new WholeDef(false);
	}

	WholeType(Object[] meta, boolean positive) {
		super(meta, positive? WholeDef.POS_INSTANCE : WholeDef.NEG_INSTANCE);
		this.positive= positive;
		this.impl= positive? WholeImpl.INSTANCE : NegWholeImpl.INSTANCE;
	}

	public UserpType makeSynonym(Object[] newMeta) {
		return new WholeType(newMeta, positive);
	}

	public boolean equals(Object other) {
		return other instanceof WholeType
			&& ((WholeType)other).positive == positive;
	}

	public boolean isPositive() {
		return positive;
	}

	public int getSign() {
		return positive? 1 : -1;
	}

	public static final boolean POSITIVE= true, NEGATIVE= false;
}
