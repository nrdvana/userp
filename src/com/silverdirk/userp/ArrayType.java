package com.silverdirk.userp;

import java.math.BigInteger;

/**
 * <p>Project: Universal Serialization Protocol</p>
 * <p>Title: Type Array</p>
 * <p>Description: Type Array defines variable or fixed length arrays of elements.</p>
 * <p>Copyright Copyright (c) 2004-2007</p>
 *
 * @author Michael Conrad
 * @version $Revision$
 */
public class ArrayType extends TupleType {
	ArrayDef arrayDef;

	public static final int
		VARIABLE_LEN= TypeDef.VARIABLE_LEN;

	public static class ArrayDef extends TupleDef {
		int length;

		public ArrayDef(UserpType elemType) {
			this(TupleCoding.TIGHT, elemType, VARIABLE_LEN);
		}

		public ArrayDef(UserpType elemType, int length) {
			this(TupleCoding.TIGHT, elemType, length);
		}

		public ArrayDef(TupleCoding encoding, UserpType elemType, int length) {
			this.encoding= encoding;
			this.typeRefs= new UserpType[] { elemType };
			if (length < 0 && length != VARIABLE_LEN)
				throw new RuntimeException("'length' parameter must be non-negative, or the value ArrayDef.VARIABLE_LEN");
			this.length= length;
			if (length == VARIABLE_LEN)
				scalarBitLen= (encoding != TupleCoding.INDEFINITE)? VARIABLE_LEN : NONSCALAR;
			else
				scalarBitLen= (encoding == TupleCoding.BITPACK || encoding == TupleCoding.TIGHT)?
					UNRESOLVED : NONSCALAR;
			if (scalarBitLen == VARIABLE_LEN)
				scalarRange= INFINITE;
		}

		public boolean equals(Object other) {
			if (!(other instanceof ArrayDef))
				return false;
			ArrayDef otherDef= (ArrayDef) other;
			return encoding == otherDef.encoding
				&& typeRefs[0].equals(otherDef.typeRefs[0])
				&& length == otherDef.length;
		}

		public String toString() {
			return "Array("+encoding+", "+typeRefs[0]+", "+(length==VARIABLE_LEN? "VARIABLE_LEN" : ""+length)+")";
		}

		public UserpType resolve(PartialType pt) {
			return new ArrayType(pt.meta, this);
		}
	}

	public ArrayType(String name, UserpType elemType) {
		this(nameToMeta(name), new ArrayDef(elemType));
	}

	public ArrayType(String name, UserpType elemType, int length) {
		this(nameToMeta(name), new ArrayDef(elemType, length));
	}

	public ArrayType(String name, TupleCoding coding, UserpType elemType, int length) {
		this(nameToMeta(name), new ArrayDef(coding, elemType, length));
	}

	public ArrayType(Object[] meta, UserpType elemType) {
		this(meta, new ArrayDef(elemType));
	}

	public ArrayType(Object[] meta, UserpType elemType, int length) {
		this(meta, new ArrayDef(elemType, length));
	}

	public ArrayType(Object[] meta, TupleCoding coding, UserpType elemType, int length) {
		this(meta, new ArrayDef(coding, elemType, length));
	}

	protected ArrayType(Object[] meta, ArrayDef def) {
		super(meta, def);
		this.arrayDef= def;
		this.impl= def.encoding.coder;
		Class elemClass= arrayDef.typeRefs[0].preferredNativeStorage;
		if (elemClass != null)
			preferredNativeStorage= java.lang.reflect.Array.newInstance(elemClass, 0).getClass();
	}

	public UserpType makeSynonym(Object[] newMeta) {
		return new ArrayType(newMeta, arrayDef);
	}

	public TupleCoding getEncodingMode() {
		return arrayDef.encoding;
	}

	public int getElemCount() {
		return arrayDef.length;
	}

	public UserpType getElemType(int idx) {
		return arrayDef.typeRefs[0];
	}
}
