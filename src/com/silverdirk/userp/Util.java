package com.silverdirk.userp;

import java.util.*;
import java.lang.reflect.Array;
import java.math.BigInteger;

/**
 * <p>Project: Universal Serialization Protocol</p>
 * <p>Title: Utility Functions</p>
 * <p>Description: Generic functions needed for the Userp library.</p>
 * <p>Copyright: Copyright (c) 2004-2007</p>
 *
 * @author Michael Conrad
 * @version $Revision$
 */
public class Util {
	public static Hashtable generateMap(Object[] keys, Object[] values) {
		Hashtable result= new Hashtable();
		if (keys.length != values.length)
			throw new RuntimeException("Keys and values are not the same length");
		for (int i=0; i<keys.length; i++)
			result.put(keys[i], values[i]);
		return result;
	}

	public static String bytesToReadableString(byte[] bytes) {
		try {
			return new String(bytes, "UTF-8");
		}
		catch (Exception ex) {}
		try {
			return new String(bytes, "ISO-8859-1");
		}
		catch (Exception ex) {}
		char[] hex= new char[] {'0','1','2','3','4','5','6','7','8','9','A','B','C','D','E','F'};
		StringBuffer sb= new StringBuffer(bytes.length*2);
		for (int i=0; i<bytes.length; i++)
			sb.append(hex[(bytes[i]>>4)&0x7]).append(hex[bytes[i]&0x7]);
		return sb.toString();
	}

	// The following is "Find the log base 2 of an N-bit integer in O(lg(N)) operations with multiply and lookup"
	// from http://graphics.stanford.edu/~seander/bithacks.html
	// written by Eric Cole
	static final int[] MultiplyDeBruijnBitPosition= new int[] {
		0, 1, 28, 2, 29, 14, 24, 3, 30, 22, 20, 15, 25, 17, 4, 8,
		31, 27, 13, 23, 21, 19, 16, 7, 26, 12, 18, 6, 11, 5, 10, 9
	};
	public static int getBitLength(int val) {
		val|= val >>> 1; // first round down to power of 2
		val|= val >>> 2;
		val|= val >>> 4;
		val|= val >>> 8;
		val|= val >>> 16;
		val= (val >>> 1) + 1;
		return MultiplyDeBruijnBitPosition[(val * 0x077CB531) >>> 27];
	}
	// I didn't feel like calculating a new DeBruijin sequence, so we have this cheap hack
	public static int getBitLength(long val) {
		return (val >>> 32 != 0)? getBitLength((int)(val>>32))+32 : getBitLength((int)val);
	}

	public static int getByteLength(long value) {
		return (getBitLength(value)+7)>>3;
	}

	public static BigInteger unsignedToBigInt(int val) {
		return BigInteger.valueOf(val&0xFFFFFFFFL);
	}

	public static BigInteger unsignedToBigInt(long val) {
		BigInteger result= BigInteger.valueOf((val<<1)>>>1);
		return val < 0? result.setBit(64) : result;
	}

	public static long bigIntToLong(BigInteger val) {
		if (val.bitLength() >= 64)
			throw new LibraryLimitationException("Attempt to process "+val.bitLength()+" bits as 'long'");
		return val.longValue();
	}

	public static int bigIntToInt(BigInteger val) {
		if (val.bitLength() >= 32)
			throw new LibraryLimitationException("Attempt to process "+val.bitLength()+" bits as 'int'");
		return val.intValue();
	}

	public static int LongToInt(long val) {
		// nifty trick to check whether the upper 32 bits are "empty" on a signed number
		if ((val >> 32) != (val >> 63))
			throw new LibraryLimitationException("Attempt to process 64 bits as 'int'");
		return (int) val;
	}

	public static final Object arrayClone(Object a) {
		if (a == null) return null;
		int len= Array.getLength(a);
		Object result= Array.newInstance(a.getClass().getComponentType(), len);
		System.arraycopy(a, 0, result, 0, len);
		return result;
	}

	public static final Object arrayConcat(Object[] a, Object[] b) {
		if (a == null && b == null) return null;
		if (a == null) return b;
		if (b == null) return a;
		Object result= Array.newInstance(a.getClass().getComponentType(), a.length + b.length);
		System.arraycopy(a, 0, result, 0, a.length);
		System.arraycopy(b, 0, result, a.length, b.length);
		return result;
	}

	public static boolean arrayEquals(boolean deep, Object a, Object b) {
		if (a == b) return true;
		if (a == null || b == null) return false;
		int len= Array.getLength(a);
		if (len != Array.getLength(b))
			return false;
		for (int i=0; i<len; i++) {
			Object aElem= Array.get(a, i), bElem= Array.get(b, i);
			if (aElem != bElem)
				if (!deep || !aElem.equals(bElem))
					return false;
		}
		return true;
	}

	public static int arrayHash(Object[] a) {
		int accum= 0;
		for (int i=0; i<a.length; i++)
			accum= accum + (accum ^ a[i].hashCode());
		return accum;
	}

	static final boolean declaresInterface(Class cls, Class iface) {
		Class[] ifaces= cls.getInterfaces();
		for (int i=0; i<ifaces.length; i++)
			if (ifaces[i] == iface)
				return true;
		return false;
	}

	static final Class findJavaEquivClass(String className) {
		if (className != null) try {
			return Class.forName(className);
		}
		catch (ClassNotFoundException ex) {
		}
		return null;
	}

	static final String getReadableClassName(Class cls) {
		String name= cls.getName();
		int aryCount= 0;
		for (; aryCount<name.length() && name.charAt(aryCount) == '['; aryCount++);
		int type= name.charAt(aryCount);
		switch (type) {
		case 'B': name= "byte"; break;
		case 'C': name= "char"; break;
		case 'D': name= "double"; break;
		case 'F': name= "float"; break;
		case 'I': name= "int"; break;
		case 'J': name= "long"; break;
		case 'S': name= "short"; break;
		case 'Z': name= "boolean"; break;
		case 'V': name= "void"; break;
		case 'L':
			name= name.substring(aryCount+1, name.length()-1);
		default:
			int pkgIdx= name.lastIndexOf('.');
			if (pkgIdx != -1)
				name= name.substring(pkgIdx+1);
			int nextIdx= name.indexOf('$');
			if (nextIdx != -1) {
				StringBuffer nameBuf= new StringBuffer(name);
				do {
					nameBuf.setCharAt(nextIdx, '.');
				} while ((nextIdx=nameBuf.indexOf("$", nextIdx)) != -1);
				name= nameBuf.toString();
			}
			break;
		}
		for (int i=0; i<aryCount; i++)
			name+= "[]";
		return name;
	}

	static final RuntimeException asRuntimeEx(Exception ex) {
		return (ex instanceof RuntimeException)? (RuntimeException)ex : new RuntimeException(ex);
	}
}
