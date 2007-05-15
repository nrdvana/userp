package com.silverdirk.userp;

import com.silverdirk.userp.UserpType.TypeDef;
import java.util.Map;

/**
 * <p>Project: </p>
 * <p>Title: </p>
 * <p>Description: </p>
 * <p>Copyright Copyright (c) 2004</p>
 *
 * @author not attributable
 * @version $Revision$
 */
public class AnyType extends UserpType {
	public static class TAnyDef extends UserpType.TypeDef {
		private TAnyDef() {}

		public int hashCode() {
			return ~42;
		}

		public boolean equals(Object other) {
			return other == this;
		}

		public String toString() {
			return "<union of all types>";
		}

		public static final TAnyDef INSTANCE= new TAnyDef();
	}

	private AnyType() {
		super(new Symbol("TAny"));
	}

	/**
	 * cloneAs
	 *
	 * @param newName Symbol
	 * @return UserpType
	 */
	protected UserpType cloneAs_internal(Symbol newName) {
		throw new UnsupportedOperationException("Cannot clone type TAny");
	}

	/**
	 *
	 * @return TypeDef
	 */
	public TypeDef getDefinition() {
		return TAnyDef.INSTANCE;
	}

	/**
	 *
	 * @return boolean
	 */
	public boolean hasEncoderParamDefaults() {
		return false;
	}

	public Codec makeCodecDescriptor() {
		return TAnyCodec.INSTANCE;
	}

	public static final AnyType INSTANCE= new AnyType();
}
