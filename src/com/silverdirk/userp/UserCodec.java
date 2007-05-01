package com.silverdirk.userp;

import java.io.IOException;

/**
 * <p>Project: </p>
 * <p>Title: </p>
 * <p>Description: </p>
 * <p>Copyright Copyright (c) 2004</p>
 *
 * @author not attributable
 * @version $Revision$
 */
public interface UserCodec {
	public void encode(UserpWriter dest) throws IOException;
	public int decode(UserpReader src) throws IOException;
}
