package com.silverdirk.userp;

import java.math.BigInteger;
import java.util.Map;
import java.util.Arrays;

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
	ArrayDef def;

	public static class ArrayDef extends TypeDef {
		UserpType elemType;
		int length;

		public ArrayDef(UserpType elemType) {
			this(elemType, VARIABLE_LEN);
		}

		public ArrayDef(UserpType elemType, int length) {
			this.elemType= elemType;
			if (length < 0 && length != VARIABLE_LEN)
				throw new IllegalArgumentException("'length' parameter must be non-negative, or the value ArrayDef.VARIABLE_LEN");
			this.length= length;
		}

		public int hashCode() {
			return ~length ^ elemType.hashCode();
		}

		public boolean equals(Object other) {
			if (!super.equals(other))
				return false;
			ArrayDef otherDef= (ArrayDef) other;
			return length == otherDef.length
				&& elemType.equals(otherDef.elemType);
		}

		public String toString() {
			return "Array("+elemType+", "+(length==VARIABLE_LEN? "VARIABLE_LEN" : ""+length)+")";
		}

		public static final int VARIABLE_LEN= -1;
	}

	public static final int VARIABLE_LEN= ArrayDef.VARIABLE_LEN;

	public ArrayType(String name) {
		this(new Symbol(name));
	}

	public ArrayType(Symbol name) {
		super(name);
	}

	public ArrayType init(UserpType elemType) {
		return init(new ArrayDef(elemType, VARIABLE_LEN));
	}

	public ArrayType init(UserpType elemType, int length) {
		return init(new ArrayDef(elemType, length));
	}

	public ArrayType init(ArrayDef def) {
		this.def= def;
		return this;
	}

	public ArrayType setEncParam(TupleCoding defTupleCoding) {
		defaultTupleCoding= defTupleCoding;
		return this;
	}

	protected UserpType cloneAs_internal(Symbol newName) {
		return new ArrayType(newName).init(def);
	}

	public TypeDef getDefinition() {
		return def;
	}

	public Codec makeCodecDescriptor() {
		if (getDefinition() == null)
			throw new UninitializedTypeException(this, "getCodecDescriptor");
		return new ArrayCodec(this, getEncoderParam_TupleCoding());
	}

	public int getElemCount() {
		return def.length;
	}

	public UserpType getElemType(int idx) {
		return def.elemType;
	}
}
