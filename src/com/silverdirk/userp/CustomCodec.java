package com.silverdirk.userp;

public interface CustomCodec {
	public UserpType[] getHandledTypes();
	public Class[] getHandledClasses();

	public Object decode(UserpReader reader);
	public void encode(Object value, UserpWriter writer);
}
