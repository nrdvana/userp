package com.silverdirk.userp;

/**
 * <p>Project: Universal Serialization Protocol</p>
 * <p>Title: Typed Data</p>
 * <p>Description: This specifies a specific type for the associated data object; commonly used by unions and Type Any.</p>
 * <p>Copyright Copyright (c) 2006-2007</p>
 *
 * @author Michael Conrad
 * @version $Revision$
 */
public class TypedData {
	UserpType dataType;
	Object data;

	public TypedData() {
	}

	public TypedData(UserpType t, Object d) {
		dataType= t;
		data= d;
	}

	public String toString() {
		return "("+dataType.getName()+": "+data+")";
	}

	public void setValue(UserpType type, Object data) {
		this.dataType= type;
		this.data= data;
	}

	public boolean equals(Object other) {
		return other instanceof TypedData && equals((TypedData)other);
	}
	public boolean equals(TypedData other) {
		return dataType == other.dataType
			&& data.equals(other.data);
	}
}
