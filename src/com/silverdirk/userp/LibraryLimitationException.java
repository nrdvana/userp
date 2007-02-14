package com.silverdirk.userp;

/**
 * <p>Project: Universal Serialization Protocol</p>
 * <p>Title: Library Limitation Exception</p>
 * <p>Description: Exception that is thrown when values encountered in the data stream exceed the limitations of the library.</p>
 * <p>Copyright Copyright (c) 2007</p>
 *
 * @author Michael Conrad
 * @version $Revision$
 */
class LibraryLimitationException extends RuntimeException {
	public LibraryLimitationException() {
	}
	public LibraryLimitationException(Object quantity, int maxBits) {
		this("Quantity from stream ("+quantity+") exceeds implementation limit of "+maxBits+" bits");
	}

	public LibraryLimitationException(String msg) {
		super(msg);
	}
	public LibraryLimitationException(String msg, Throwable cause) {
		super(msg, cause);
	}
}
