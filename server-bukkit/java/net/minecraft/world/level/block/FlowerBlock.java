// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.level.block;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.world.effect.MobEffect;
import net.minecraft.world.effect.MobEffectInstance;
import net.minecraft.world.item.component.SuspiciousStewEffects;
import net.minecraft.world.level.BlockGetter;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.phys.shapes.CollisionContext;
import net.minecraft.world.phys.shapes.VoxelShape;
public class FlowerBlock extends VegetationBlock implements SuspiciousEffectHolder {
    protected FlowerBlock(int index) {
        super(index);
    }
    private SuspiciousStewEffects stewEffects = SuspiciousStewEffects.EMPTY;
    public FlowerBlock(Holder<MobEffect> suspiciousStewEffect, float effectSeconds, BlockBehaviour.Properties properties) {
        this(makeEffectList(suspiciousStewEffect, effectSeconds), properties);
    }
    public FlowerBlock(SuspiciousStewEffects suspiciousStewEffects, BlockBehaviour.Properties properties) {
        super((net.minecraft.world.level.block.state.BlockBehaviour.Properties) null);
        this.stewEffects = suspiciousStewEffects;
    }
    protected static SuspiciousStewEffects makeEffectList(Holder<MobEffect> suspiciousStewEffect, float effectSeconds) {
        return new SuspiciousStewEffects(java.util.List.of(new SuspiciousStewEffects.Entry(suspiciousStewEffect, net.minecraft.util.Mth.floor(effectSeconds * 20.0F))));
    }
    protected VoxelShape getShape(BlockState state, BlockGetter level, BlockPos pos, CollisionContext context) {
        return super.getShape(state, level, pos, context);
    }
    public SuspiciousStewEffects getSuspiciousEffects() {
        return stewEffects;
    }
    public MobEffectInstance getBeeInteractionEffect() {
        return null;
    }
}
