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
public class RangeType extends UserpType.ResolvedType {
	RangeDef rangeDef;

	public static class RangeDef extends TypeDef {
		BigInteger from, to;
		BigInteger min, max;

		public RangeDef(UserpType base, long from, long to) {
			this(base, BigInteger.valueOf(from), BigInteger.valueOf(to));
		}
		public RangeDef(UserpType base, BigInteger from, BigInteger to) {
			if (!base.isScalar())
				throw new RuntimeException("\""+base+"\" is not a scalar type, and cannot be the base type of a range");
			if ((from.signum() < 0)
				|| to.signum() < 0 && to != INFINITE)
				throw new RuntimeException("Range bounds refer to the scalar component of the base type- 'from' must be non-negative, and 'to' must be either non-negative or the constant RangeDef.INFINITE");
			typeRefs= new UserpType[] { base };
			this.from= from;
			this.to= to;
			boolean reverse= min.compareTo(max) > 0;
			this.min= reverse? to : from;
			this.max= reverse? from : to;
			if (to != INFINITE) {
				BigInteger range= max.subtract(min).add(BigInteger.ONE);
				setScalarRange(range);
			}
			else
				setScalarRange(INFINITE);
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
		finishInit();
	}

	public RangeType(Object[] meta, UserpType base, BigInteger from, BigInteger to) {
		this(meta, new RangeDef(base, from, to));
		finishInit();
	}

	protected RangeType(Object[] meta, RangeDef def) {
		super(meta, def);
		this.rangeDef= def;
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
