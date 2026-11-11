// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.blockpredicates;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Vec3i;
import net.minecraft.world.level.LevelAccessor;
public class InsideWorldBoundsPredicate implements BlockPredicate {
    public static final MapCodec<InsideWorldBoundsPredicate> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.blockpredicates.InsideWorldBoundsPredicate.class, "CODEC");
    private final Vec3i offset;
    public InsideWorldBoundsPredicate(Vec3i offset) {
        this.offset = offset;
    }
    public boolean test(LevelAccessor level, BlockPos origin) {
        return !level.isOutsideBuildHeight(origin.offset(offset));
    }
    public BlockPredicateType<?> type() {
        return BlockPredicateType.INSIDE_WORLD_BOUNDS;
    }
}
