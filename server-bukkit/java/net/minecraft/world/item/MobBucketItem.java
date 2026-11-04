// Independent reimplementation, not Mojang code. See server-bukkit/NOTICE.md
package net.minecraft.world.item;

import java.util.*;
import java.util.function.*;
import net.minecraft.core.*;
import net.minecraft.core.component.*;
import net.minecraft.network.chat.*;
import net.minecraft.server.level.*;
import net.minecraft.sounds.*;
import net.minecraft.world.*;
import net.minecraft.world.entity.*;
import net.minecraft.world.entity.player.*;
import net.minecraft.world.item.context.*;
import net.minecraft.world.level.*;
import net.minecraft.world.level.block.*;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.gameevent.GameEvent;
import net.minecraft.world.phys.*;
import net.minecraft.world.item.component.*;
import net.minecraft.world.entity.projectile.*;
import net.minecraft.world.entity.projectile.throwableitemprojectile.*;
import net.minecraft.world.entity.projectile.hurtingprojectile.*;
import net.minecraft.world.entity.projectile.arrow.*;
import net.minecraft.world.level.block.state.properties.BlockStateProperties;
import net.minecraft.advancements.triggers.CriteriaTriggers;
import net.minecraft.stats.Stats;
import net.minecraft.util.Mth;
import net.minecraft.util.RandomSource;
import net.minecraft.world.item.enchantment.EnchantmentHelper;
import net.minecraft.world.level.block.entity.SignBlockEntity;
import net.minecraft.world.level.block.entity.SignTextSlot;
import net.minecraft.tags.*;

import net.minecraft.world.level.material.*;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.entity.Bucketable;
public class MobBucketItem extends BucketItem {
    private final EntityType<? extends Mob> type;
    private final SoundEvent emptySound;

    public MobBucketItem(EntityType<? extends Mob> type, Fluid content, SoundEvent emptySound, Item.Properties properties) {
        super(content, properties);
        this.type = type;
        this.emptySound = emptySound;
    }

    @SuppressWarnings("unchecked")
    private EntityType<? extends Mob> mobType() {
        return type != null ? type : (EntityType<? extends Mob>) ItemClasses.entityType(this);
    }

    @Override
    public void checkExtraContent(LivingEntity user, Level level, ItemStack itemStack, BlockPos pos) {
        if (level instanceof ServerLevel serverLevel) {
            spawn(serverLevel, itemStack, pos);
            level.gameEvent(user, GameEvent.ENTITY_PLACE, pos);
        }
    }

    @Override
    protected void playEmptySound(LivingEntity user, LevelAccessor level, BlockPos pos) {
        level.playSound(user, pos, emptySound, SoundSource.NEUTRAL, 1.0F, 1.0F);
    }

    private void spawn(ServerLevel level, ItemStack itemStack, BlockPos spawnPos) {
        Mob mob = mobType().create(level, EntityType.createDefaultStackConfig(level, itemStack, null), spawnPos, EntitySpawnReason.BUCKET, true, false);
        if (mob instanceof Bucketable bucketable) {
            CustomData data = itemStack.getOrDefault(DataComponents.BUCKET_ENTITY_DATA, CustomData.EMPTY);
            bucketable.loadFromBucketTag(data.copyTag());
            bucketable.setFromBucket(true);
        }
        if (mob != null) {
            level.addFreshEntityWithPassengers(mob);
            mob.playAmbientSound();
        }
    }

    @Override
    public boolean emptyContents(LivingEntity user, Level level, BlockPos pos, BlockHitResult hitResult) {
        if (content == Fluids.EMPTY) {
            playEmptySound(user, level, pos);
            return true;
        }
        return super.emptyContents(user, level, pos, hitResult);
    }

    @Override
    public ClipContext.Fluid getFluidContext() {
        return ClipContext.Fluid.NONE;
    }
}
