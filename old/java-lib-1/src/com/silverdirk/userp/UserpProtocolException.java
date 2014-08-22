package com.silverdirk.userp;

import java.io.IOException;

/**
 * <p>Project: Universal Serialization Protocol</p>
 * <p>Title: Library Limitation Exception</p>
 * <p>Description: Exception that is thrown when errors are encountered in the protocol stream.</p>
 * <p>Copyright Copyright (c) 2007</p>
 *
 * @author Michael Conrad
 * @version $Revision$
 */
class UserpProtocolException extends IOException {
	public UserpProtocolException() {
	}
	public UserpProtocolException(String msg) {
		super(msg);
	}
//	public UserpProtocolException(String msg, Throwable cause) {
//		super(msg, cause);
//	}
}
