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
public interface CodecOverride {
	public void writeValue(CodecImpl superClass, UserpWriter writer, ValRegister.StoType type, long val_i64, Object val_o) throws IOException;
	public void readValue(CodecImpl superClass, UserpReader reader, ValRegister val) throws IOException;
}
