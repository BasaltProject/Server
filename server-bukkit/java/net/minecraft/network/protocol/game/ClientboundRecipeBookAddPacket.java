// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.network.protocol.game;
import net.minecraft.network.codec.ByteBufCodecs;
import java.util.List;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.Packet;
import net.minecraft.network.protocol.PacketType;
import net.minecraft.world.item.crafting.display.RecipeDisplayEntry;
public record ClientboundRecipeBookAddPacket(List<Entry> entries, boolean replace) implements Packet<ClientGamePacketListener> {
    public static final StreamCodec<RegistryFriendlyByteBuf, ClientboundRecipeBookAddPacket> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundRecipeBookAddPacket.STREAM_CODEC", () -> StreamCodec.composite(ClientboundRecipeBookAddPacket.Entry.STREAM_CODEC.apply(ByteBufCodecs.list()), ClientboundRecipeBookAddPacket::entries, ByteBufCodecs.BOOL, ClientboundRecipeBookAddPacket::replace, ClientboundRecipeBookAddPacket::new));
    public PacketType<ClientboundRecipeBookAddPacket> type() {
        return GamePacketTypes.CLIENTBOUND_RECIPE_BOOK_ADD;
    }
    public void handle(ClientGamePacketListener listener) {
        listener.handleRecipeBookAdd(this);
    }
    public static record Entry(RecipeDisplayEntry contents, byte flags) {
        public static final byte FLAG_NOTIFICATION = (byte) 1;
        public static final byte FLAG_HIGHLIGHT = (byte) 2;
        public static final StreamCodec<RegistryFriendlyByteBuf, Entry> STREAM_CODEC = org.basaltmc.bukkit.net.GuardedCodecs.guard("ClientboundRecipeBookAddPacket.Entry.STREAM_CODEC", () -> StreamCodec.composite(RecipeDisplayEntry.STREAM_CODEC, Entry::contents, ByteBufCodecs.BYTE, Entry::flags, Entry::new));
        public Entry(RecipeDisplayEntry contents, boolean notification, boolean highlight) {
            this(contents, (byte) ((notification ? FLAG_NOTIFICATION : 0) | (highlight ? FLAG_HIGHLIGHT : 0)));
        }
        public boolean notification() {
            return (flags & FLAG_NOTIFICATION) != 0;
        }
        public boolean highlight() {
            return (flags & FLAG_HIGHLIGHT) != 0;
        }
    }
}
