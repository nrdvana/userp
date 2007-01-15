package com.silverdirk.userp;

import java.util.HashMap;
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
public class EnumType extends UserpType.ResolvedType {
	EnumDef enumDef;
	BigInteger scalarRange;
	int scalarBitLenCache;
	HashMap valueLookup= null;

	public static class EnumDef extends TypeDef {
		boolean symbolic;
		Object[] members;

		public EnumDef(boolean symbolic, Object[] members) {
			this.symbolic= symbolic;
			this.members= members;
			scalarRange= BigInteger.valueOf(members.length);
			scalarBitLen= Util.getBitLength(members.length);
		}

		public String toString() {
			StringBuffer sb= new StringBuffer("Enum([");
			int count= members.length;
			for (int i=0, high=Math.min(3, count-1); i<=high; i++) {
				sb.append(members[i]);
				if (i != high) sb.append(", ");
			}
			if (count > 4)
				sb.append("... ");
			if (count >= 4)
				sb.append(", ").append(members[count-1]);
			sb.append("])");
			return sb.toString();
		}

		public boolean equals(Object other) {
			if (!(other instanceof EnumDef)) return false;
			EnumDef otherEnum= (EnumDef) other;
			return symbolic == otherEnum.symbolic
				&& Util.arrayEquals(true, members, otherEnum.members);
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

		UserpType resolve(PartialType pt) {
			return new EnumType(pt.meta, this);
		}
	}

	public EnumType(String name, boolean isSymbolic, Object[] members) {
		this(nameToMeta(name), new EnumDef(isSymbolic, members));
	}

	public EnumType(Object[] meta, boolean isSymbolic, Object[] members) {
		this(meta, new EnumDef(isSymbolic, members));
		finishInit();
	}
	protected EnumType(Object[] meta, EnumDef def) {
		super(meta, def);
		this.enumDef= def;
		valueLookup= new HashMap();
		for (int i=0, stop=getMemberCount(); i<stop; i++)
			valueLookup.put(getMember(i), new Integer(i));
	}

	public UserpType makeSynonym(Object[] newMeta) {
		EnumType result= new EnumType(newMeta, (EnumDef) def);
		result.valueLookup= valueLookup;
		return result;
	}

//	protected void finishInit() {
//	}

	public boolean isSymbolic() {
		return enumDef.symbolic;
	}

	public UserpType getMemberType() {
		return enumDef.symbolic? (UserpType)CommonTypes.TSymbol : FundamentalTypes.TAny;
	}

	public int getMemberCount() {
		return enumDef.members.length;
	}

	public Object getMember(int idx) {
		return enumDef.members[idx];
	}

	public int getMemberIdxByValue(Object val) {
		Integer idx= (Integer) valueLookup.get(val);
		return idx != null? idx.intValue() : -1;
	}

	public boolean isEnum() {
		return true;
	}
}
