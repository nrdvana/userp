package com.silverdirk.userp;

public class ValRegister {
	Object obj;
	long i64;
	StoType sto;

	public enum StoType {
		BOOL(boolean.class), BYTE(byte.class), SHORT(short.class), INT(int.class),
		LONG(long.class), FLOAT(float.class), DOUBLE(double.class), CHAR(char.class),
		OBJECT(Object.class);

		public final Class javaCls;
		private StoType(Class javaCls) {
			this.javaCls= javaCls;
		}
	};

	static final StoType[] nativeTypeByteLenMap= new ValRegister.StoType[] {
		ValRegister.StoType.BYTE, ValRegister.StoType.SHORT,
		ValRegister.StoType.INT,  ValRegister.StoType.INT,
		ValRegister.StoType.LONG, ValRegister.StoType.LONG,
		ValRegister.StoType.LONG, ValRegister.StoType.LONG,
	};
	public static ValRegister.StoType getTypeForBitLen(int bits) {
		return nativeTypeByteLenMap[bits>>3];
	}
}
