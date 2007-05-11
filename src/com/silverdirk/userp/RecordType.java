package com.silverdirk.userp;

import java.math.BigInteger;
import java.util.Map;
import java.util.Arrays;

/**
 * <p>Project: Universal Serialization Protocol</p>
 * <p>Title: type Record</p>
 * <p>Description: Type Record allows specification of tuples with named elements.</p>
 * <p>Copyright Copyright (c) 2004-2007</p>
 *
 * @author Michael Conrad
 * @version $Revision$
 */
public class RecordType extends TupleType {
	RecordDef def;

	public static class Field {
		public final Symbol name;
		public final UserpType type;

		public Field(String name, UserpType type) {
			this(new Symbol(name), type);
		}
		public Field(Symbol name, UserpType type) {
			this.name= name;
			this.type= type;
		}
	}

	public static class RecordDef extends TypeDef {
		Symbol[] fieldNames;
		UserpType[] fieldTypes;

		public RecordDef(Field[] fields) {
			if (fields.length == 0)
				throw new RuntimeException("Records must have at least one field");
			fieldNames= new Symbol[fields.length];
			fieldTypes= new UserpType[fields.length];
			for (int i=0; i<fields.length; i++) {
				fieldNames[i]= fields[i].name;
				fieldTypes[i]= fields[i].type;
			}
		}

		RecordDef(Symbol[] fieldNames, UserpType[] fieldTypes) {
			this.fieldNames= fieldNames;
			this.fieldTypes= fieldTypes;
		}

		public int hashCode() {
			return ~Arrays.deepHashCode(fieldNames) ^ Arrays.deepHashCode(fieldTypes);
		}

		protected boolean equals(TypeDef other, Map<TypeHandle,TypeHandle> equalityMap) {
			if (!(other instanceof RecordDef))
				return false;
			RecordDef otherDef= (RecordDef) other;
			return Arrays.deepEquals(fieldNames, otherDef.fieldNames)
				&& Util.typeArrayDeepEquals(fieldTypes, otherDef.fieldTypes, equalityMap);
		}

		public String toString() {
			StringBuffer sb= new StringBuffer(24*fieldNames.length).append("Record([");
			for (int i=0; i<fieldNames.length; i++) {
				sb.append(fieldNames[i]+":"+fieldTypes[i].getName());
				if (i != fieldNames.length-1) sb.append(", ");
			}
			sb.append("])");
			return sb.toString();
		}
	}

	public RecordType(String name) {
		this(new Symbol(name));
	}

	public RecordType(Symbol name) {
		super(name);
	}

	public RecordType init(Field[] fields) {
		return init(new RecordDef(fields));
	}

	public RecordType init(RecordDef def) {
		this.def= def;
		return this;
	}

	public RecordType setEncParam(TupleCoding defTupleCoding) {
		defaultTupleCoding= defTupleCoding;
		return this;
	}

	protected UserpType cloneAs_internal(Symbol newName) {
		return new RecordType(newName).init(def);
	}

	public TypeDef getDefinition() {
		return def;
	}

	public Codec makeCodecDescriptor() {
		if (getDefinition() == null)
			throw new UninitializedTypeException(this, "getCodecDescriptor");
		return new RecordCodec(this, getEncoderParam_TupleCoding());
	}

	public int getElemCount() {
		return def.fieldTypes.length;
	}

	public UserpType getElemType(int idx) {
		return def.fieldTypes[idx];
	}

	public Symbol getFieldName(int idx) {
		return def.fieldNames[idx];
	}
}
