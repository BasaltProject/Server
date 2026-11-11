// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.blockpredicates;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.core.Vec3i;
import net.minecraft.world.level.LevelAccessor;
public class HasSturdyFacePredicate implements BlockPredicate {
    public static final MapCodec<HasSturdyFacePredicate> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.blockpredicates.HasSturdyFacePredicate.class, "CODEC");
    private final Vec3i offset;
    private final Direction direction;
    public HasSturdyFacePredicate(Vec3i offset, Direction direction) {
        this.offset = offset;
        this.direction = direction;
    }
    public boolean test(LevelAccessor level, BlockPos origin) {
        BlockPos at = origin.offset(offset);
        return level.getBlockState(at).isFaceSturdy(level, at, direction);
    }
    public BlockPredicateType<?> type() {
        return BlockPredicateType.HAS_STURDY_FACE;
    }
}
