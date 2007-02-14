package com.silverdirk.userp;

import java.math.BigInteger;
import java.io.IOException;

/**
 * <p>Project: Universal Serialization Protocol</p>
 * <p>Title: Type Type</p>
 * <p>Description: The protocol type used for referring to a type.</p>
 * <p>Copyright Copyright (c) 2006-2007</p>
 *
 * @author Michael Conrad
 * @version $Revision$
 */
public class TypeType extends UserpType.ResolvedType {
	static class TypeTypeDef extends TypeDef {
		public boolean equals(Object other) {
			return other instanceof TypeTypeDef;
		}

		public boolean isScalar() {
			return false;
		}

		public boolean hasScalarComponent() {
			// It does technically, but the scalar-reading is built into readType()
			return false;
		}

		public BigInteger getScalarRange() {
			throw new UnsupportedOperationException();
		}

		public int getScalarBitLen() {
			return VARIABLE_LEN;
		}

		public UserpType resolve(PartialType pt) {
			throw new UnsupportedOperationException();
		}

		static final TypeTypeDef INSTANCE= new TypeTypeDef();
	}

	private TypeType(String name) {
		super(nameToMeta(name), TypeTypeDef.INSTANCE);
		impl= TypeImpl.INSTANCE;
	}

	public UserpType makeSynonym(Object[] newName) {
		throw new UnsupportedOperationException("Cannot make synonyms of TType");
	}

	public String toString() {
		return name;
	}

	public boolean equals(Object other) {
		return other == this;
	}

	public static final TypeType INSTANCE_TTYPE= new TypeType("TType");
}
