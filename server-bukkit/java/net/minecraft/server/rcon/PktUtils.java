// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.server.rcon;
import java.nio.charset.StandardCharsets;
public class PktUtils {
    public static final int MAX_PACKET_SIZE = 1460;
    public static final char[] HEX_CHAR = "0123456789abcdef".toCharArray();
    public static String stringFromByteArray(byte[] b, int offset, int length) {
        int last = length - 1;
        int end = Math.min(offset, last);
        while (end < last && b[end] != 0) {
            end++;
        }
        return new String(b, offset, end - offset, StandardCharsets.UTF_8);
    }
    public static int intFromByteArray(byte[] b, int offset) {
        return intFromByteArray(b, offset, b.length);
    }
    public static int intFromByteArray(byte[] b, int offset, int length) {
        if (length - offset < 4) {
            return 0;
        }
        return (b[offset + 3] << 24) | ((b[offset + 2] & 0xFF) << 16) | ((b[offset + 1] & 0xFF) << 8) | (b[offset] & 0xFF);
    }
    public static int intFromNetworkByteArray(byte[] b, int offset, int length) {
        if (length - offset < 4) {
            return 0;
        }
        return (b[offset] << 24) | ((b[offset + 1] & 0xFF) << 16) | ((b[offset + 2] & 0xFF) << 8) | (b[offset + 3] & 0xFF);
    }
    public static String toHexString(byte b) {
        return "" + HEX_CHAR[(b & 0xF0) >>> 4] + HEX_CHAR[b & 0x0F];
    }
}
