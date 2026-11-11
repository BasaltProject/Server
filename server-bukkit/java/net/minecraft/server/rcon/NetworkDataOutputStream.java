// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.rcon;
import java.io.ByteArrayOutputStream;
import java.io.DataOutputStream;
import java.io.IOException;
import java.nio.charset.StandardCharsets;
public class NetworkDataOutputStream {
    private final ByteArrayOutputStream bytes;
    private final DataOutputStream out;
    public NetworkDataOutputStream(int size) {
        this.bytes = new ByteArrayOutputStream(size);
        this.out = new DataOutputStream(bytes);
    }
    public void writeBytes(byte[] data) throws IOException {
        out.write(data, 0, data.length);
    }
    public void writeString(String data) throws IOException {
        out.write(data.getBytes(StandardCharsets.UTF_8));
        out.write(0);
    }
    public void write(int data) throws IOException {
        out.write(data);
    }
    public void writeShort(short data) throws IOException {
        out.writeShort(Short.reverseBytes(data));
    }
    public void writeInt(int data) throws IOException {
        out.writeInt(Integer.reverseBytes(data));
    }
    public void writeFloat(float data) throws IOException {
        out.writeInt(Integer.reverseBytes(Float.floatToIntBits(data)));
    }
    public byte[] toByteArray() {
        return bytes.toByteArray();
    }
    public void reset() {
        bytes.reset();
    }
}
