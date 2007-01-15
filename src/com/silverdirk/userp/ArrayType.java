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
public class ArrayType extends TupleType {
	ArrayDef arrayDef;

	public static class ArrayDef extends TypeDef {
		TupleCoding encoding;
		int length;

		public ArrayDef(UserpType elemType) {
			this(TupleCoding.INHERIT, elemType, VARIABLE_LEN);
		}

		public ArrayDef(UserpType elemType, int length) {
			this(TupleCoding.INHERIT, elemType, length);
		}

		public ArrayDef(TupleCoding encoding, UserpType elemType, int length) {
			this.encoding= encoding;
			this.typeRefs= new UserpType[] { elemType };
			if (length < 0 && length != VARIABLE_LEN)
				throw new RuntimeException("'length' parameter must be non-negative, or the value ArrayDef.VARIABLE_LEN");
			this.length= length;
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

		public TupleCoding getEncodingMode() {
			return encoding;
		}

		public boolean isScalar() {
			return false;
		}

		public boolean hasScalarComponent() {
			return length == VARIABLE_LEN;
		}

		public BigInteger getScalarRange() {
			if (length == VARIABLE_LEN)
				return INFINITE;
			else
				throw new UnsupportedOperationException();
		}

		public int getScalarBitLen() {
			if (length == VARIABLE_LEN)
				return VARIABLE_LEN;
			else
				throw new UnsupportedOperationException();
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
