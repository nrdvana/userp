package com.silverdirk.userp;

import com.silverdirk.userp.UserpType.TypeDef;

/**
 * <p>Project: </p>
 * <p>Title: </p>
 * <p>Description: </p>
 * <p>Copyright Copyright (c) 2004</p>
 *
 * @author not attributable
 * @version $Revision$
 */
public abstract class TupleType extends UserpType {
	TupleCoding defaultTupleCoding= null;

	public TupleType(Symbol name) {
		super(name);
	}
	public abstract int getElemCount();
	public abstract UserpType getElemType(int idx);

	public boolean hasEncoderParamDefaults() {
		return defaultTupleCoding != null;
	}

	public TupleCoding getEncoderParam_TupleCoding() {
		return defaultTupleCoding != null? defaultTupleCoding : TupleCoding.PACK;
	}
}
