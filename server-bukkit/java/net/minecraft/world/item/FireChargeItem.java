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

public class FireChargeItem extends Item implements ProjectileItem {
    public FireChargeItem(Item.Properties properties) {
        super(properties);
    }

    @Override
    public InteractionResult useOn(UseOnContext context) {
        Level level = context.getLevel();
        BlockPos pos = context.getClickedPos();
        BlockState clicked = level.getBlockState(pos);
        boolean lit = false;
        if (CampfireBlock.canLight(clicked) || CandleBlock.canLight(clicked) || CandleCakeBlock.canLight(clicked)) {
            playSound(level, pos);
            level.setBlockAndUpdate(pos, clicked.setValue(BlockStateProperties.LIT, true));
            level.gameEvent(context.getPlayer(), GameEvent.BLOCK_CHANGE, pos);
            lit = true;
        } else {
            BlockPos fire = pos.relative(context.getClickedFace());
            if (BaseFireBlock.canBePlacedAt(level, fire, context.getHorizontalDirection())) {
                playSound(level, fire);
                level.setBlockAndUpdate(fire, BaseFireBlock.getState(level, fire));
                level.gameEvent(context.getPlayer(), GameEvent.BLOCK_PLACE, fire);
                lit = true;
            }
        }
        if (!lit) {
            return InteractionResult.FAIL;
        }
        context.getItemInHand().shrink(1);
        return InteractionResult.SUCCESS;
    }

    private void playSound(Level level, BlockPos pos) {
        RandomSource random = level.getRandom();
        level.playSound(null, pos, SoundEvents.FIRECHARGE_USE, SoundSource.BLOCKS, 1.0F, (random.nextFloat() - random.nextFloat()) * 0.2F + 1.0F);
    }

    @Override
    public Projectile asProjectile(Level level, Position position, ItemStack itemStack, Direction direction) {
        RandomSource random = level.getRandom();
        Vec3 heading = new Vec3(random.triangle(direction.getStepX(), 0.11485000000000001),
            random.triangle(direction.getStepY(), 0.11485000000000001), random.triangle(direction.getStepZ(), 0.11485000000000001));
        SmallFireball fireball = new SmallFireball(level, position.x(), position.y(), position.z(), heading.normalize());
        fireball.setItem(itemStack);
        return fireball;
    }

    @Override
    public void shoot(Projectile projectile, double xd, double yd, double zd, float pow, float uncertainty) {
    }

    @Override
    public ProjectileItem.DispenseConfig createDispenseConfig() {
        return ProjectileItem.DispenseConfig.builder()
            .positionFunction((source, direction) -> DispenserBlock.getDispensePosition(source, 1.0, Vec3.ZERO))
            .uncertainty(6.6666665F).power(1.0F).overrideDispenseEvent(1018).build();
    }
}
