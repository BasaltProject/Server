// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.debug;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.entity.BeehiveBlockEntity;
public record DebugHiveInfo(Block type, int occupantCount, int honeyLevel, boolean sedated) {
    public static final StreamCodec<RegistryFriendlyByteBuf, DebugHiveInfo> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.util.debug.DebugHiveInfo.class, "STREAM_CODEC");
    public static DebugHiveInfo pack(BeehiveBlockEntity beehive) {
        return new DebugHiveInfo(beehive.getBlockState().getBlock(), beehive.getOccupantCount(), beehive.getHoneyLevel(), beehive.isSedated());
    }
}
