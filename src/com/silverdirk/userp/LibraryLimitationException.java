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
class LibraryLimitationException extends RuntimeException {
	public LibraryLimitationException() {
	}
	public LibraryLimitationException(String msg) {
		super(msg);
	}
	public LibraryLimitationException(String msg, Throwable cause) {
		super(msg, cause);
	}
}
