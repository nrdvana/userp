package com.silverdirk.userp;

import java.math.BigInteger;

/**
 * <p>Project: Universal Serialization Protocol</p>
 * <p>Title: Type Tuple</p>
 * <p>Description: Tuple handles a lot of the functionality needed for Arrays and Records.</p>
 * <p>Copyright Copyright (c) 2004-2007</p>
 *
 * @author Michael Conrad
 * @version $Revision$
 */
public abstract class TupleType extends UserpType.ResolvedType {
	/**
	 * This will be replaced with a Java enum as soon as Java5 is popular
	 * enough for general use.
	 */
	public static class TupleCoding {
		ReaderWriterImpl coder;
		int idx;
		TupleCoding(int idx, ReaderWriterImpl c) { this.idx= idx; coder= c; }

		public static final TupleCoding
			INDEFINITE= new TupleCoding(0, IndefiniteCoder.INSTANCE),
			INDEXED=    new TupleCoding(1, IndexedCoder.INSTANCE),
			TIGHT=      new TupleCoding(2, TightCoder.INSTANCE),
			BITPACK=    new TupleCoding(3, TightCoder.INSTANCE_BITPACK);
	}

	abstract static class TupleDef extends TypeDef {
		TupleCoding encoding;

		public TupleCoding getEncodingMode() {
			return encoding;
		}

		public boolean isScalar() {
			return false;
		}

		public boolean hasScalarComponent() {
			if (scalarBitLen == UNRESOLVED)
				calcScalar();
			return scalarBitLen != NONSCALAR;
		}

		public BigInteger getScalarRange() {
			if (scalarBitLen == UNRESOLVED)
				calcScalar();
			if (scalarBitLen == NONSCALAR)
				throw new UnsupportedOperationException();
			return scalarRange;
		}

		public int getScalarBitLen() {
			if (scalarBitLen == UNRESOLVED)
				calcScalar();
			if (scalarBitLen == NONSCALAR)
				throw new UnsupportedOperationException();
			return scalarBitLen;
		}

		private void calcScalar() {
			scalarBitLen= typeRefs[0].getScalarBitLen();
			scalarRange= typeRefs[0].getScalarRange();
		}
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
