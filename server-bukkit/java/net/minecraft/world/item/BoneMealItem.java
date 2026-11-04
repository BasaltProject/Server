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

import net.minecraft.world.level.block.BonemealSource;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.util.ParticleUtils;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.core.particles.ParticleTypes;
public class BoneMealItem extends Item {
    public static final int GRASS_SPREAD_WIDTH = 3;
    public static final int GRASS_SPREAD_HEIGHT = 1;
    public static final int GRASS_COUNT_MULTIPLIER = 3;

    public BoneMealItem(Item.Properties properties) {
        super(properties);
    }

    @Override
    public InteractionResult useOn(UseOnContext context) {
        Level level = context.getLevel();
        BlockPos pos = context.getClickedPos();
        Direction face = context.getClickedFace();
        ItemStack stack = context.getItemInHand();
        if (growCrop(stack, level, pos)) {
            if (level.isClientSide()) {
                return InteractionResult.SUCCESS;
            }
            stack.causeUseVibration(context.getPlayer(), GameEvent.ITEM_INTERACT_FINISH);
            level.levelEvent(1505, pos, 15);
            return InteractionResult.SUCCESS_SERVER;
        }
        BlockPos beside = pos.relative(face);
        if (level.getBlockState(pos).isFaceSturdy(level, pos, face) && growWaterPlant(stack, level, beside, face)) {
            if (!level.isClientSide()) {
                stack.causeUseVibration(context.getPlayer(), GameEvent.ITEM_INTERACT_FINISH);
                level.levelEvent(1505, beside, 15);
            }
            return InteractionResult.SUCCESS;
        }
        return InteractionResult.PASS;
    }

    public static boolean growCrop(ItemStack itemStack, Level level, BlockPos pos) {
        BlockState state = level.getBlockState(pos);
        if (!(state.getBlock() instanceof BonemealableBlock plant) || !plant.isValidBonemealTarget(level, pos, state, BonemealSource.INTERACTION)) {
            return false;
        }
        if (level instanceof ServerLevel serverLevel) {
            if (plant.isBonemealSuccess(level, level.getRandom(), pos, state, BonemealSource.INTERACTION)) {
                plant.performBonemeal(serverLevel, level.getRandom(), pos, state, BonemealSource.INTERACTION);
            }
            itemStack.shrink(1);
        }
        return true;
    }

    public static boolean growWaterPlant(ItemStack itemStack, Level level, BlockPos pos, Direction clickedFace) {
        if (!level.getBlockState(pos).is(Blocks.WATER) || !level.getFluidState(pos).isFull()) {
            return false;
        }
        if (!(level instanceof ServerLevel serverLevel)) {
            return true;
        }
        RandomSource random = level.getRandom();
        for (int attempt = 0; attempt < 128; attempt++) {
            BlockPos target = pos;
            BlockState toGrow = Blocks.SEAGRASS.defaultBlockState();
            boolean blocked = false;
            for (int hop = 0; hop < attempt / 16; hop++) {
                target = target.offset(random.nextInt(3) - 1, (random.nextInt(3) - 1) * random.nextInt(3) / 2, random.nextInt(3) - 1);
                if (level.getBlockState(target).isCollisionShapeFullBlock(level, target)) {
                    blocked = true;
                    break;
                }
            }
            if (blocked) {
                continue;
            }
            Holder<Biome> biome = level.getBiome(target);
            if (biome.is(BiomeTags.PRODUCES_CORALS_FROM_BONEMEAL)) {
                if (attempt == 0 && clickedFace != null && clickedFace.getAxis().isHorizontal()) {
                    toGrow = BuiltInRegistries.BLOCK.getRandomElementOf(BlockTags.WALL_CORALS, level.getRandom())
                        .map(holder -> holder.value().defaultBlockState()).orElse(toGrow);
                    if (toGrow.hasProperty(BaseCoralWallFanBlock.FACING)) {
                        toGrow = toGrow.setValue(BaseCoralWallFanBlock.FACING, clickedFace);
                    }
                } else if (random.nextInt(4) == 0) {
                    toGrow = BuiltInRegistries.BLOCK.getRandomElementOf(BlockTags.UNDERWATER_BONEMEALS, level.getRandom())
                        .map(holder -> holder.value().defaultBlockState()).orElse(toGrow);
                }
            }
            if (toGrow.is(BlockTags.WALL_CORALS, state -> state.hasProperty(BaseCoralWallFanBlock.FACING))) {
                for (int turns = 0; !toGrow.canSurvive(level, target) && turns < 4; turns++) {
                    toGrow = toGrow.setValue(BaseCoralWallFanBlock.FACING, Direction.Plane.HORIZONTAL.getRandomDirection(random));
                }
            }
            if (!toGrow.canSurvive(level, target)) {
                continue;
            }
            BlockState there = level.getBlockState(target);
            if (there.is(Blocks.WATER) && level.getFluidState(target).isFull()) {
                level.setBlockAndUpdate(target, toGrow);
            } else if (there.is(Blocks.SEAGRASS) && ((BonemealableBlock) Blocks.SEAGRASS).isValidBonemealTarget(level, target, there,
                BonemealSource.INTERACTION) && random.nextInt(10) == 0) {
                ((BonemealableBlock) Blocks.SEAGRASS).performBonemeal(serverLevel, random, target, there, BonemealSource.INTERACTION);
            }
        }
        itemStack.shrink(1);
        return true;
    }

    public static void addGrowthParticles(LevelAccessor level, BlockPos pos, int count) {
        BlockState state = level.getBlockState(pos);
        if (state.getBlock() instanceof BonemealableBlock plant) {
            BlockPos at = plant.getParticlePos(pos);
            switch (plant.getType()) {
                case NEIGHBOR_SPREADER -> ParticleUtils.spawnParticles(level, at, count * 3, 3.0, 1.0, false, ParticleTypes.HAPPY_VILLAGER);
                case GROWER -> ParticleUtils.spawnParticleInBlock(level, at, count, ParticleTypes.HAPPY_VILLAGER);
            }
        } else if (state.is(Blocks.WATER)) {
            ParticleUtils.spawnParticles(level, pos, count * 3, 3.0, 1.0, false, ParticleTypes.HAPPY_VILLAGER);
        }
    }
}
