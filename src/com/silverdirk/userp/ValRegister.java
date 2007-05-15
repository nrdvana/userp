package com.silverdirk.userp;

import java.math.BigInteger;

public class ValRegister {
	Object obj;
	long i64;
	StoType sto;

	public enum StoType {
		BOOL(boolean.class), BYTE(byte.class), SHORT(short.class), INT(int.class),
		LONG(long.class), FLOAT(float.class), DOUBLE(double.class), CHAR(char.class),
		OBJECT(Object.class);

		public final Class javaCls;
		private StoType(Class javaCls) {
			this.javaCls= javaCls;
		}
	};

	static final StoType[] nativeTypeByteLenMap= new ValRegister.StoType[] {
		ValRegister.StoType.BYTE, ValRegister.StoType.SHORT,
		ValRegister.StoType.INT,  ValRegister.StoType.INT,
		ValRegister.StoType.LONG, ValRegister.StoType.LONG,
		ValRegister.StoType.LONG, ValRegister.StoType.LONG,
	};
	public static ValRegister.StoType getTypeForBitLen(int bits) {
		return nativeTypeByteLenMap[bits>>3];
	}

	public final Object asObject() {
		switch (sto) {
		case BOOL:   return Boolean.valueOf(i64 != 0);
		case BYTE:   return new Byte((byte)i64);
		case SHORT:  return new Short((short)i64);
		case INT:    return new Integer((int)i64);
		case LONG:   return new Long(i64);
		case FLOAT:  return new Float(Float.intBitsToFloat((int)i64));
		case DOUBLE: return new Double(Double.longBitsToDouble(i64));
		case CHAR:   return new Character((char)i64);
		case OBJECT: return obj;
		default:
			throw new Error();
		}
	}

	public final boolean asBool() {
		return asLong() != 0;
	}

	public final byte asByte() {
		return (byte) asLong();
	}

	public final short asShort() {
		return (short) asLong();
	}

	public final int asInt() {
		return (int) asLong();
	}

	public final long asLong() {
		switch (sto) {
		case BOOL:  case BYTE:
		case SHORT: case INT:
		case CHAR:  case LONG:
			return i64;
		case FLOAT:  return (long) Float.intBitsToFloat((int)i64);
		case DOUBLE: return (long) Double.longBitsToDouble(i64);
		case OBJECT: return ((Number)obj).longValue();
		default:
			throw new Error();
		}
	}

	public final float asFloat() {
		switch (sto) {
		case BOOL:  case BYTE:
		case SHORT: case INT:
		case CHAR:  case LONG:
			return (float) i64;
		case FLOAT:  return Float.intBitsToFloat((int)i64);
		case DOUBLE: return (float) Double.longBitsToDouble(i64);
		case OBJECT: return ((Number)obj).floatValue();
		default:
			throw new Error();
		}
	}

	public final double asDouble() {
		switch (sto) {
		case BOOL:  case BYTE:
		case SHORT: case INT:
		case CHAR:  case LONG:
			return (double) i64;
		case FLOAT:  return (double) Float.intBitsToFloat((int)i64);
		case DOUBLE: return Double.longBitsToDouble(i64);
		case OBJECT: return ((Number)obj).doubleValue();
		default:
			throw new Error();
		}
	}

	public final char asChar() {
		return (char) asLong();
	}

	public final BigInteger asBigInt() {
		switch (sto) {
		case BOOL:  case BYTE:
		case SHORT: case INT:
		case CHAR:  case LONG:
			return BigInteger.valueOf(i64);
		case FLOAT:  return BigInteger.valueOf((long) Float.intBitsToFloat((int)i64));
		case DOUBLE: return BigInteger.valueOf((long) Double.longBitsToDouble(i64));
		case OBJECT:
			return obj instanceof BigInteger?
				(BigInteger) obj
				: BigInteger.valueOf(((Number)obj).longValue());
		default:
			throw new Error();
		}
	}
}
