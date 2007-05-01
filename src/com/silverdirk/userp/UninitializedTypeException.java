package com.silverdirk.userp;

/**
 * <p>Project: </p>
 * <p>Title: </p>
 * <p>Description: </p>
 * <p>Copyright Copyright (c) 2004</p>
 *
 * @author not attributable
 * @version $Revision$
 */
public class UninitializedTypeException extends IllegalStateException {
	public UninitializedTypeException(UserpType subject, String method) {
		super(subject.getName().toString()+"."+method+"(): Type not initialized.  "
			+"Some operations cannot be performed on a type until its definition is initialized with a call to \"init(<params>)\"");
	}
}
