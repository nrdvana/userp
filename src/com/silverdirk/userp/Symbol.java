package com.silverdirk.userp;

import java.util.Arrays;
import java.io.UnsupportedEncodingException;

/**
 * <p>Project: </p>
 * <p>Title: </p>
 * <p>Description: </p>
 * <p>Copyright Copyright (c) 2004</p>
 *
 * @author not attributable
 * @version $Revision$
 */
public class Symbol {
	byte[] data;
	String str;
	int hashCache= -1;

	public Symbol(byte[] data) {
		this.data= data;
	}

	public Symbol(String name) {
		str= name;
		try {
			data= name.getBytes("UTF-8");
		}
		catch (UnsupportedEncodingException x) { // Java spec dictates this can't happen for UTF-8
			throw new Error(x);
		}
	}

	public String toString() {
		if (str == null)
			// this is thread-safe because the string is a function of the data.
			str= Util.bytesToReadableString(data);
		return str;
	}

	public int hashCode() {
		if (hashCache == -1) {
			hashCache= Arrays.hashCode(data);
			if (hashCache == -1)
				hashCache= -2;
		}
		return hashCache;
	}

	public boolean equals(Object other) {
		return other instanceof Symbol && equals((Symbol)other);
	}

	public boolean equals(Symbol other) {
		if (data == other.data)
			return true;
		if (java.util.Arrays.equals(data, other.data)) {
			// this is thread-safe, because in the worst case the buffer
			// references will get swapped and the optimization won't happen
			// (i.e. if one thread calls a.equals(b) while the other calls b.equals(a))
			other.data= data;
			return true;
		}
		return false;
	}

	public static final Symbol NIL= new Symbol(new byte[0]);
}
