package com.silverdirk.userp;

import java.util.*;
import java.lang.reflect.Array;
import java.math.BigInteger;

/**
 * <p>Project: Universal Data Trees</p>
 * <p>Title: Utility Functions</p>
 * <p>Description: Generic functions needed for the UDT library.</p>
 * <p>Copyright: Copyright (c) 2004-2006</p>
 *
 * @author Michael Conrad / TheSilverDirk
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

	public static int getBitLength(long value) {
		int bitLen= 0;
		while (value > 0xF) {
			bitLen+= 4;
			value>>>=4;
		}
		while (value != 0) {
			bitLen++;
			value>>>=1;
		}
		return bitLen;
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
