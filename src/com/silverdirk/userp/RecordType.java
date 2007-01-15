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
public class RecordType extends TupleType {
	RecordDef recordDef;

	public static class RecordDef extends TypeDef {
		TupleCoding encoding;
		String[] fieldNames;

		public RecordDef(String[] fieldNames, UserpType[] fieldTypes) {
			this(TupleCoding.INHERIT, fieldNames, fieldTypes);
		}

		public RecordDef(TupleCoding encoding, String[] fieldNames, UserpType[] fieldTypes) {
			this.encoding= encoding;
			if (fieldTypes.length == 0)
				throw new RuntimeException("Records must have at least one field");
			this.typeRefs= (UserpType[]) Util.arrayClone(fieldTypes);
			if (fieldNames.length != fieldTypes.length)
				throw new RuntimeException("Number of field names does not match number of field types");
			this.fieldNames= (String[]) Util.arrayClone(fieldNames);
		}

		public boolean equals(Object other) {
			if (!(other instanceof RecordDef))
				return false;
			RecordDef otherDef= (RecordDef) other;
			return encoding == otherDef.encoding
				&& Util.arrayEquals(true, fieldNames, otherDef.fieldNames)
				&& Util.arrayEquals(true, typeRefs, otherDef.typeRefs);
		}

		public String toString() {
			StringBuffer sb= new StringBuffer(64).append("Record(").append(encoding).append(", [");
			for (int i=0; i<fieldNames.length; i++) {
				sb.append(fieldNames[i]+"="+typeRefs[i].name);
				if (i != fieldNames.length-1) sb.append(", ");
			}
			sb.append("])");
			return sb.toString();
		}

		public TupleCoding getEncodingMode() {
			return encoding;
		}

		public boolean isScalar() {
			return false;
		}

		public boolean hasScalarComponent() {
			return typeRefs[0].hasScalarComponent();
		}

		public BigInteger getScalarRange() {
			if (hasScalarComponent())
				return typeRefs[0].getScalarRange();
			else
				throw new UnsupportedOperationException();
		}

		public int getScalarBitLen() {
			if (hasScalarComponent())
				return typeRefs[0].getScalarBitLen();
			else
				throw new UnsupportedOperationException();
		}

		public UserpType resolve(PartialType pt) {
			return new RecordType(pt.meta, this);
		}
	}

	public RecordType(String name, String[] fieldNames, UserpType[] fieldTypes) {
		this(nameToMeta(name), new RecordDef(fieldNames, fieldTypes));
	}

	public RecordType(String name, TupleCoding encoding, String[] fieldNames, UserpType[] fieldTypes) {
		this(nameToMeta(name), new RecordDef(encoding, fieldNames, fieldTypes));
	}

	public RecordType(Object[] meta, String[] fieldNames, UserpType[] fieldTypes) {
		this(meta, new RecordDef(fieldNames, fieldTypes));
	}

	public RecordType(Object[] meta, TupleCoding encoding, String[] fieldNames, UserpType[] fieldTypes) {
		this(meta, new RecordDef(encoding, fieldNames, fieldTypes));
	}

	protected RecordType(Object[] meta, RecordDef def) {
		super(meta, def);
		recordDef= def;
	}

	public UserpType makeSynonym(Object[] newMeta) {
		return new RecordType(newMeta, recordDef);
	}

	public TupleCoding getEncodingMode() {
		return recordDef.encoding;
	}

	public int getElemCount() {
		return recordDef.typeRefs.length;
	}

	public UserpType getElemType(int idx) {
		return recordDef.typeRefs[idx];
	}
}
