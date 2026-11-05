// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network;

import io.netty.buffer.ByteBuf;
import io.netty.buffer.ByteBufUtil;
import io.netty.handler.codec.DecoderException;
import io.netty.handler.codec.EncoderException;
import java.nio.charset.StandardCharsets;

public class Utf8String {
    public static String read(ByteBuf input, int maxLength) {
        int maxBytes = ByteBufUtil.utf8MaxBytes(maxLength);
        int length = VarInt.read(input);
        if (length < 0) {
            throw new DecoderException("Negative string length " + length);
        }
        if (length > maxBytes) {
            throw new DecoderException("String of " + length + " bytes exceeds the limit of " + maxBytes);
        }
        if (length > input.readableBytes()) {
            throw new DecoderException("String of " + length + " bytes but only " + input.readableBytes() + " are available");
        }
        String text = input.toString(input.readerIndex(), length, StandardCharsets.UTF_8);
        input.skipBytes(length);
        if (text.length() > maxLength) {
            throw new DecoderException("String of " + text.length() + " characters exceeds the limit of " + maxLength);
        }
        return text;
    }

    public static void write(ByteBuf output, CharSequence value, int maxLength) {
        if (value.length() > maxLength) {
            throw new EncoderException("String of " + value.length() + " characters exceeds the limit of " + maxLength);
        }
        byte[] encoded = value.toString().getBytes(StandardCharsets.UTF_8);
        if (encoded.length > ByteBufUtil.utf8MaxBytes(maxLength)) {
            throw new EncoderException("String of " + encoded.length + " bytes exceeds the limit");
        }
        VarInt.write(output, encoded.length);
        output.writeBytes(encoded);
    }
}
