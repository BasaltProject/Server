// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.util.debug;
import io.netty.buffer.ByteBuf;
import java.util.List;
import java.util.Set;
import net.minecraft.core.BlockPos;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.server.level.ServerLevel;
import net.minecraft.world.entity.LivingEntity;
public record DebugBrainDump(String name, String profession, int xp, float health, float maxHealth, String inventory, boolean wantsGolem, int angerLevel, List<String> activities, List<String> behaviors, List<String> memories, List<String> gossips, Set<BlockPos> pois, Set<BlockPos> potentialPois) {
    public static final StreamCodec<ByteBuf, DebugBrainDump> STREAM_CODEC = org.basaltmc.bukkit.facade.LazyCodecs.streamCodec(net.minecraft.util.debug.DebugBrainDump.class, "STREAM_CODEC");
    public static DebugBrainDump takeBrainDump(ServerLevel serverLevel, LivingEntity entity) {
        return new DebugBrainDump(String.valueOf(entity.getName().getString()), "", 0, entity.getHealth(), entity.getMaxHealth(), "", false, 0,
            java.util.List.of(), java.util.List.of(), java.util.List.of(), java.util.List.of(), java.util.Set.of(), java.util.Set.of());
    }
    public boolean hasPoi(BlockPos poiPos) {
        return pois.contains(poiPos);
    }
    public boolean hasPotentialPoi(BlockPos poiPos) {
        return potentialPois.contains(poiPos);
    }
}
