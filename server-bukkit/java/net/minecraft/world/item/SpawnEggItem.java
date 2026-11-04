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

import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.resources.Identifier;
import net.minecraft.world.entity.AgeableMob;
import net.minecraft.world.entity.Mob;
import net.minecraft.world.level.Spawner;
import net.minecraft.world.level.block.LiquidBlock;
public class SpawnEggItem extends Item {
    public SpawnEggItem(Item.Properties properties) {
        super(properties);
    }

    public InteractionResult useOn(UseOnContext context) {
        Level world = context.getLevel();
        ItemStack egg = context.getItemInHand();
        EntityType<?> kind = typeOf(egg);
        if (kind == null || !kind.canSpawn(world)) {
            return InteractionResult.FAIL;
        }
        if (!(world instanceof ServerLevel serverWorld)) {
            return InteractionResult.SUCCESS;
        }
        BlockPos clickedPos = context.getClickedPos();
        BlockState clicked = world.getBlockState(clickedPos);
        if (world.getBlockEntity(clickedPos) instanceof Spawner spawner) {
            return configureSpawner(context, serverWorld, spawner, kind, clicked);
        }
        Direction face = context.getClickedFace();
        BlockPos spawnPos = clicked.getCollisionShape(world, clickedPos).isEmpty() ? clickedPos : clickedPos.relative(face);
        boolean onTop = face == Direction.UP && !Objects.equals(clickedPos, spawnPos);
        return spawnMob(kind, context.getPlayer(), egg, serverWorld, spawnPos, true, onTop);
    }

    private static InteractionResult configureSpawner(UseOnContext context, ServerLevel world, Spawner spawner, EntityType<?> kind, BlockState clicked) {
        Player player = context.getPlayer();
        if (!world.isSpawnerBlockEnabled()) {
            if (player instanceof ServerPlayer serverPlayer) {
                serverPlayer.sendSystemMessage(Component.translatable("advMode.notEnabled.spawner"));
            }
            return InteractionResult.FAIL;
        }
        ItemStack egg = context.getItemInHand();
        BlockPos at = context.getClickedPos();
        TypedEntityData<EntityType<?>> custom = egg.get(DataComponents.ENTITY_DATA);
        boolean mayCustomise = player != null && custom != null && world.getServer().getPlayerList().isOp(player.nameAndId());
        if (mayCustomise) {
            spawner.setEntityData(custom, world.getRandom());
        } else {
            spawner.setEntityId(kind, world.getRandom());
        }
        world.sendBlockUpdated(at, clicked, clicked, 3);
        world.gameEvent(player, GameEvent.BLOCK_CHANGE, at);
        egg.shrink(1);
        return InteractionResult.SUCCESS;
    }

    private static InteractionResult spawnMob(EntityType<?> type, LivingEntity user, ItemStack itemStack, ServerLevel level, BlockPos spawnPos,
            boolean tryMoveDown, boolean movedUp) {
        if (type.spawn(level, itemStack, user, spawnPos, EntitySpawnReason.SPAWN_ITEM_USE, tryMoveDown, movedUp) == null) {
            return InteractionResult.FAIL;
        }
        itemStack.consume(1, user);
        level.gameEvent(user, GameEvent.ENTITY_PLACE, spawnPos);
        return InteractionResult.SUCCESS;
    }

    public InteractionResult use(Level level, Player player, InteractionHand hand) {
        BlockHitResult hit = getPlayerPOVHitResult(level, player, ClipContext.Fluid.SOURCE_ONLY);
        if (hit.getType() != HitResult.Type.BLOCK) {
            return InteractionResult.PASS;
        }
        ItemStack egg = player.getItemInHand(hand);
        EntityType<?> kind = typeOf(egg);
        if (kind == null || !kind.canSpawn(level)) {
            return InteractionResult.FAIL;
        }
        if (!(level instanceof ServerLevel serverWorld)) {
            return InteractionResult.SUCCESS;
        }
        BlockPos waterPos = hit.getBlockPos();
        if (!(level.getBlockState(waterPos).getBlock() instanceof LiquidBlock)) {
            return InteractionResult.PASS;
        }
        if (!level.mayInteract(player, waterPos) || !player.mayUseItemAt(waterPos, hit.getDirection(), egg)) {
            return InteractionResult.FAIL;
        }
        InteractionResult outcome = spawnMob(kind, player, egg, serverWorld, waterPos, false, false);
        if (outcome == InteractionResult.SUCCESS) {
            player.awardStat(Stats.ITEM_USED.get(this));
        }
        return outcome;
    }

    public static boolean spawnsEntity(ItemStack itemStack, EntityType<?> type) {
        return Objects.equals(getType(itemStack), type);
    }

    public static Optional<Holder<Item>> byId(EntityType<?> type) {
        return BuiltInRegistries.ITEM.componentLookup().findMatching(DataComponents.ENTITY_DATA, data -> data.type() == type).findAny();
    }

    public static EntityType<?> getType(ItemStack itemStack) {
        return typeOf(itemStack);
    }

    private static EntityType<?> typeOf(ItemStack itemStack) {
        TypedEntityData<EntityType<?>> data = itemStack.get(DataComponents.ENTITY_DATA);
        if (data != null) {
            return data.type();
        }
        String path = itemStack.getItem().itemKey().getPath();
        if (!(itemStack.getItem() instanceof SpawnEggItem) || !path.endsWith("_spawn_egg")) {
            return null;
        }
        return BuiltInRegistries.ENTITY_TYPE.getOptional(Identifier.withDefaultNamespace(path.substring(0, path.length() - 10))).orElse(null);
    }

    public static Optional<Mob> spawnOffspringFromSpawnEgg(Player player, Mob parent, EntityType<? extends Mob> type, ServerLevel level, Vec3 pos,
            ItemStack spawnEggStack) {
        if (!spawnsEntity(spawnEggStack, type)) {
            return Optional.empty();
        }
        Mob child = parent instanceof AgeableMob ageable ? ageable.getBreedOffspring(level, ageable) : (Mob) type.create(level, EntitySpawnReason.SPAWN_ITEM_USE);
        if (child == null) {
            return Optional.empty();
        }
        child.setBaby(true);
        if (!child.isBaby()) {
            return Optional.empty();
        }
        child.snapTo(pos.x(), pos.y(), pos.z(), 0.0F, 0.0F);
        child.applyComponentsFromItemStack(spawnEggStack);
        level.addFreshEntityWithPassengers(child);
        spawnEggStack.consume(1, player);
        return Optional.of(child);
    }

    @Override
    public boolean shouldPrintOpWarning(ItemStack stack, Player player) {
        if (player == null || !player.permissions().hasPermission(net.minecraft.server.permissions.Permissions.COMMANDS_GAMEMASTER)) {
            return false;
        }
        TypedEntityData<EntityType<?>> data = stack.get(DataComponents.ENTITY_DATA);
        return data != null && data.type().onlyOpCanSetNbt();
    }
}
