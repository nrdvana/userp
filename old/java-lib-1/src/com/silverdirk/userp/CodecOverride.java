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
	public void writeValue(UserpWriter writer, ValRegister.StoType type, long val_i64, Object val_o) throws IOException;
	public void loadValue(UserpReader reader) throws IOException;


	public static class ImplementationError extends Error {
		ImplementationError(CodecOverride co, int startDepth, int endDepth, boolean wasReadMethod) {
			super(getBaseError(co, wasReadMethod)+"starting tuple depth was "+startDepth+" but ended at "+endDepth);
		}
		ImplementationError(CodecOverride co, int elemsWritten, boolean wasReadMethod) {
			super(getBaseError(co, wasReadMethod)+elemsWritten+" elements were "+(wasReadMethod?"loaded":"written"));
		}

		static final String getBaseError(CodecOverride co, boolean wasReadMethod) {
			return "Error in implementation of "+Util.getReadableClassName(co.getClass())+"."+(wasReadMethod?"load":"write")+"Value: ";
		}
	}
}
