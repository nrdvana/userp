package com.silverdirk.userp;

import java.math.BigInteger;
import java.io.IOException;

/**
 * <p>Project: Universal Serialization Protocol</p>
 * <p>Title: Type Range</p>
 * <p>Description: Range types allow a subset of values that exist in a scalar base type.</p>
 * <p>Copyright Copyright (c) 2004-2007</p>
 *
 * @author Michael Conrad
 * @version $Revision$
 */
public class RangeType extends UserpType.ResolvedType {
	RangeDef rangeDef;

	public static class RangeDef extends TypeDef {
		BigInteger from, to;
		BigInteger min, max;
		long min_l, max_l;
		boolean invert;

		public RangeDef(UserpType base, long from, long to) {
			this(base, BigInteger.valueOf(from), BigInteger.valueOf(to));
		}
		public RangeDef(UserpType base, BigInteger from, BigInteger to) {
			if (!base.isScalar())
				throw new RuntimeException("\""+base+"\" is not a scalar type, and cannot be the base type of a range");
			if ((from.signum() < 0)
				|| to.signum() < 0 && to != INFINITE)
				throw new RuntimeException("Range bounds refer to the scalar component of the base type; 'from' must be non-negative, and 'to' must be either non-negative or the constant RangeDef.INFINITE");
			typeRefs= new UserpType[] { base };
			this.from= from;
			this.to= to;
			min= from;
			max= to;
			invert= false;
			if (to != INFINITE) {
				if (from.compareTo(to) > 0) {
					invert= true;
					max= from;
					min= to;
				}
				BigInteger range= max.subtract(min).add(BigInteger.ONE);
				setScalarRange(range);
			}
			else
				setScalarRange(INFINITE);
			min_l= min.longValue();
			max_l= max.longValue();
		}

		public String toString() {
			return "RangeDef("+getBaseType()+", "+from+", "+to+")";
		}

		public boolean equals(Object other) {
			if (!(other instanceof RangeDef)) return false;
			RangeDef otherDef= (RangeDef) other;
			return from.equals(otherDef.from)
				&& to.equals(otherDef.to)
				&& typeRefs[0].equals(otherDef.typeRefs[0]);
		}

		public UserpType getBaseType() {
			return typeRefs[0];
		}

		public boolean isScalar() {
			return true;
		}

		public boolean hasScalarComponent() {
			return true;
		}

		public BigInteger getScalarRange() {
			return scalarRange;
		}

		public int getScalarBitLen() {
			return scalarBitLen;
		}

		public UserpType resolve(PartialType pt) {
			return new RangeType(pt.meta, this);
		}
	}

	public RangeType(String name, UserpType base, long from, long to) {
		this(nameToMeta(name), new RangeDef(base, from, to));
	}

	public RangeType(String name, UserpType base, BigInteger from, BigInteger to) {
		this(nameToMeta(name), new RangeDef(base, from, to));
	}

	public RangeType(Object[] meta, UserpType base, long from, long to) {
		this(meta, new RangeDef(base, from, to));
	}

	public RangeType(Object[] meta, UserpType base, BigInteger from, BigInteger to) {
		this(meta, new RangeDef(base, from, to));
	}

	protected RangeType(Object[] meta, RangeDef def) {
		super(meta, def);
		this.rangeDef= def;
		if (rangeDef.from.bitLength() < 64 && rangeDef.to != RangeDef.INFINITE && rangeDef.to.bitLength() < 64)
			impl= RangeImpl_Long.INSTANCE;
		else if (rangeDef.from.bitLength() < 64 && rangeDef.invert == false)
			impl= RangeImpl_InfRange.INSTANCE;
		else
			impl= RangeImpl_BigInt.INSTANCE;
	}

	public UserpType makeSynonym(Object[] meta) {
		return new RangeType(meta, rangeDef);
	}

	public BigInteger getFrom() {
		return rangeDef.from;
	}

	public BigInteger getTo() {
		return rangeDef.to;
	}

	public UserpType getBaseType() {
		return rangeDef.getBaseType();
	}

	public boolean isRange() {
		return true;
	}
}

