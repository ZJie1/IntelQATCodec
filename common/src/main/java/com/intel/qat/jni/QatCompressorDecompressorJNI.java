package com.intel.qat.jni;

import com.intel.qat.util.NativeCodeLoader;

public class QatCompressorDecompressorJNI {

    public static native int compressBytesDirect();

    public static native int decompressBytesDirect();

    public static native Object nativeAllocateBB(long capacity, boolean numa,
                                                 boolean forcePinned);

    public native static void initIDs(int level);

    public native static void initIDsNoLevel();

    public native static String getLibraryName();
}
