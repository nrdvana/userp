package com.silverdirk.userp;

import java.util.*;
import java.math.BigInteger;
import java.nio.charset.*;
import java.nio.ByteBuffer;

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

	static final CharsetDecoder utf8dec;
	static final char[] hexChars= "0123456789ABCDEF".toCharArray();

	static {
		Charset UTF8= Charset.forName("UTF-8");
		utf8dec= UTF8.newDecoder();
		utf8dec.onMalformedInput(CodingErrorAction.REPLACE);
		utf8dec.onUnmappableCharacter(CodingErrorAction.REPLACE);
	}

	public static String bytesToReadableString(byte[] bytes) {
		synchronized (utf8dec) {
			try {
				utf8dec.reset();
				return utf8dec.decode(ByteBuffer.wrap(bytes)).toString();
			}
			// Should be no exceptions with the options I specified, but just
			// in case, this is the fallback behavior I want
			catch (CharacterCodingException ex) {
				StringBuffer sb= new StringBuffer(bytes.length*2);
				for (int i=0; i<bytes.length; i++)
					sb.append(hexChars[(bytes[i]>>4)&0x7]).append(hexChars[bytes[i]&0x7]).append(' ');
				return sb.toString();
			}
		}
	}

	static final boolean typeArrayDeepEquals(UserpType[] a, UserpType[] b, Map<UserpType.TypeHandle,UserpType.TypeHandle> equalityMap) {
		if (a.length != b.length)
			return false;
		for (int i=0,stop=a.length; i<stop; i++)
			if (!a[i].equals(b[i], equalityMap))
				return false;
		return true;
	}

	// The following is derived from
	// "Find the log base 2 of an N-bit integer in O(lg(N)) operations with multiply and lookup"
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
		int log2= MultiplyDeBruijnBitPosition[(val * 0x077CB531) >>> 27];
		return (val == 0)? 0 : log2+1; // +1 because we want bit-length, not highest-bit-set-index
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
