package com.silverdirk.userp;

import java.math.BigInteger;
import java.util.HashMap;

/**
 * <p>Project: </p>
 * <p>Title: </p>
 * <p>Description: </p>
 * <p>Copyright Copyright (c) 2004</p>
 *
 * @author Michael Conrad / TheSilverDirk
 * @version $Revision$
 */
public class UnionType extends UserpType.ResolvedType {
	UnionDef unionDef;
	UnionTypeCoder coder;
	HashMap typeIdxMap= null; // remains null unless threshhold is exceeded

	static final int
		HASH_THRESHHOLD= 4; // max number of subtypes we allow before we decide to hash them

	public static class UnionDef extends TypeDef {
		boolean bitpacked;
		boolean[] inlined;
		BigInteger[] offsets;
		long[] offsets_l;
		boolean pureScalar;

		public UnionDef(UserpType[] subTypes) {
			this(false, subTypes, null);
		}

		public UnionDef(UserpType[] subTypes, boolean[] inlined) {
			this(false, subTypes, inlined);
		}

		public UnionDef(boolean bitpacked, UserpType[] subTypes, boolean[] inlined) {
			this.bitpacked= bitpacked;
			if (subTypes.length < 2)
				throw new RuntimeException("Cannot form a union of fewer than two sub-types");
			typeRefs= (UserpType[]) Util.arrayClone(subTypes);
			if (inlined != null && inlined.length != typeRefs.length)
				throw new RuntimeException("The size of the 'inlined' array must match the number of sub-types");
			this.inlined= (boolean[]) Util.arrayClone(inlined);
		}

		public String toString() {
			StringBuffer sb= new StringBuffer("Union([");
			for (int i=0, max=typeRefs.length-1; i <= max; i++) {
				if (!inlined[i]) sb.append('(');
				sb.append(typeRefs[i].name);
				if (!inlined[i]) sb.append('}');
				if (i != max) sb.append(", ");
			}
			sb.append("])");
			return sb.toString();
		}

		public boolean equals(Object other) {
			if (!(other instanceof UnionDef)) return false;
			UnionDef otherDef= (UnionDef) other;
			// could use Util.arrayEquals, but it'd be rather inefficient.
			if (!Util.arrayEquals(true, typeRefs, otherDef.typeRefs))
				return false;
			for (int i=0; i<inlined.length; i++)
				if (inlined[i] != otherDef.inlined[i])
					return false;
			return bitpacked == otherDef.bitpacked;
		}

		public boolean isScalar() {
			if (scalarBitLen == UNRESOLVED)
				calcScalarAndOffsets();
			return pureScalar;
		}

		public boolean hasScalarComponent() {
			return true;
		}

		public BigInteger getScalarRange() {
			if (scalarBitLen == UNRESOLVED)
				calcScalarAndOffsets();
			return scalarRange;
		}

		public int getScalarBitLen() {
			if (scalarBitLen == UNRESOLVED)
				calcScalarAndOffsets();
			return scalarBitLen;
		}

		public UserpType resolve(PartialType pt) {
			if (inlined == null && !tryCalcInlines())
				throw new RuntimeException("The \"inlined\" param was not given, and there is not enough information available to determine a default.");
			if (scalarBitLen == UNRESOLVED)
				calcScalarAndOffsets();
			return new UnionType(pt.meta, this);
		}

		private void calcScalarAndOffsets() {
			pureScalar= true;
			BigInteger offsetVal= BigInteger.ZERO;
			BigInteger[] offsetCalc= new BigInteger[typeRefs.length];
			offsetCalc[0]= BigInteger.ZERO;
			for (int i= 0; i<typeRefs.length; i++) {
				if (inlined[i]) {
					if (!typeRefs[i].isScalar())
						pureScalar= false;
					if (!typeRefs[i].hasScalarComponent())
						throw new RuntimeException("sub-type "+typeRefs[i].getName()+" has no scalar component, but was specified to be inlined");
					else if (typeRefs[i].getScalarRange() == INFINITE) {
						if (i != typeRefs.length-1)
							throw new RuntimeException("sub-type "+typeRefs[i].getName()+" is infinite, and inlined, and is not the last sub-type");
						else
							offsetVal= INFINITE;
					}
					else
						offsetVal= offsetVal.add(typeRefs[i].getScalarRange());
				}
				else {
					offsetVal= offsetVal.add(BigInteger.ONE);
					pureScalar= false;
				}
				if (i != typeRefs.length-1)
					offsetCalc[i+1]= offsetVal;
			}
			setScalarRange(offsetVal);
			offsets= offsetCalc;

			offsets_l= new long[offsets.length];
			for (int i=0; i<offsets.length; i++)
				offsets_l[i]= offsets[i].bitLength() < 64? offsets[i].longValue() : Long.MAX_VALUE;
			if (offsetVal == INFINITE)
				offsets_l[offsets_l.length]= Long.MAX_VALUE;
		}

		static int findRangeIdxOf(BigInteger[] offsets, BigInteger val) {
			int min= 0, max= offsets.length-1, mid= 0;
			while (min < max) {
				mid= (min+max+1) >> 1;
				if (val.compareTo(offsets[mid]) < 0) max= mid-1;
				else min= mid;
			}
			return max;
		}

		static int findRangeIdxOf(long[] offsets, long val) {
			int min= 0, max= offsets.length-1, mid= 0;
			while (min < max) {
				mid= (min+max+1) >> 1;
				if (val < offsets[mid]) max= mid-1;
				else min= mid;
			}
			return max;
		}

		/** Attempt to calculate whether each sub-type can be inlined into the selector value.
		 * This method is named 'try' because it will might fail if one of the
		 * sub-types has not been resolved.
		 *
		 * @return true, if 'inlined' was successfully calculated, and false otherwise
		 */
		protected boolean tryCalcInlines() {
			try {
				boolean[] inlineCalc= new boolean[typeRefs.length];
				for (int i=0; i<typeRefs.length; i++)
					inlineCalc[i]= (typeRefs[i].isScalar() || typeRefs[i].hasScalarComponent())
						&& (i == typeRefs.length-1 || typeRefs[i].getScalarRange() != INFINITE);
				inlined= inlineCalc;
				return true;
			}
			catch (Exception ex) {
				return false;
			}
		}
	}

	public UnionType(String name, UserpType[] subTypes) {
		this(nameToMeta(name), new UnionDef(subTypes));
	}

	public UnionType(String name, UserpType[] subTypes, boolean[] inlined) {
		this(nameToMeta(name), new UnionDef(subTypes, inlined));
	}

	public UnionType(String name, boolean bitpacked, UserpType[] subTypes, boolean[] inlined) {
		this(nameToMeta(name), new UnionDef(bitpacked, subTypes, inlined));
	}

	public UnionType(Object[] meta, UserpType[] subTypes) {
		this(meta, new UnionDef(subTypes));
	}

	public UnionType(Object[] meta, UserpType[] subTypes, boolean[] inlined) {
		this(meta, new UnionDef(subTypes, inlined));
	}

	public UnionType(Object[] meta, boolean bitpacked, UserpType[] subTypes, boolean[] inlined) {
		this(meta, new UnionDef(bitpacked, subTypes, inlined));
	}

	protected UnionType(Object[] meta, UnionDef def) {
		super(meta, def);
		unionDef= def;
		boolean fitsInLong= unionDef.getScalarRange() != TypeDef.INFINITE && unionDef.getScalarRange().bitLength() < 64;
		coder= fitsInLong? (UnionTypeCoder) UnionTypeCoder_Long.INSTANCE : UnionTypeCoder_BigInt.INSTANCE;
	}

	protected void finishInit() {
		if (def.typeRefs.length > HASH_THRESHHOLD) {
			typeIdxMap= new HashMap();
			for (int i=0; i<def.typeRefs.length; i++)
				typeIdxMap.put(def.typeRefs[i].handle, new Integer(i));
		}
	}

	public UserpType makeSynonym(Object[] newMeta) {
		UnionType result= new UnionType(newMeta, unionDef);
		result.typeIdxMap= typeIdxMap;
		return result;
	}

	public int getSubtypeCount() {
		return def.typeRefs.length;
	}

	public UserpType getSubtype(int idx) {
		return def.typeRefs[idx];
	}

	public int getSubtypeIdx(UserpType type) {
		// if we have a lookup table, use it
		if (typeIdxMap != null) {
			Integer result= (Integer) typeIdxMap.get(type.handle);
			if (result != null)
				return result.intValue();
		}
		// else use the boring algorithm
		for (int i=0; i<def.typeRefs.length; i++)
			if (def.typeRefs[i] == type)
				return i;
		// else fail
		return -1;
	}

	public void typeSwap(UserpType from, UserpType to) {
		if (typeIdxMap != null && from instanceof UserpType) {
			Integer idx= (Integer) typeIdxMap.get(((UserpType)from).handle);
			if (idx != null) {
				def.typeRefs[idx.intValue()]= to;
				typeIdxMap.remove(((UserpType)from).handle);
				typeIdxMap.put(to.handle, idx);
			}
		}
		else
			super.typeSwap(from, to);
	}

	public boolean isScalar() {
		return unionDef.pureScalar;
	}

	public boolean hasScalarComponent() {
		return true;
	}

	public BigInteger getScalarRange() {
		return unionDef.scalarRange;
	}

	public int getScalarBitLen() {
		return unionDef.scalarBitLen;
	}

	void decode(UserpReader reader) {
		int typeIdx= coder.decode(unionDef, reader);
		reader.unionSubtype= getSubtype(typeIdx);
	}

	static abstract class UnionTypeCoder {
		abstract int decode(UnionDef def, UserpReader reader);
	}

	static class UnionTypeCoder_Long extends UnionTypeCoder {
		int decode(UnionDef def, UserpReader reader) {
			long val= reader.scalarBig == null? reader.scalar64
				: Util.bigIntToLong(reader.scalarBig);
			reader.scalarBig= null;
			int typeIdx= def.findRangeIdxOf(def.offsets_l, val);
			if (def.inlined[typeIdx])
				reader.scalar64-= def.offsets_l[typeIdx];
			return typeIdx;
		}
		static final UnionTypeCoder_Long INSTANCE= new UnionTypeCoder_Long();
	}

	static class UnionTypeCoder_BigInt extends UnionTypeCoder {
		int decode(UnionDef def, UserpReader reader) {
			int typeIdx;
			if (reader.scalarBig == null && reader.scalar64 < Long.MAX_VALUE) {
				typeIdx= def.findRangeIdxOf(def.offsets_l, reader.scalar64);
				if (def.inlined[typeIdx])
					reader.scalar64-= def.offsets_l[typeIdx];
			}
			else {
				if (reader.scalarBig == null)
					reader.scalarBig= BigInteger.valueOf(reader.scalar64);
				typeIdx= def.findRangeIdxOf(def.offsets, reader.scalarBig);
				if (def.inlined[typeIdx])
					reader.scalarBig= reader.scalarBig.subtract(def.offsets[typeIdx]);
			}
			return typeIdx;
		}
		static final UnionTypeCoder_BigInt INSTANCE= new UnionTypeCoder_BigInt();
	}
}
