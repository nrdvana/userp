package com.silverdirk.userp;

import java.util.*;

/**
 * <p>Project: Universal Serialization Protocol</p>
 * <p>Title: Typed Data</p>
 * <p>Description: This specifies a specific type for the associated data object; commonly used by unions and Type Any.</p>
 * <p>Copyright Copyright (c) 2006-2007</p>
 *
 * @author Michael Conrad
 * @version $Revision$
 */
public class TypedData implements UserpType.RecursiveAware {
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
		return equals(other, new HashMap<UserpType.TypeHandle,UserpType.TypeHandle>());
	}

	public boolean equals(Object other, Map<UserpType.TypeHandle,UserpType.TypeHandle> equalityMap) {
		return other instanceof TypedData && equals((TypedData)other, equalityMap);
	}

	public boolean equals(TypedData other, Map<UserpType.TypeHandle,UserpType.TypeHandle> equalityMap) {
		return dataType.equals(other.dataType, equalityMap)
			&& data.equals(other.data);
	}
}
