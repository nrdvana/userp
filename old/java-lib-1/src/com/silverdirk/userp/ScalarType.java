package com.silverdirk.userp;

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
public abstract class ScalarType extends UserpType {
	protected ScalarType(Symbol name) {
		super(name);
	}

	public abstract BigInteger getValueCount();
	public abstract boolean isDoublyInfinite();
}
