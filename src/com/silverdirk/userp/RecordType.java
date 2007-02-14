package com.silverdirk.userp;

import java.math.BigInteger;
import java.io.IOException;

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
	RecordDef recordDef;

	public static class RecordDef extends TupleDef {
		TupleCoding encoding;
		String[] fieldNames;

		int scalarBitLenCache;
		BigInteger scalarRangeCache;

		public RecordDef(String[] fieldNames, UserpType[] fieldTypes) {
			this(TupleCoding.TIGHT, fieldNames, fieldTypes);
		}

		public RecordDef(TupleCoding encoding, String[] fieldNames, UserpType[] fieldTypes) {
			this.encoding= encoding;
			if (fieldTypes.length == 0)
				throw new RuntimeException("Records must have at least one field");
			this.typeRefs= (UserpType[]) Util.arrayClone(fieldTypes);
			if (fieldNames.length != fieldTypes.length)
				throw new RuntimeException("Number of field names does not match number of field types");
			this.fieldNames= (String[]) Util.arrayClone(fieldNames);

			scalarBitLen= (encoding == TupleCoding.BITPACK || encoding == TupleCoding.TIGHT)?
				UNRESOLVED : NONSCALAR;
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

	public String getFieldName(int idx) {
		return recordDef.fieldNames[idx];
	}
}
