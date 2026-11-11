// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.levelgen.blockpredicates;
import com.mojang.serialization.MapCodec;
import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.world.level.LevelAccessor;
public class AnyOfPredicate extends CombiningPredicate {
    public static final MapCodec<AnyOfPredicate> CODEC = CombiningPredicate.codec(AnyOfPredicate::new);
    public AnyOfPredicate(List<BlockPredicate> predicates) {
        super(predicates);
    }
    public boolean test(LevelAccessor level, BlockPos origin) {
        for (BlockPredicate predicate : predicates) {
            if (predicate.test(level, origin)) {
                return true;
            }
        }
        return false;
    }
    public BlockPredicateType<?> type() {
        return BlockPredicateType.ANY_OF;
    }
}
