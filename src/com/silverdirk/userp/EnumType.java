package com.silverdirk.userp;

import java.util.*;
import java.math.BigInteger;

/**
 * <p>Project: Universal Serialization Protocol</p>
 * <p>Title: Type (Symbolic) Enum</p>
 * <p>Description: Symbolic enums.</p>
 * <p>Copyright Copyright (c) 2004-2007</p>
 *
 * @author Michael Conrad
 * @version $Revision$
 */
public class EnumType extends ScalarType {
	EnumDef def;

	public static class EnumDef extends TypeDef {
		Object[] spec;
		BigInteger scalarRange;
		int hashCache= -1;

		public EnumDef(Symbol[] values) {
			spec= values;
			scalarRange= BigInteger.valueOf(values.length);
		}

		public EnumDef(Object[] specComponents) {
			// assert each component is one of Symbol, String (convert to symbol),
			// TypeData, or Range.  Assert ranges are non-infinite, except last one.
			int singleCount= 0;
			spec= new Object[specComponents.length];
			BigInteger rangeWidth= BigInteger.ZERO;
			for (int i=0; i<specComponents.length; i++) {
				Object spec_i= specComponents[i];
				if (spec_i instanceof Range) {
					BigInteger width= ((Range) spec_i).getScalarRange();
					if (width == INF) {
						if (i != specComponents.length-1)
							throw new RuntimeException("An infinite range can only be specified as the last element of an enumeration");
						rangeWidth= INF;
					}
					else
						rangeWidth= rangeWidth.add(width);
				}
				else {
					if (spec_i instanceof String)
						spec_i= new Symbol((String)spec_i);
					else if (!(spec_i instanceof Symbol) && !(spec_i instanceof TypedData))
						throw new RuntimeException("Enum spec components must be one of Symbol, TypedData, or Range");
					singleCount++;
				}
				spec[i]= spec_i;
			}
			if (rangeWidth == INF)
				scalarRange= INF;
			else
				scalarRange= rangeWidth.add(BigInteger.valueOf(singleCount));
		}

		public int hashCode() {
			return Arrays.deepHashCode(spec);
		}

		public boolean equals(Object other) {
			return super.equals(other) && Arrays.deepEquals(spec, ((EnumDef)other).spec);
		}

		public String toString() {
			StringBuffer sb= new StringBuffer("Enum([");
			if (spec != null) {
				int count= spec.length;
				for (int i=0, high=Math.min(3, count-1); i<=high; i++) {
					sb.append(spec[i]);
					if (i != high) sb.append(", ");
				}
				if (count > 4)
					sb.append("... ");
				if (count >= 4)
					sb.append(", ").append(spec[count-1]);
			}
			else
				sb.append("?");
			sb.append("])");
			return sb.toString();
		}

		public final BigInteger getValueCount() {
			return scalarRange;
		}

		public final TypedData getValueByOrdinal(int ordinal) {
			throw new Error("Unimplemented");
		}

		public final int getOrdinalByValue(TypedData val) {
			throw new Error("Unimplemented");
		}
	}

	public static class Range {
		ScalarType domain;
		BigInteger from, to;

		public Range(ScalarType domain, long from, long to) {
			this(domain, BigInteger.valueOf(from), BigInteger.valueOf(to));
		}

		public Range(ScalarType domain, long from, BigInteger to) {
			this(domain, BigInteger.valueOf(from), to);
		}

		public Range(ScalarType domain, BigInteger from, BigInteger to) {
			this.domain= domain;
			this.from= from;
			this.to= to;
			if (from == INF || from == NEG_INF)
				throw new RuntimeException("A range's \"from\" cannot be infinite.");
		}

		public BigInteger getScalarRange() {
			if (to == INF || to == NEG_INF)
				return INF;
			return (from.compareTo(to) < 0? to.subtract(from) : from.subtract(to)).add(BigInteger.ONE);
		}

		public int hashCode() {
			return from.intValue() ^ to.intValue() ^ domain.hashCode();
		}

		public boolean equals(Object other) {
			return other instanceof Range && equals((Range)other);
		}

		public boolean equals(Range other) {
			return from.equals(other.from)
				// InfFlag will assert that the other is also an InfFlag, but BigInteger.ZERO will see itself equal to INF
				&& other.to.equals(to) && to.equals(other.to)
				&& domain.equals(other.domain);
		}

		// re-reference these values so users can find them more easily
		public static final InfFlag
			INF= UserpType.INF,
			NEG_INF= UserpType.NEG_INF;
	}

	public EnumType(String name) {
		this(new Symbol(name));
	}

	public EnumType(Symbol name) {
		super(name);
	}

	public EnumType init(Object[] spec) {
		return init(new EnumDef(spec));
	}

	public EnumType init(EnumDef def) {
		this.def= def;
		return this;
	}

	protected UserpType cloneAs_internal(Symbol newName) {
		return new EnumType(newName).init(def);
	}

	public TypeDef getDefinition() {
		return def;
	}

	public boolean hasEncoderParamDefaults() {
		return false;
	}

	public Codec createCodec() {
		if (def == null)
			throw new UninitializedTypeException(this, "getCodecDescriptor");
		return new EnumCodec(this);
	}

	public boolean isDoublyInfinite() {
		return false;
	}

	public final BigInteger getValueCount() {
		return def.getValueCount();
	}

	public final TypedData getValueByOrdinal(int ordinal) {
		return def.getValueByOrdinal(ordinal);
	}

	public final int getOrdinalByValue(TypedData val) {
		return def.getOrdinalByValue(val);
	}
}
