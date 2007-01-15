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
	}

	private WholeType(Object[] meta, WholeDef def) {
		super(meta, def);
		this.positive= def.positive;
	}

	public UserpType makeSynonym(Object[] newMeta) {
		return new WholeType(newMeta, (WholeDef) def);
	}

	public boolean equals(Object other) {
		return other == this;
	}

	public boolean isPositive() {
		return positive;
	}

	public int getSign() {
		return positive? 1 : -1;
	}

	public static final WholeType
		INSTANCE_TWHOLE= new WholeType(nameToMeta("TWhole"), new WholeDef(true)),
		INSTANCE_TNEGWHOLE= new WholeType(nameToMeta("TNegWhole"), new WholeDef(false));
}
