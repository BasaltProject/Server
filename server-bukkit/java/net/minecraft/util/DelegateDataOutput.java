// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util;

import java.io.DataOutput;
import java.io.IOException;

public class DelegateDataOutput implements DataOutput {
    private final DataOutput parent;

    public DelegateDataOutput(DataOutput parent) {
        this.parent = parent;
    }

    public void write(int b) throws IOException {
        parent.write(b);
    }

    public void write(byte[] b) throws IOException {
        parent.write(b);
    }

    public void write(byte[] b, int off, int len) throws IOException {
        parent.write(b, off, len);
    }

    public void writeBoolean(boolean v) throws IOException {
        parent.writeBoolean(v);
    }

    public void writeByte(int v) throws IOException {
        parent.writeByte(v);
    }

    public void writeShort(int v) throws IOException {
        parent.writeShort(v);
    }

    public void writeChar(int v) throws IOException {
        parent.writeChar(v);
    }

    public void writeInt(int v) throws IOException {
        parent.writeInt(v);
    }

    public void writeLong(long v) throws IOException {
        parent.writeLong(v);
    }

    public void writeFloat(float v) throws IOException {
        parent.writeFloat(v);
    }

    public void writeDouble(double v) throws IOException {
        parent.writeDouble(v);
    }

    public void writeBytes(String s) throws IOException {
        parent.writeBytes(s);
    }

    public void writeChars(String s) throws IOException {
        parent.writeChars(s);
    }

    public void writeUTF(String s) throws IOException {
        parent.writeUTF(s);
    }
}
