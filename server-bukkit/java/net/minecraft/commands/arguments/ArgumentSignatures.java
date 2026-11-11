// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.commands.arguments;
import io.netty.buffer.ByteBuf;
import java.util.List;
import net.minecraft.network.chat.MessageSignature;
import net.minecraft.network.chat.SignableCommand;
import net.minecraft.network.codec.StreamCodec;
public record ArgumentSignatures(List<Entry> entries) {
    public static final ArgumentSignatures EMPTY = new ArgumentSignatures(List.of());
    public static final StreamCodec<ByteBuf, ArgumentSignatures> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.commands.arguments.ArgumentSignatures.class, "STREAM_CODEC");
    public static ArgumentSignatures signCommand(SignableCommand<?> command, Signer signer) {
        List<Entry> signed = new java.util.ArrayList<>();
        for (SignableCommand.Argument<?> argument : command.arguments()) {
            MessageSignature signature = signer.sign(argument.value());
            if (signature != null) {
                signed.add(new Entry(argument.name(), signature));
            }
        }
        return new ArgumentSignatures(List.copyOf(signed));
    }
    public static record Entry(String name, MessageSignature signature) {
        public static final StreamCodec<ByteBuf, Entry> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.commands.arguments.ArgumentSignatures.Entry.class, "STREAM_CODEC");
    }
    public interface Signer {
        MessageSignature sign(String content);
    }
}
