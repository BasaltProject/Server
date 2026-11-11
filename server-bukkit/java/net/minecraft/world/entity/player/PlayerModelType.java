// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.entity.player;
import com.mojang.serialization.Codec;
import io.netty.buffer.ByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.util.StringRepresentable;
import java.util.function.Function;
import net.minecraft.network.codec.ByteBufCodecs;
public enum PlayerModelType implements StringRepresentable {
    SLIM("slim", "slim"), WIDE("wide", "default");
    public static final Codec<PlayerModelType> CODEC = StringRepresentable.fromEnum(PlayerModelType::values);
    private static final Function<String, PlayerModelType> kByLegacyName = StringRepresentable.createNameLookup(values(), type -> type.legacyServicesId);
    public static final StreamCodec<ByteBuf, PlayerModelType> STREAM_CODEC = ByteBufCodecs.BOOL.map(slim -> slim ? SLIM : WIDE, type -> type == SLIM);
    private final String id;
    private final String legacyServicesId;
    PlayerModelType(String id, String legacyServicesId) {
        this.id = id;
        this.legacyServicesId = legacyServicesId;
    }
    public static PlayerModelType byLegacyServicesName(String name) {
        PlayerModelType found = kByLegacyName.apply(name);
        return found == null ? WIDE : found;
    }
    @Override
    public String getSerializedName() {
        return id;
    }
}
