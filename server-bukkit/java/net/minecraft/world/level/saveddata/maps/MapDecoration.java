// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.saveddata.maps;
import java.util.Optional;
import net.minecraft.core.Holder;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.resources.Identifier;
public record MapDecoration(Holder<MapDecorationType> type, byte x, byte y, byte rot, Optional<Component> name) {
    public static final StreamCodec<RegistryFriendlyByteBuf, MapDecoration> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.world.level.saveddata.maps.MapDecoration.class, "STREAM_CODEC");
    public MapDecoration(Holder<MapDecorationType> type, byte x, byte y, byte rot, Optional<Component> name) {
        this.type = type;
        this.x = x;
        this.y = y;
        this.rot = (byte) (rot & 15);
        this.name = name;
    }
    public Identifier getSpriteLocation() {
        return type.value().assetId();
    }
    public boolean renderOnFrame() {
        return type.value().showOnItemFrame();
    }
}
