// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.gameevent;
import com.mojang.serialization.MapCodec;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
public interface PositionSourceType<T extends PositionSource> {
    PositionSourceType<BlockPositionSource> BLOCK = new BlockPositionSource.Type();
    PositionSourceType<EntityPositionSource> ENTITY = new EntityPositionSource.Type();
    MapCodec<T> codec();
    StreamCodec<? super RegistryFriendlyByteBuf, T> streamCodec();
    static <S extends PositionSourceType<T>, T extends PositionSource> S register(String name, S serializer) {
        throw org.basaltmc.bukkit.facade.NmsStub.unsupported("net.minecraft.world.level.gameevent.PositionSourceType#register");
    }
}
