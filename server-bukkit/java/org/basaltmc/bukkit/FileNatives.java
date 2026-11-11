package org.basaltmc.bukkit;

public final class FileNatives {
    private FileNatives() {}

    /** Kind (0 file, 1 directory, 2 other), size and modified millis; null when the path does not exist. */
    public static native long[] stat(String path);

    /** Entries under a directory as "f:", "d:" or "o:" and the relative path, parents first. */
    public static native String[] walk(String directory);

    public static native String[] list(String directory);

    public static native byte[] read(String path);

    /** Atomic: written to a sibling file and renamed. */
    public static native boolean write(String path, byte[] bytes);

    public static native boolean makeDirectories(String path);

    public static native boolean copy(String from, String to);

    public static native boolean link(String from, String to);

    public static native boolean move(String from, String to, boolean atomic);

    public static native boolean remove(String path);

    public static native boolean removeTree(String path);

    /** Bit 0 atomic move, bit 1 hard links. */
    public static native int capabilities(String directory);
}
