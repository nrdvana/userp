package com.silverdirk.userp;

/**
 * <p>Project: </p>
 * <p>Title: </p>
 * <p>Description: </p>
 * <p>Copyright: Copyright (c) 2004-2005</p>
 *
 * @author not attributable
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
