package com.silverdirk.userp;

import java.util.Map;
import java.math.BigInteger;

/**
 * <p>Project: </p>
 * <p>Title: </p>
 * <p>Description: </p>
 * <p>Copyright Copyright (c) 2004</p>
 *
 * @author not attributable
 * @version $Revision$
 */
public class IntegerType extends ScalarType {
	static class IntegerDef extends TypeDef {
		public int hashCode() {
			return 42;
		}

		public boolean equals(Object other) {
			return other == this;
		}

		public String toString() {
			return "Integer";
		}

		public static final IntegerDef INSTANCE= new IntegerDef();
	}

	public IntegerType(String name) {
		this(new Symbol(name));
	}

	public IntegerType(Symbol name) {
		super(name);
	}

	protected UserpType cloneAs_internal(Symbol newName) {
		return new IntegerType(newName);
	}

	public TypeDef getDefinition() {
		return IntegerDef.INSTANCE;
	}

	public boolean hasEncoderParamDefaults() {
		return false;
	}

	public Codec createCodec() {
		return new IntegerCodec(this);
	}

	public boolean isDoublyInfinite() {
		return true;
	}

	public BigInteger getValueCount() {
		return INF;
	}
}
