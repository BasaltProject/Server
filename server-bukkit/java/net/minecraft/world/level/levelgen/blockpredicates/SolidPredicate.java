// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.blockpredicates;
import com.mojang.serialization.MapCodec;
import net.minecraft.core.Vec3i;
import net.minecraft.world.level.block.state.BlockState;
public class SolidPredicate extends StateTestingPredicate {
    public static final MapCodec<SolidPredicate> CODEC = org.basaltmc.bukkit.facade.LazyCodecs.mapCodec(net.minecraft.world.level.levelgen.blockpredicates.SolidPredicate.class, "CODEC");
    public SolidPredicate(Vec3i offset) {
        super(offset);
    }
    protected boolean test(BlockState state) {
        return state.isSolid();
    }
    public BlockPredicateType<?> type() {
        return BlockPredicateType.SOLID;
    }
}
